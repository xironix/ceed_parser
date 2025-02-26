/**
 * @file seed_parser.c
 * @brief Implementation of the seed phrase parser
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mnemonic.h"
#include "seed_parser.h"
#include "wallet.h"

/**
 * @brief Maximum size of a file chunk to read at once (1MB)
 */
#define DEFAULT_CHUNK_SIZE (1024 * 1024)

/**
 * @brief Maximum word length supported
 */
#define MAX_WORD_LENGTH 32

/**
 * @brief Size of the working buffer for processing text
 */
#define BUFFER_SIZE (4 * 1024 * 1024)

/**
 * @brief Maximum length of a file path
 */
#define MAX_PATH_LENGTH 1024

/**
 * @brief Maximum number of words in a sliding window
 */
#define MAX_WINDOW_SIZE 32

/**
 * @brief Default number of extra words allowed in a phrase
 */
#define DEFAULT_MAX_EXWORDS 2

/**
 * @brief Default batch size for database operations
 */
#define DEFAULT_DB_BATCH_SIZE 1000

/**
 * @brief File extensions to skip
 */
static const char *BAD_EXTENSIONS[] = {".jpg", ".png", ".jpeg", ".ico", ".gif",
                                       ".iso", ".dll", ".sys",  ".zip", ".rar",
                                       ".7z",  ".cab", ".dat",  NULL};

/**
 * @brief Directory names to skip
 */
static const char *BAD_DIRS[] = {"System Volume Information",
                                 "$RECYCLE.BIN",
                                 "Windows",
                                 "Program Files",
                                 "Program Files (x86)",
                                 NULL};

/**
 * @brief File names to skip
 */
static const char *BAD_FILES[] = {"ntuser.dat", "pagefile.sys", "hiberfil.sys",
                                  NULL};

/**
 * @brief Standard BIP-39 word chain sizes
 */
static const size_t STANDARD_WORD_CHAIN_SIZES[] = {12, 15, 18, 21, 24, 25, 0};

/**
 * @brief Internal database controller
 */
typedef struct {
  sqlite3 *db;
  pthread_mutex_t lock;
  char **batch;
  size_t batch_count;
  size_t batch_size;
  bool in_memory;
} DBController;

/**
 * @brief Task for worker threads
 */
typedef struct {
  char path[MAX_PATH_LENGTH];
} Task;

/**
 * @brief Shared state for the parser
 */
typedef struct {
  const SeedParserConfig *config;
  struct MnemonicContext *mnemonic_ctx;
  DBController *db;
  SeedParserStats stats;
  pthread_mutex_t stats_lock;

  /* Thread pool and task queue */
  pthread_t *threads;
  int num_threads;
  Task *task_queue;
  size_t queue_size;
  size_t queue_head;
  size_t queue_tail;
  size_t queue_count;
  pthread_mutex_t queue_lock;
  pthread_cond_t queue_not_empty;
  pthread_cond_t queue_not_full;

  /* File handles for output */
  FILE *seed_log;
  FILE *addr_log;
  FILE *full_log;
  FILE *eth_addr_log;
  FILE *eth_key_log;
  FILE *monero_log;

  /* Control flags */
  volatile bool running;
  volatile bool graceful_shutdown;
} SeedParser;

/**
 * @brief Global parser state
 */
static SeedParser g_parser;

/**
 * @brief Define default excluded words
 */
static const char *DEFAULT_EXCLUDED_WORDS[] = {
    "a",  "an", "the", "and", "but", "or",   "for",  "nor",   "so",  "yet",
    "to", "of", "in",  "on",  "at",  "by",   "up",   "as",    "is",  "if",
    "it", "be", "he",  "she", "we",  "they", "them", "their", "our", "your"};
static const size_t DEFAULT_EXCLUDED_WORDS_COUNT =
    sizeof(DEFAULT_EXCLUDED_WORDS) / sizeof(DEFAULT_EXCLUDED_WORDS[0]);

/**
 * @brief Initialize the database controller
 */
static DBController *db_init(const SeedParserConfig *config) {
  DBController *db = (DBController *)malloc(sizeof(DBController));
  if (!db) {
    return NULL;
  }

  memset(db, 0, sizeof(DBController));
  db->in_memory =
      (config->db_path == NULL || strcmp(config->db_path, ":memory:") == 0);
  db->batch_size = DEFAULT_DB_BATCH_SIZE;

  pthread_mutex_init(&db->lock, NULL);

  int result;
  if (db->in_memory) {
    result = sqlite3_open(":memory:", &db->db);
  } else {
    result = sqlite3_open(config->db_path, &db->db);
  }

  if (result != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db->db));
    free(db);
    return NULL;
  }

  /* Performance optimizations for SQLite */
  sqlite3_exec(db->db, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
  sqlite3_exec(db->db, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
  sqlite3_exec(db->db, "PRAGMA temp_store=MEMORY", NULL, NULL, NULL);
  sqlite3_exec(db->db, "PRAGMA cache_size=10000", NULL, NULL, NULL);

  /* Create tables */
  const char *create_phrases_table = "CREATE TABLE IF NOT EXISTS phrases ("
                                     "  phrase TEXT PRIMARY KEY, "
                                     "  type INTEGER, "
                                     "  language INTEGER, "
                                     "  timestamp INTEGER"
                                     ")";

  if (sqlite3_exec(db->db, create_phrases_table, NULL, NULL, NULL) !=
      SQLITE_OK) {
    fprintf(stderr, "Failed to create phrases table: %s\n",
            sqlite3_errmsg(db->db));
    sqlite3_close(db->db);
    free(db);
    return NULL;
  }

  /* Create index */
  const char *create_index =
      "CREATE INDEX IF NOT EXISTS idx_phrases_timestamp ON phrases(timestamp)";
  sqlite3_exec(db->db, create_index, NULL, NULL, NULL);

  /* Allocate batch buffer */
  db->batch = (char **)malloc(db->batch_size * sizeof(char *));
  if (!db->batch) {
    sqlite3_close(db->db);
    free(db);
    return NULL;
  }

  db->batch_count = 0;

  return db;
}

/**
 * @brief Clean up the database controller
 */
static void db_cleanup(DBController *db) {
  if (!db) {
    return;
  }

  /* Free any pending batch items */
  for (size_t i = 0; i < db->batch_count; i++) {
    free(db->batch[i]);
  }

  free(db->batch);

  /* Close database */
  sqlite3_close(db->db);

  pthread_mutex_destroy(&db->lock);

  free(db);
}

/**
 * @brief Add a phrase to the database
 */
static int db_add_phrase(DBController *db, const char *phrase,
                         MnemonicType type, MnemonicLanguage language) {
  pthread_mutex_lock(&db->lock);

  /* Add phrase to batch */
  db->batch[db->batch_count] = strdup(phrase);
  if (!db->batch[db->batch_count]) {
    pthread_mutex_unlock(&db->lock);
    return -1;
  }

  db->batch_count++;

  /* Flush batch if full */
  if (db->batch_count >= db->batch_size) {
    /* Prepare SQL statement */
    sqlite3_stmt *stmt;
    const char *sql = "INSERT OR IGNORE INTO phrases (phrase, type, language, "
                      "timestamp) VALUES (?, ?, ?, ?)";

    if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
      pthread_mutex_unlock(&db->lock);
      return -1;
    }

    /* Execute batch */
    sqlite3_exec(db->db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    for (size_t i = 0; i < db->batch_count; i++) {
      sqlite3_reset(stmt);
      sqlite3_bind_text(stmt, 1, db->batch[i], -1, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 2, (int)type);
      sqlite3_bind_int(stmt, 3, (int)language);
      sqlite3_bind_int64(stmt, 4, (int64_t)time(NULL));

      if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert phrase: %s\n",
                sqlite3_errmsg(db->db));
      }

      free(db->batch[i]);
      db->batch[i] = NULL;
    }

    sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
    sqlite3_finalize(stmt);

    db->batch_count = 0;
  }

  pthread_mutex_unlock(&db->lock);
  return 0;
}

/**
 * @brief Flush pending database batches
 */
static void db_flush(DBController *db) {
  if (!db || db->batch_count == 0) {
    return;
  }

  pthread_mutex_lock(&db->lock);

  /* Prepare SQL statement */
  sqlite3_stmt *stmt;
  const char *sql = "INSERT OR IGNORE INTO phrases (phrase, type, language, "
                    "timestamp) VALUES (?, ?, ?, ?)";

  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    pthread_mutex_unlock(&db->lock);
    return;
  }

  /* Execute batch */
  sqlite3_exec(db->db, "BEGIN TRANSACTION", NULL, NULL, NULL);

  for (size_t i = 0; i < db->batch_count; i++) {
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, db->batch[i], -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, 0); /* Default type */
    sqlite3_bind_int(stmt, 3, 0); /* Default language */
    sqlite3_bind_int64(stmt, 4, (int64_t)time(NULL));

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      fprintf(stderr, "Failed to insert phrase: %s\n", sqlite3_errmsg(db->db));
    }

    free(db->batch[i]);
    db->batch[i] = NULL;
  }

  sqlite3_exec(db->db, "COMMIT", NULL, NULL, NULL);
  sqlite3_finalize(stmt);

  db->batch_count = 0;

  pthread_mutex_unlock(&db->lock);
}

/**
 * @brief Check if a phrase exists in the database
 */
static bool db_phrase_exists(DBController *db, const char *phrase) {
  bool exists = false;

  pthread_mutex_lock(&db->lock);

  sqlite3_stmt *stmt;
  const char *sql = "SELECT 1 FROM phrases WHERE phrase = ? LIMIT 1";

  if (sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    pthread_mutex_unlock(&db->lock);
    return false;
  }

  sqlite3_bind_text(stmt, 1, phrase, -1, SQLITE_TRANSIENT);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    exists = true;
  }

  sqlite3_finalize(stmt);

  pthread_mutex_unlock(&db->lock);

  return exists;
}

/**
 * @brief Update parser statistics
 */
static void update_stats(SeedParser *parser, const char *key, uint64_t value) {
  pthread_mutex_lock(&parser->stats_lock);

  if (strcmp(key, "files_processed") == 0) {
    parser->stats.files_processed += value;
  } else if (strcmp(key, "bytes_processed") == 0) {
    parser->stats.bytes_processed += value;
  } else if (strcmp(key, "phrases_found") == 0) {
    parser->stats.phrases_found += value;
  } else if (strcmp(key, "eth_keys_found") == 0) {
    parser->stats.eth_keys_found += value;
  } else if (strcmp(key, "monero_phrases_found") == 0) {
    parser->stats.monero_phrases_found += value;
  } else if (strcmp(key, "errors") == 0) {
    parser->stats.errors += value;
  }

  pthread_mutex_unlock(&parser->stats_lock);
}

/**
 * @brief Write to log file with proper locking
 */
static void write_log(FILE *file, const char *data) {
  if (!file || !data) {
    return;
  }

  /* Use low-level file operations for better performance */
  flockfile(file);
  fprintf(file, "%s\n", data);
  fflush(file);
  funlockfile(file);
}

/**
 * @brief Check if a file should be skipped based on extension
 */
static bool should_skip_extension(const char *filepath) {
  const char *ext = strrchr(filepath, '.');
  if (!ext) {
    return false;
  }

  for (size_t i = 0; BAD_EXTENSIONS[i] != NULL; i++) {
    if (strcasecmp(ext, BAD_EXTENSIONS[i]) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Check if a directory should be skipped
 */
static bool should_skip_dir(const char *dirname) {
  for (size_t i = 0; BAD_DIRS[i] != NULL; i++) {
    if (strcasecmp(dirname, BAD_DIRS[i]) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Check if a file should be skipped
 */
static bool should_skip_file(const char *filename) {
  for (size_t i = 0; BAD_FILES[i] != NULL; i++) {
    if (strcasecmp(filename, BAD_FILES[i]) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Process a valid mnemonic phrase
 */
static void process_mnemonic(SeedParser *parser, const char *mnemonic,
                             const char *source_file) {
  MnemonicType type;
  MnemonicLanguage language;

  /* Validate the mnemonic */
  if (!mnemonic_validate(parser->mnemonic_ctx, mnemonic, &type, &language)) {
    return;
  }

  /* Check if already in database to avoid duplicates */
  if (db_phrase_exists(parser->db, mnemonic)) {
    return;
  }

  /* Add to database */
  if (db_add_phrase(parser->db, mnemonic, type, language) != 0) {
    update_stats(parser, "errors", 1);
    return;
  }

  /* Update statistics based on type */
  if (type == MNEMONIC_BIP39) {
    update_stats(parser, "phrases_found", 1);
  } else if (type == MNEMONIC_MONERO) {
    update_stats(parser, "monero_phrases_found", 1);
  }

  /* Current timestamp for logs */
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

  /* Log the phrase */
  char log_entry[4096];
  snprintf(log_entry, sizeof(log_entry), "[%s] %s - Source: %s", timestamp,
           mnemonic, source_file);

  if (type == MNEMONIC_BIP39) {
    write_log(parser->seed_log, log_entry);
    write_log(parser->full_log, log_entry);

    /* Generate wallets for BIP-39 phrases */
    Wallet wallets[20];
    size_t wallet_count = 0;

    if (wallet_generate_multiple(mnemonic, wallets, 20, &wallet_count) == 0) {
      for (size_t i = 0; i < wallet_count; i++) {
        char wallet_entry[1024];
        snprintf(wallet_entry, sizeof(wallet_entry), "%s - %s", mnemonic,
                 wallets[i].addresses[0]);

        write_log(parser->addr_log, wallet_entry);

        if (wallets[i].type == WALLET_TYPE_ETHEREUM) {
          write_log(parser->eth_addr_log, wallet_entry);

          /* Also log private key for ETH if desired */
          if (parser->config->parse_eth) {
            char key_entry[1024];
            snprintf(key_entry, sizeof(key_entry), "%s - %s - %s", mnemonic,
                     wallets[i].addresses[0], wallets[i].private_keys[0]);
            write_log(parser->eth_key_log, key_entry);
          }
        }
      }
    }
  } else if (type == MNEMONIC_MONERO) {
    write_log(parser->monero_log, log_entry);
    write_log(parser->full_log, log_entry);

    /* Generate Monero wallet */
    Wallet wallet;
    if (wallet_monero_from_mnemonic(mnemonic, &wallet) == 0) {
      char wallet_entry[1024];
      snprintf(wallet_entry, sizeof(wallet_entry), "%s - %s", mnemonic,
               wallet.addresses[0]);

      write_log(parser->monero_log, wallet_entry);
    }
  }
}

/**
 * @brief Extract words from a chunk of data
 */
static char **extract_words(const char *data, size_t *word_count) {
  if (!data || !word_count) {
    return NULL;
  }

  /* Allocate an initial array for words */
  size_t capacity = 1000;
  char **words = (char **)malloc(capacity * sizeof(char *));
  if (!words) {
    return NULL;
  }

  *word_count = 0;

  /* Extract all words matching a-z pattern */
  const char *p = data;
  while (*p) {
    /* Skip non-alphabetic characters */
    while (*p && !isalpha(*p)) {
      p++;
    }

    if (!*p) {
      break;
    }

    /* Extract word if it's all lowercase */
    const char *start = p;
    while (*p && isalpha(*p) && islower(*p)) {
      p++;
    }

    /* Valid word must be all lowercase and between 3-16 characters */
    size_t len = p - start;
    if (len >= 3 && len <= MAX_WORD_LENGTH) {
      /* Reallocate if needed */
      if (*word_count >= capacity) {
        capacity *= 2;
        char **new_words = (char **)realloc(words, capacity * sizeof(char *));
        if (!new_words) {
          /* Free existing words */
          for (size_t i = 0; i < *word_count; i++) {
            free(words[i]);
          }
          free(words);
          return NULL;
        }
        words = new_words;
      }

      /* Copy the word */
      words[*word_count] = (char *)malloc(len + 1);
      if (!words[*word_count]) {
        /* Free existing words */
        for (size_t i = 0; i < *word_count; i++) {
          free(words[i]);
        }
        free(words);
        return NULL;
      }

      memcpy(words[*word_count], start, len);
      words[*word_count][len] = '\0';
      (*word_count)++;
    }
  }

  return words;
}

/**
 * @brief Free an array of words
 */
static void free_words(char **words, size_t word_count) {
  if (!words) {
    return;
  }

  for (size_t i = 0; i < word_count; i++) {
    free(words[i]);
  }

  free(words);
}

/**
 * @brief Check if word repetition is within limits
 */
static bool valid_phrase_repetition(const char **words, size_t count,
                                    int max_repetition) {
  if (max_repetition <= 0) {
    return true; /* No repetition check */
  }

  for (size_t i = 0; i < count; i++) {
    int repetitions = 0;
    for (size_t j = 0; j < count; j++) {
      if (strcmp(words[i], words[j]) == 0) {
        repetitions++;
      }
    }

    if (repetitions > max_repetition) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Identify and process possible phrases from a sliding window of words
 */
static void process_word_window(SeedParser *parser, char **word_window,
                                size_t window_size, const char *source_file) {
  if (window_size < 12) {
    return; /* Minimum phrase length is 12 words */
  }

  /* Get configured word chain sizes */
  const size_t *chain_sizes = parser->config->word_chain_sizes;
  if (!chain_sizes || chain_sizes[0] == 0) {
    chain_sizes = STANDARD_WORD_CHAIN_SIZES;
  }

  /* Try all supported chain sizes */
  for (size_t i = 0; chain_sizes[i] != 0; i++) {
    size_t size = chain_sizes[i];

    /* Skip if window is too small */
    if (window_size < size) {
      continue;
    }

    /* Try all possible phrases of this size within the window */
    for (size_t start = 0; start <= window_size - size; start++) {
      /* Check word repetition */
      if (!valid_phrase_repetition((const char **)(word_window + start), size,
                                   parser->config->max_exwords)) {
        continue;
      }

      /* Build the phrase */
      char phrase[1024] = {0};
      for (size_t j = 0; j < size; j++) {
        if (j > 0) {
          strcat(phrase, " ");
        }
        strcat(phrase, word_window[start + j]);
      }

      /* Process the mnemonic */
      process_mnemonic(parser, phrase, source_file);
    }
  }
}

/**
 * @brief Process a file looking for seed phrases
 */
static int process_file(SeedParser *parser, const char *filepath) {
  /* Make a temporary non-const copy for basename which doesn't accept const
   * char* */
  char filepath_copy[PATH_MAX];
  if (filepath) {
    strncpy(filepath_copy, filepath, PATH_MAX - 1);
    filepath_copy[PATH_MAX - 1] = '\0';
  } else {
    filepath_copy[0] = '\0';
  }

  /* Check if we should parse this file */
  if (should_skip_extension(filepath) ||
      should_skip_file(basename(filepath_copy))) {
    return 0;
  }

  /* Open the file */
  FILE *file = fopen(filepath, "rb");
  if (!file) {
    update_stats(parser, "errors", 1);
    return -1;
  }

  /* Buffer for reading the file */
  char *buffer = (char *)malloc(parser->config->chunk_size + 1);
  if (!buffer) {
    fclose(file);
    update_stats(parser, "errors", 1);
    return -1;
  }

  /* Sliding window of words */
  char *word_window[MAX_WINDOW_SIZE];
  size_t window_size = 0;

  /* Read the file in chunks */
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, parser->config->chunk_size, file)) >
         0) {
    /* Update bytes processed */
    update_stats(parser, "bytes_processed", bytes_read);

    /* Ensure buffer is null-terminated */
    buffer[bytes_read] = '\0';

    /* Try different encodings for text files */
    for (int encoding = 0; encoding < 3; encoding++) {
      /* Skip binary-looking data */
      bool looks_binary = false;
      for (size_t i = 0; i < bytes_read && i < 1000; i++) {
        if ((unsigned char)buffer[i] < 32 && !isspace(buffer[i])) {
          looks_binary = true;
          break;
        }
      }

      if (looks_binary) {
        continue;
      }

      /* Extract words from the data */
      size_t word_count = 0;
      char **words = extract_words(buffer, &word_count);

      if (words && word_count > 0) {
        /* Add new words to the sliding window */
        for (size_t i = 0; i < word_count; i++) {
          /* If window is full, shift it */
          if (window_size >= MAX_WINDOW_SIZE) {
            free(word_window[0]);
            memmove(word_window, word_window + 1,
                    (MAX_WINDOW_SIZE - 1) * sizeof(char *));
            window_size--;
          }

          /* Add new word to window */
          word_window[window_size++] = words[i];

          /* Process the window when it's large enough */
          if (window_size >= 12) {
            process_word_window(parser, word_window, window_size, filepath);
          }
        }

        /* Free the words array but not the actual words (now in window) */
        free(words);

        /* Skip other encodings if we succeeded */
        break;
      }

      /* Free words if extracted */
      if (words) {
        free_words(words, word_count);
      }
    }
  }

  /* Process any remaining window */
  if (window_size >= 12) {
    process_word_window(parser, word_window, window_size, filepath);
  }

  /* Clean up window */
  for (size_t i = 0; i < window_size; i++) {
    free(word_window[i]);
  }

  /* Clean up */
  free(buffer);
  fclose(file);

  update_stats(parser, "files_processed", 1);

  return 0;
}

/**
 * @brief Worker thread function
 */
static void *worker_thread(void *arg) {
  SeedParser *parser = (SeedParser *)arg;

  while (parser->running) {
    /* Get a task from the queue */
    Task task;
    pthread_mutex_lock(&parser->queue_lock);

    while (parser->queue_count == 0 && parser->running) {
      pthread_cond_wait(&parser->queue_not_empty, &parser->queue_lock);
    }

    if (!parser->running) {
      pthread_mutex_unlock(&parser->queue_lock);
      break;
    }

    /* Take a task from the queue */
    memcpy(&task, &parser->task_queue[parser->queue_head], sizeof(Task));
    parser->queue_head = (parser->queue_head + 1) % parser->queue_size;
    parser->queue_count--;

    /* Signal that the queue is not full */
    pthread_cond_signal(&parser->queue_not_full);

    pthread_mutex_unlock(&parser->queue_lock);

    /* Process the file */
    process_file(parser, task.path);
  }

  return NULL;
}

/**
 * @brief Scan a directory recursively
 */
static int scan_directory(SeedParser *parser, const char *dirpath) {
  DIR *dir = opendir(dirpath);
  if (!dir) {
    fprintf(stderr, "Error opening directory %s: %s\n", dirpath,
            strerror(errno));
    update_stats(parser, "errors", 1);
    return -1;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    /* Skip . and .. */
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    /* Skip directories we don't want to scan */
    if (should_skip_dir(entry->d_name)) {
      continue;
    }

    /* Build the full path */
    char path[MAX_PATH_LENGTH];
    snprintf(path, sizeof(path), "%s/%s", dirpath, entry->d_name);

    /* Check if it's a directory */
    struct stat st;
    if (stat(path, &st) != 0) {
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      /* Recursively scan the directory */
      scan_directory(parser, path);
    } else if (S_ISREG(st.st_mode)) {
      /* Queue the file for processing */
      pthread_mutex_lock(&parser->queue_lock);

      /* Wait if the queue is full */
      while (parser->queue_count == parser->queue_size && parser->running) {
        pthread_cond_wait(&parser->queue_not_full, &parser->queue_lock);
      }

      if (!parser->running) {
        pthread_mutex_unlock(&parser->queue_lock);
        break;
      }

      /* Add the task to the queue */
      Task *task = &parser->task_queue[parser->queue_tail];
      strncpy(task->path, path, sizeof(task->path) - 1);
      task->path[sizeof(task->path) - 1] = '\0';

      parser->queue_tail = (parser->queue_tail + 1) % parser->queue_size;
      parser->queue_count++;

      /* Signal that the queue is not empty */
      pthread_cond_signal(&parser->queue_not_empty);

      pthread_mutex_unlock(&parser->queue_lock);
    }

    /* Check for graceful shutdown */
    if (parser->graceful_shutdown) {
      break;
    }
  }

  closedir(dir);
  return 0;
}

/**
 * @brief Open log files
 */
static int open_log_files(SeedParser *parser) {
  /* Create log directory if it doesn't exist */
  struct stat st = {0};
  if (stat(parser->config->log_dir, &st) == -1) {
    if (mkdir(parser->config->log_dir, 0755) != 0) {
      fprintf(stderr, "Error creating log directory: %s\n", strerror(errno));
      return -1;
    }
  }

  /* Generate timestamp for log filenames */
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", tm_info);

  /* Path buffers */
  char seed_path[MAX_PATH_LENGTH];
  char addr_path[MAX_PATH_LENGTH];
  char full_path[MAX_PATH_LENGTH];
  char eth_addr_path[MAX_PATH_LENGTH];
  char eth_key_path[MAX_PATH_LENGTH];
  char monero_path[MAX_PATH_LENGTH];

  /* Build paths */
  snprintf(seed_path, sizeof(seed_path), "%s/seed-%s.txt",
           parser->config->log_dir, timestamp);
  snprintf(addr_path, sizeof(addr_path), "%s/addresses-%s.txt",
           parser->config->log_dir, timestamp);
  snprintf(full_path, sizeof(full_path), "%s/full-log-%s.txt",
           parser->config->log_dir, timestamp);
  snprintf(eth_addr_path, sizeof(eth_addr_path), "%s/eth-a-log-%s.txt",
           parser->config->log_dir, timestamp);
  snprintf(eth_key_path, sizeof(eth_key_path), "%s/eth-p-log-%s.txt",
           parser->config->log_dir, timestamp);
  snprintf(monero_path, sizeof(monero_path), "%s/monero-log-%s.txt",
           parser->config->log_dir, timestamp);

  /* Open log files */
  parser->seed_log = fopen(seed_path, "w");
  parser->addr_log = fopen(addr_path, "w");
  parser->full_log = fopen(full_path, "w");
  parser->eth_addr_log = fopen(eth_addr_path, "w");
  parser->eth_key_log = fopen(eth_key_path, "w");
  parser->monero_log = fopen(monero_path, "w");

  /* Check if all files were opened */
  if (!parser->seed_log || !parser->addr_log || !parser->full_log ||
      !parser->eth_addr_log || !parser->eth_key_log || !parser->monero_log) {
    fprintf(stderr, "Error opening log files\n");
    return -1;
  }

  /* Set secure permissions */
  chmod(seed_path, 0600);
  chmod(addr_path, 0600);
  chmod(full_path, 0600);
  chmod(eth_addr_path, 0600);
  chmod(eth_key_path, 0600);
  chmod(monero_path, 0600);

  return 0;
}

/**
 * @brief Close log files
 */
static void close_log_files(SeedParser *parser) {
  if (parser->seed_log) {
    fclose(parser->seed_log);
    parser->seed_log = NULL;
  }

  if (parser->addr_log) {
    fclose(parser->addr_log);
    parser->addr_log = NULL;
  }

  if (parser->full_log) {
    fclose(parser->full_log);
    parser->full_log = NULL;
  }

  if (parser->eth_addr_log) {
    fclose(parser->eth_addr_log);
    parser->eth_addr_log = NULL;
  }

  if (parser->eth_key_log) {
    fclose(parser->eth_key_log);
    parser->eth_key_log = NULL;
  }

  if (parser->monero_log) {
    fclose(parser->monero_log);
    parser->monero_log = NULL;
  }
}

/**
 * @brief Initialize default configuration
 */
int seed_parser_config_init(SeedParserConfig *config) {
  if (!config) {
    return -1;
  }

  /* Default values */
  memset(config, 0, sizeof(SeedParserConfig));

  config->source_dir = ".";
  config->log_dir = "./logs";
  config->db_path = ":memory:";
  config->threads = 0; /* Auto-detect */
  config->parse_eth = true;
  config->chunk_size = DEFAULT_CHUNK_SIZE;
  config->exwords = DEFAULT_EXCLUDED_WORDS;
  config->max_exwords = DEFAULT_EXCLUDED_WORDS_COUNT;
  config->wordlist_dir = "./data/wordlist";
  config->detect_monero = true;

  /* Enable English by default */
  config->languages[0] = LANGUAGE_ENGLISH;
  config->languages[1] = LANGUAGE_COUNT; /* Terminator */

  /* Default word chain sizes */
  size_t i = 0;
  for (; STANDARD_WORD_CHAIN_SIZES[i] != 0; i++) {
    config->word_chain_sizes[i] = STANDARD_WORD_CHAIN_SIZES[i];
  }
  config->word_chain_sizes[i] = 0; /* Terminator */

  return 0;
}

/**
 * @brief Initialize the seed parser with the given configuration options
 */
bool seed_parser_init(const SeedParserConfig *config) {
  if (!config) {
    fprintf(stderr, "Error: No configuration provided\n");
    return false;
  }

  /* Initialize the parser state */
  memset(&g_parser, 0, sizeof(SeedParser));
  g_parser.config = config;

  /* Set up excluded words if not provided */
  if (config->exwords == NULL) {
    /* Use the default excluded words */
    SeedParserConfig *mutable_config = (SeedParserConfig *)config;
    mutable_config->exwords = DEFAULT_EXCLUDED_WORDS;
    mutable_config->max_exwords = DEFAULT_EXCLUDED_WORDS_COUNT;
  }

  /* Initialize the mnemonic subsystem */
  g_parser.mnemonic_ctx = mnemonic_init(config->wordlist_dir);
  if (!g_parser.mnemonic_ctx) {
    fprintf(stderr, "Error: Failed to initialize mnemonic subsystem\n");
    return false;
  }

  /* Initialize thread management */
  pthread_mutex_init(&g_parser.stats_lock, NULL);
  pthread_mutex_init(&g_parser.queue_lock, NULL);
  pthread_cond_init(&g_parser.queue_not_empty, NULL);
  pthread_cond_init(&g_parser.queue_not_full, NULL);

  /* Determine the number of threads */
  g_parser.num_threads = config->threads;
  if (g_parser.num_threads <= 0) {
    g_parser.num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (g_parser.num_threads <= 0) {
      g_parser.num_threads = 4; /* Fallback */
    }
  }

  /* Clamp thread count to a reasonable range */
  if (g_parser.num_threads < 1) {
    g_parser.num_threads = 1;
  } else if (g_parser.num_threads > 64) {
    g_parser.num_threads = 64;
  }

  /* Allocate thread array */
  g_parser.threads =
      (pthread_t *)malloc(g_parser.num_threads * sizeof(pthread_t));
  if (!g_parser.threads) {
    seed_parser_cleanup();
    return false;
  }

  /* Allocate task queue */
  g_parser.queue_size = g_parser.num_threads * 100; /* 100 tasks per thread */
  g_parser.task_queue = (Task *)malloc(g_parser.queue_size * sizeof(Task));
  if (!g_parser.task_queue) {
    seed_parser_cleanup();
    return false;
  }

  /* Load enabled languages */
  for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
    if (config->languages[i] == LANGUAGE_COUNT) {
      break;
    }

    if (mnemonic_load_wordlist(g_parser.mnemonic_ctx, config->languages[i]) !=
        0) {
      fprintf(stderr, "Warning: Failed to load wordlist for language %d\n",
              config->languages[i]);
    }
  }

  /* Initialize database */
  g_parser.db = db_init(config);
  if (!g_parser.db) {
    seed_parser_cleanup();
    return false;
  }

  /* Open log files */
  if (open_log_files(&g_parser) != 0) {
    seed_parser_cleanup();
    return false;
  }

  /* Initialize wallet module */
  if (wallet_init() != 0) {
    seed_parser_cleanup();
    return false;
  }

  return true;
}

/**
 * @brief Start scanning for seed phrases
 */
int seed_parser_start(void) {
  /* Check if initialized */
  if (!g_parser.config || !g_parser.db || !g_parser.mnemonic_ctx) {
    return -1;
  }

  /* Set running flag */
  g_parser.running = true;
  g_parser.graceful_shutdown = false;

  /* Start worker threads */
  for (int i = 0; i < g_parser.num_threads; i++) {
    if (pthread_create(&g_parser.threads[i], NULL, worker_thread, &g_parser) !=
        0) {
      fprintf(stderr, "Error creating worker thread %d\n", i);
      g_parser.running = false;
      return -1;
    }
  }

  /* Set up signal handlers */
  signal(SIGINT, seed_parser_handle_signal);
  signal(SIGTERM, seed_parser_handle_signal);

  /* Scan the directory */
  scan_directory(&g_parser, g_parser.config->source_dir);

  /* Wait for all tasks to be processed */
  pthread_mutex_lock(&g_parser.queue_lock);
  while (g_parser.queue_count > 0 && !g_parser.graceful_shutdown) {
    pthread_mutex_unlock(&g_parser.queue_lock);
    usleep(100000); /* 100ms */
    pthread_mutex_lock(&g_parser.queue_lock);
  }
  pthread_mutex_unlock(&g_parser.queue_lock);

  /* Signal shutdown */
  g_parser.running = false;

  /* Wake up all waiting threads */
  pthread_mutex_lock(&g_parser.queue_lock);
  pthread_cond_broadcast(&g_parser.queue_not_empty);
  pthread_cond_broadcast(&g_parser.queue_not_full);
  pthread_mutex_unlock(&g_parser.queue_lock);

  /* Wait for threads to finish */
  for (int i = 0; i < g_parser.num_threads; i++) {
    pthread_join(g_parser.threads[i], NULL);
  }

  /* Flush database */
  db_flush(g_parser.db);

  return 0;
}

/**
 * @brief Clean up resources used by the seed parser
 */
void seed_parser_cleanup(void) {
  /* Close log files */
  close_log_files(&g_parser);

  /* Clean up database */
  if (g_parser.db) {
    db_cleanup(g_parser.db);
    g_parser.db = NULL;
  }

  /* Clean up mnemonic module */
  if (g_parser.mnemonic_ctx) {
    mnemonic_cleanup(g_parser.mnemonic_ctx);
    g_parser.mnemonic_ctx = NULL;
  }

  /* Clean up wallet module */
  wallet_cleanup();

  /* Free thread array */
  free(g_parser.threads);
  g_parser.threads = NULL;

  /* Free task queue */
  free(g_parser.task_queue);
  g_parser.task_queue = NULL;

  /* Destroy synchronization objects */
  pthread_mutex_destroy(&g_parser.stats_lock);
  pthread_mutex_destroy(&g_parser.queue_lock);
  pthread_cond_destroy(&g_parser.queue_not_empty);
  pthread_cond_destroy(&g_parser.queue_not_full);
}

/**
 * @brief Get current statistics
 */
void seed_parser_get_stats(SeedParserStats *stats) {
  if (!stats) {
    return;
  }

  pthread_mutex_lock(&g_parser.stats_lock);
  memcpy(stats, &g_parser.stats, sizeof(SeedParserStats));
  pthread_mutex_unlock(&g_parser.stats_lock);
}

/**
 * @brief Validate a mnemonic phrase
 */
bool seed_parser_validate_mnemonic(const char *mnemonic, MnemonicType *type,
                                   MnemonicLanguage *language) {
  if (!g_parser.mnemonic_ctx || !mnemonic) {
    return false;
  }

  return mnemonic_validate(g_parser.mnemonic_ctx, mnemonic, type, language);
}

/**
 * @brief Process a file looking for seed phrases
 */
int seed_parser_process_file(const char *filepath) {
  if (!g_parser.running || !filepath) {
    return -1;
  }

  return process_file(&g_parser, filepath);
}

/**
 * @brief Handle SIGINT for graceful shutdown
 */
void seed_parser_handle_signal(int signum) {
  if (signum == SIGINT || signum == SIGTERM) {
    fprintf(stderr, "\nReceived shutdown signal, cleaning up...\n");
    g_parser.graceful_shutdown = true;
  }
}

/**
 * @brief Check if the seed parser has completed its work
 */
bool seed_parser_is_complete(void) {
  pthread_mutex_lock(&g_parser.queue_lock);
  bool is_complete = (g_parser.queue_count == 0 && !g_parser.running);
  pthread_mutex_unlock(&g_parser.queue_lock);
  return is_complete;
}

/**
 * @brief Callback function for progress updates
 */
typedef void (*SeedParserProgressCallback)(const char *,
                                           const SeedParserStats *);

/**
 * @brief Global progress callback
 */
static SeedParserProgressCallback g_progress_callback = NULL;

/**
 * @brief Register a callback for progress updates
 */
void seed_parser_register_progress_callback(
    void (*callback)(const char *, const SeedParserStats *)) {
  g_progress_callback = callback;
}

/**
 * @brief Callback function for when a seed is found
 */
typedef void (*SeedParserSeedFoundCallback)(const char *, const char *,
                                            MnemonicType, MnemonicLanguage,
                                            size_t);

/**
 * @brief Global seed found callback
 */
static SeedParserSeedFoundCallback g_seed_found_callback = NULL;

/**
 * @brief Register a callback for when a seed is found
 */
void seed_parser_register_seed_found_callback(void (*callback)(
    const char *, const char *, MnemonicType, MnemonicLanguage, size_t)) {
  g_seed_found_callback = callback;
}

/**
 * @brief Stop the seed parser
 */
void seed_parser_stop(void) {
  g_parser.graceful_shutdown = true;
  g_parser.running = false;

  /* Wake up all waiting threads */
  pthread_mutex_lock(&g_parser.queue_lock);
  pthread_cond_broadcast(&g_parser.queue_not_empty);
  pthread_cond_broadcast(&g_parser.queue_not_full);
  pthread_mutex_unlock(&g_parser.queue_lock);
}

/**
 * @brief Process a line of text looking for seed phrases
 *
 * @param line The line of text to process
 * @return true on success, false on failure
 */
bool seed_parser_process_line(const char *line) {
  if (!line || !g_parser.running) {
    return false;
  }

  /* Extract words from the line */
  size_t word_count = 0;
  char **words = extract_words(line, &word_count);

  if (!words || word_count < 12) {
    /* Free words if extracted */
    if (words) {
      free_words(words, word_count);
    }
    return false;
  }

  /* Process the words as a sliding window */
  if (word_count >= 12) {
    process_word_window(&g_parser, words, word_count, "direct_input");
  }

  /* Clean up */
  free_words(words, word_count);

  return true;
}