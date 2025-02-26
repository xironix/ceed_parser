/**
 * @file main.c
 * @brief Command-line interface for the seed parser
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../include/mnemonic.h"
#include "../include/seed_parser.h"
#include "../include/seed_parser_optimized.h"
#include "../include/wallet.h"

#ifdef USE_OPTIMIZED_PARSER
#include "../include/cache.h"
#include "../include/memory_pool.h"
#include "../include/simd_utils.h"
#include "../include/thread_pool.h"
#endif

/**
 * @brief Define ENABLE_DEBUG for debug macros
 */
#define ENABLE_DEBUG 1

/**
 * @brief Default output file for found seed phrases
 */
#define DEFAULT_OUTPUT_FILE "found_seeds.txt"

/**
 * @brief Default paths to scan
 */
#define DEFAULT_SCAN_PATH "."

/**
 * @brief Default number of threads to use
 */
#define DEFAULT_THREAD_COUNT 4

/**
 * @brief Flag indicating whether the program should continue running
 */
static volatile bool g_running = true;

/**
 * @brief Flag indicating whether verbose output is enabled
 */
static bool g_verbose = false;

/**
 * @brief Flag indicating whether debug output is enabled
 */
bool g_debug_enabled = false;

/**
 * @brief Global parser configuration
 */
static SeedParserConfig g_config;

/**
 * @brief Global parser statistics
 */
static SeedParserStats g_stats;

/**
 * @brief Signal handler for graceful shutdown
 */
void handle_signal(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    printf("\nReceived termination signal. Shutting down gracefully...\n");
    g_running = false;
  }
}

/**
 * @brief Print usage information
 */
void print_usage(const char *program_name) {
  printf("Usage: %s [OPTIONS] [PATHS...]\n\n", program_name);
  printf("Options:\n");
  printf("  -h, --help                  Display this help message\n");
  printf("  -o, --output FILE           Output file for found seeds (default: "
         "%s)\n",
         DEFAULT_OUTPUT_FILE);
  printf(
      "  -t, --threads N             Number of threads to use (default: %d)\n",
      DEFAULT_THREAD_COUNT);
  printf("  -v, --verbose               Enable verbose output\n");
  printf("  -D, --debug                 Enable debug output\n");
  printf("  -m, --monero                Enable detection of Monero 25-word "
         "seed phrases\n");
  printf("  -l, --languages LANGS       Comma-separated list of languages to "
         "use for BIP-39\n");
  printf("                              (e.g., english,french,spanish)\n");
  printf("                              (default: english only)\n");
  printf("  -A, --all-languages         Enable all supported languages\n");
  printf("  -a, --addresses N           Generate N wallet addresses for each "
         "seed (default: 1)\n");
  printf("  -r, --recursive             Recursively scan directories\n");
  printf("  -f, --fast                  Fast mode (less validation, more "
         "speed)\n");
  printf("  -d, --database FILE         SQLite database file for results\n");
#ifdef USE_OPTIMIZED_PARSER
  printf("  -p, --performance           Show performance statistics\n");
  printf("  -c, --cpu-info              Show CPU and SIMD capabilities\n");
  printf("  -T, --threads NUM           Set number of threads (0 = auto)\n");
#endif
  printf("\n");
  printf("If no paths are specified, the current directory will be scanned.\n");
}

/**
 * @brief Parse command line arguments
 */
bool parse_args(int argc, char **argv) {
  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"output", required_argument, NULL, 'o'},
      {"threads", required_argument, NULL, 't'},
      {"verbose", no_argument, NULL, 'v'},
      {"debug", no_argument, NULL, 'D'},
      {"monero", no_argument, NULL, 'm'},
      {"languages", required_argument, NULL, 'l'},
      {"all-languages", no_argument, NULL, 'A'},
      {"addresses", required_argument, NULL, 'a'},
      {"recursive", no_argument, NULL, 'r'},
      {"fast", no_argument, NULL, 'f'},
      {"database", required_argument, NULL, 'd'},
#ifdef USE_OPTIMIZED_PARSER
      {"performance", no_argument, NULL, 'p'},
      {"cpu-info", no_argument, NULL, 'c'},
      {"threads", required_argument, NULL, 'T'},
#endif
      {NULL, 0, NULL, 0}};

  const char *short_options = "ho:t:vDml:Aa:rfd:";
  char *output_file = DEFAULT_OUTPUT_FILE;
  char *db_file = NULL;
  int thread_count = DEFAULT_THREAD_COUNT;
  int address_count = 1;
  char *languages_str = NULL;
  bool all_languages = false;

  /* Set default configuration values */
  memset(&g_config, 0, sizeof(SeedParserConfig));
  memset(&g_stats, 0, sizeof(SeedParserStats));
  g_config.thread_count = DEFAULT_THREAD_COUNT;
  g_config.recursive = false;
  g_config.detect_monero = false;
  g_config.fast_mode = false;
  g_config.max_wallets = 1;

  /* Add default word chain sizes */
  g_config.word_chain_count = 2;
  g_config.word_chain_sizes[0] = 12;
  g_config.word_chain_sizes[1] = 24;

  /* Enable English by default */
  g_config.languages[0] = LANGUAGE_ENGLISH;
  g_config.language_count = 1;

  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, short_options, long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'h':
      print_usage(argv[0]);
      exit(EXIT_SUCCESS);
      break;

    case 'o':
      output_file = optarg;
      break;

    case 't':
      thread_count = atoi(optarg);
      if (thread_count <= 0) {
        fprintf(stderr, "Error: Invalid thread count: %s\n", optarg);
        return false;
      }
      g_config.thread_count = thread_count;
      break;

    case 'v':
      g_verbose = true;
      break;

    case 'D':
      g_debug_enabled = true;
      printf("Debug mode enabled\n");
      break;

    case 'm':
      g_config.detect_monero = true;
      break;

    case 'l':
      languages_str = optarg;
      break;

    case 'A':
      all_languages = true;
      break;

    case 'a':
      address_count = atoi(optarg);
      if (address_count <= 0) {
        fprintf(stderr, "Error: Invalid address count: %s\n", optarg);
        return false;
      }
      g_config.max_wallets = address_count;
      break;

    case 'r':
      g_config.recursive = true;
      break;

    case 'f':
      g_config.fast_mode = true;
      break;

    case 'd':
      db_file = optarg;
      break;

#ifdef USE_OPTIMIZED_PARSER
    case 'p':
      g_config.show_performance = true;
      break;

    case 'c':
      g_config.show_cpu_info = true;
      break;

    case 'T':
      g_config.threads = atoi(optarg);
      if (g_config.threads < 0) {
        fprintf(stderr, "Error: Invalid thread count: %s\n", optarg);
        return false;
      }
      break;
#endif

    case '?':
      return false;

    default:
      fprintf(stderr, "Error: Unexpected option: %c\n", opt);
      return false;
    }
  }

  /* Set output file path */
  strncpy(g_config.output_file, output_file, sizeof(g_config.output_file) - 1);
  g_config.output_file[sizeof(g_config.output_file) - 1] = '\0';

  /* Set database file path if provided */
  if (db_file) {
    strncpy(g_config.db_file, db_file, sizeof(g_config.db_file) - 1);
    g_config.db_file[sizeof(g_config.db_file) - 1] = '\0';
    g_config.use_database = true;
  } else {
    g_config.use_database = false;
  }

  /* Enable all languages if requested */
  if (all_languages) {
    g_config.language_count = 0;
    g_config.languages[g_config.language_count++] = LANGUAGE_ENGLISH;
    g_config.languages[g_config.language_count++] = LANGUAGE_FRENCH;
    g_config.languages[g_config.language_count++] = LANGUAGE_SPANISH;
    g_config.languages[g_config.language_count++] = LANGUAGE_ITALIAN;
    g_config.languages[g_config.language_count++] = LANGUAGE_PORTUGUESE;
    g_config.languages[g_config.language_count++] = LANGUAGE_CZECH;
    g_config.languages[g_config.language_count++] = LANGUAGE_JAPANESE;
    g_config.languages[g_config.language_count++] = LANGUAGE_CHINESE_SIMPLIFIED;
    g_config.languages[g_config.language_count++] =
        LANGUAGE_CHINESE_TRADITIONAL;
    g_config.languages[g_config.language_count++] = LANGUAGE_KOREAN;
    printf("Enabling all supported languages (%zu)\n", g_config.language_count);
  }
  /* Parse languages if provided */
  else if (languages_str) {
    char *token;
    char *str = strdup(languages_str);
    char *tofree = str;
    int language_count = 0;

    token = strtok(str, ",");
    while (token && language_count < LANGUAGE_COUNT) {
      bool found = false;

      /* Map language name to enum */
      if (strcasecmp(token, "english") == 0) {
        g_config.languages[language_count++] = LANGUAGE_ENGLISH;
        found = true;
      } else if (strcasecmp(token, "french") == 0) {
        g_config.languages[language_count++] = LANGUAGE_FRENCH;
        found = true;
      } else if (strcasecmp(token, "spanish") == 0) {
        g_config.languages[language_count++] = LANGUAGE_SPANISH;
        found = true;
      } else if (strcasecmp(token, "italian") == 0) {
        g_config.languages[language_count++] = LANGUAGE_ITALIAN;
        found = true;
      } else if (strcasecmp(token, "portuguese") == 0) {
        g_config.languages[language_count++] = LANGUAGE_PORTUGUESE;
        found = true;
      } else if (strcasecmp(token, "czech") == 0) {
        g_config.languages[language_count++] = LANGUAGE_CZECH;
        found = true;
      } else if (strcasecmp(token, "japanese") == 0) {
        g_config.languages[language_count++] = LANGUAGE_JAPANESE;
        found = true;
      } else if (strcasecmp(token, "chinese_simplified") == 0) {
        g_config.languages[language_count++] = LANGUAGE_CHINESE_SIMPLIFIED;
        found = true;
      } else if (strcasecmp(token, "chinese_traditional") == 0) {
        g_config.languages[language_count++] = LANGUAGE_CHINESE_TRADITIONAL;
        found = true;
      } else if (strcasecmp(token, "korean") == 0) {
        g_config.languages[language_count++] = LANGUAGE_KOREAN;
        found = true;
      }

      if (!found) {
        fprintf(stderr, "Warning: Unsupported language: %s\n", token);
      }

      token = strtok(NULL, ",");
    }

    free(tofree);

    if (language_count > 0) {
      g_config.language_count = language_count;
    }
  }

  /* If Monero detection is enabled, add 25 to word chain sizes */
  if (g_config.detect_monero) {
    g_config.word_chain_sizes[g_config.word_chain_count++] = 25;
  }

  /* Get paths to scan */
  g_config.path_count = 0;

  while (optind < argc && g_config.path_count < MAX_SCAN_PATHS) {
    strncpy(g_config.paths[g_config.path_count], argv[optind], PATH_MAX - 1);
    g_config.paths[g_config.path_count][PATH_MAX - 1] = '\0';
    g_config.path_count++;
    optind++;
  }

  /* Use default path if none specified */
  if (g_config.path_count == 0) {
    strncpy(g_config.paths[0], DEFAULT_SCAN_PATH, PATH_MAX - 1);
    g_config.paths[0][PATH_MAX - 1] = '\0';
    g_config.path_count = 1;
  }

  return true;
}

/**
 * @brief Print parser configuration
 */
void print_config(void) {
  printf("Configuration:\n");
  printf("  Output File: %s\n", g_config.output_file);
  printf("  Thread Count: %zu\n", g_config.thread_count);
  printf("  Recursive Mode: %s\n", g_config.recursive ? "Enabled" : "Disabled");
  printf("  Monero Detection: %s\n",
         g_config.detect_monero ? "Enabled" : "Disabled");
  printf("  Fast Mode: %s\n", g_config.fast_mode ? "Enabled" : "Disabled");
  printf("  Database: %s\n",
         g_config.use_database ? g_config.db_file : "Disabled");
  printf("  Max Wallets: %zu\n", g_config.max_wallets);

  printf("  Languages:");
  for (size_t i = 0; i < g_config.language_count; i++) {
    const char *lang_name = "Unknown";

    switch (g_config.languages[i]) {
    case LANGUAGE_ENGLISH:
      lang_name = "English";
      break;
    case LANGUAGE_SPANISH:
      lang_name = "Spanish";
      break;
    case LANGUAGE_FRENCH:
      lang_name = "French";
      break;
    case LANGUAGE_ITALIAN:
      lang_name = "Italian";
      break;
    case LANGUAGE_PORTUGUESE:
      lang_name = "Portuguese";
      break;
    case LANGUAGE_CZECH:
      lang_name = "Czech";
      break;
    case LANGUAGE_JAPANESE:
      lang_name = "Japanese";
      break;
    case LANGUAGE_CHINESE_SIMPLIFIED:
      lang_name = "Chinese (Simplified)";
      break;
    case LANGUAGE_CHINESE_TRADITIONAL:
      lang_name = "Chinese (Traditional)";
      break;
    case LANGUAGE_KOREAN:
      lang_name = "Korean";
      break;
    case LANGUAGE_COUNT:
      lang_name = "Unknown";
      break;
    }

    printf(" %s%s", lang_name, i < g_config.language_count - 1 ? "," : "");
  }
  printf("\n");

  printf("  Word Chain Sizes:");
  for (size_t i = 0; i < g_config.word_chain_count; i++) {
    printf(" %zu%s", g_config.word_chain_sizes[i],
           i < g_config.word_chain_count - 1 ? "," : "");
  }
  printf("\n");

  printf("  Paths to Scan:\n");
  for (size_t i = 0; i < g_config.path_count; i++) {
    printf("    %s\n", g_config.paths[i]);
  }
  printf("\n");
}

/**
 * @brief Print parser statistics
 */
void print_stats(void) {
  printf("Statistics:\n");
  printf("  Files Processed: %lu\n", g_stats.files_processed);
  printf("  Files Skipped: %lu\n", g_stats.files_skipped);
  printf("  Total Lines Processed: %lu\n", g_stats.lines_processed);
  printf("  Total Bytes Processed: %lu\n", g_stats.bytes_processed);
  printf("  BIP-39 Phrases Found: %llu\n", g_stats.bip39_phrases_found);

  if (g_config.detect_monero) {
    printf("  Monero Phrases Found: %llu\n", g_stats.monero_phrases_found);
  }

  printf("  Elapsed Time: %.2f seconds\n", g_stats.elapsed_time);

  if (g_stats.elapsed_time > 0) {
    printf("  Processing Speed: %.2f MB/s\n",
           (double)g_stats.bytes_processed /
               (1024 * 1024 * g_stats.elapsed_time));
  }

  printf("\n");
}

/**
 * @brief Progress callback function
 */
void progress_callback(const char *file_path, const SeedParserStats *stats) {
  /* Copy stats */
  memcpy(&g_stats, stats, sizeof(SeedParserStats));

  if (g_verbose) {
    printf("Processing: %s\n", file_path);
  }
}

/**
 * @brief Seed phrase found callback function
 */
void seed_found_callback(const char *file_path, const char *mnemonic,
                         MnemonicType type, MnemonicLanguage language,
                         size_t line_number) {
  const char *type_str = (type == MNEMONIC_BIP39) ? "BIP-39" : "Monero";
  const char *lang_str = "Unknown";

  switch (language) {
  case LANGUAGE_ENGLISH:
    lang_str = "English";
    break;
  case LANGUAGE_FRENCH:
    lang_str = "French";
    break;
  case LANGUAGE_SPANISH:
    lang_str = "Spanish";
    break;
  case LANGUAGE_ITALIAN:
    lang_str = "Italian";
    break;
  case LANGUAGE_PORTUGUESE:
    lang_str = "Portuguese";
    break;
  case LANGUAGE_CZECH:
    lang_str = "Czech";
    break;
  case LANGUAGE_JAPANESE:
    lang_str = "Japanese";
    break;
  case LANGUAGE_CHINESE_SIMPLIFIED:
    lang_str = "Chinese (Simplified)";
    break;
  case LANGUAGE_CHINESE_TRADITIONAL:
    lang_str = "Chinese (Traditional)";
    break;
  case LANGUAGE_KOREAN:
    lang_str = "Korean";
    break;
  case LANGUAGE_COUNT:
    lang_str = "Unknown";
    break;
  }

  printf("Found %s %s mnemonic in %s (line %zu): %s\n", lang_str, type_str,
         file_path, line_number, mnemonic);
}

/**
 * @brief Main function
 */
int main(int argc, char **argv) {
  int result = EXIT_SUCCESS;

  /* Set up signal handlers for graceful shutdown */
  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  /* Parse command line arguments */
  if (!parse_args(argc, argv)) {
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  /* Print configuration */
  if (g_verbose) {
    print_config();
  }

  /* Set wordlist directory in configuration with absolute path */
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    fprintf(stderr, "Error: Unable to get current working directory\n");
    return EXIT_FAILURE;
  }

  /* Initialize the wordlist directory */
  char wordlist_dir[PATH_MAX];
  snprintf(wordlist_dir, sizeof(wordlist_dir), "%s/data", cwd);
  g_config.wordlist_dir = strdup(wordlist_dir);

  /* Set a valid log directory */
  char log_dir[PATH_MAX];
  snprintf(log_dir, sizeof(log_dir), "%s/logs", cwd);
  g_config.log_dir = strdup(log_dir);

  /* Set source directory based on the first path in g_config.paths */
  if (g_config.path_count > 0) {
    g_config.source_dir = strdup(g_config.paths[0]);
    if (g_verbose) {
      printf("Setting scan directory to: %s\n", g_config.source_dir);
    }
  } else {
    g_config.source_dir = strdup(DEFAULT_SCAN_PATH);
    if (g_verbose) {
      printf("No paths specified, setting scan directory to: %s\n",
             g_config.source_dir);
    }
  }

  if (g_verbose) {
    printf("Using wordlist directory: %s\n", g_config.wordlist_dir);
    printf("Using log directory: %s\n", g_config.log_dir);
  }

  /* Set up wordlist paths based on absolute directory */
  const char **wordlist_paths = malloc(22 * sizeof(char *));
  if (!wordlist_paths) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    return EXIT_FAILURE;
  }

  // Create absolute paths for all supported wordlists
  char temp_path[PATH_MAX];
  int path_index = 0;

  // BIP-39 wordlists
  snprintf(temp_path, sizeof(temp_path), "%s/english.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/spanish.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/french.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/italian.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/portuguese.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/czech.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/japanese.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/chinese_simplified.txt",
           wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/chinese_traditional.txt",
           wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  snprintf(temp_path, sizeof(temp_path), "%s/korean.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  // Monero wordlists
  snprintf(temp_path, sizeof(temp_path), "%s/monero_english.txt", wordlist_dir);
  wordlist_paths[path_index++] = strdup(temp_path);

  g_config.wordlist_paths = wordlist_paths;
  g_config.wordlist_count = path_index;

  if (g_verbose) {
    printf("Loaded %zu wordlist paths\n", g_config.wordlist_count);
  }

  /* Initialize modules */
  struct MnemonicContext *mnemonic_ctx;
  mnemonic_ctx = mnemonic_init(g_config.wordlist_dir);
  if (mnemonic_ctx == NULL) {
    fprintf(stderr, "Error: Failed to initialize mnemonic module\n");
    free((void *)g_config.wordlist_dir);
    return EXIT_FAILURE;
  }

  if (wallet_init() != 0) {
    fprintf(stderr, "Error: Failed to initialize wallet module\n");
    mnemonic_cleanup(mnemonic_ctx);
    free((void *)g_config.wordlist_dir);
    return EXIT_FAILURE;
  }

  /* Load wordlists */
  for (size_t i = 0; i < g_config.language_count; i++) {
    if (mnemonic_load_wordlist(mnemonic_ctx, g_config.languages[i]) != 0) {
      fprintf(stderr, "Error: Failed to load wordlist for language %d\n",
              g_config.languages[i]);
      result = EXIT_FAILURE;
      goto cleanup;
    }
  }

  /* Share the mnemonic context with the seed parser */
  seed_parser_set_mnemonic_ctx(mnemonic_ctx);

  /* Load optimized wordlists */
  if (!seed_parser_opt_load_wordlists(g_config.wordlist_dir)) {
    fprintf(stderr, "Warning: Failed to load optimized wordlists\n");
    // Continue anyway as this is just an optimization
  }

  /* Register callbacks */
  seed_parser_register_progress_callback(progress_callback);
  seed_parser_register_seed_found_callback(seed_found_callback);

  /* Initialize the seed parser with our configuration */
  if (!seed_parser_init(&g_config)) {
    fprintf(stderr, "Error: Failed to initialize seed parser\n");
    result = EXIT_FAILURE;
    goto cleanup;
  }

  /* Start seed parser */
  time_t start_time = time(NULL);
  if (seed_parser_start() != 0) {
    fprintf(stderr, "Error: Failed to start seed parser\n");
    result = EXIT_FAILURE;
    goto cleanup;
  }

  /* Wait for completion or termination signal */
  while (g_running && !seed_parser_is_complete()) {
    if (g_verbose) {
      print_stats();
    }

    sleep(1);
  }

  /* Stop seed parser */
  seed_parser_stop();

  /* Calculate elapsed time */
  g_stats.elapsed_time = difftime(time(NULL), start_time);

  /* Print final statistics */
  print_stats();

cleanup:
  /* Clean up resources */
  seed_parser_cleanup();
  wallet_cleanup();
  mnemonic_cleanup(mnemonic_ctx);

  /* Free dynamically allocated wordlist paths */
  if (g_config.wordlist_paths) {
    for (size_t i = 0; i < g_config.wordlist_count; i++) {
      free((void *)g_config.wordlist_paths[i]);
    }
    free(g_config.wordlist_paths);
  }

  /* Free wordlist directory */
  free((void *)g_config.wordlist_dir);

  /* Free log directory */
  free((void *)g_config.log_dir);

  /* Free source directory */
  free((void *)g_config.source_dir);

  return result;
}
