/**
 * @file mnemonic.c
 * @brief Implementation of mnemonic validation and processing
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Remove OpenSSL dependencies
// #include <openssl/sha.h>
// #include <openssl/hmac.h>
// #include <openssl/evp.h>

#include "../include/mnemonic.h"

// Define missing constants
#define MAX_WORD_LENGTH 32
#define MAX_WORDLIST_SIZE 2048

/**
 * @brief Language name mapping
 */
static const char *LANGUAGE_NAMES[] = {"english",
                                       "spanish",
                                       "french",
                                       "italian",
                                       "portuguese",
                                       "czech",
                                       "japanese",
                                       "korean",
                                       "chinese_simplified",
                                       "chinese_traditional"};

/**
 * @brief Language filename mapping
 */
static const char *LANGUAGE_FILES[] = {"english.txt",
                                       "spanish.txt",
                                       "french.txt",
                                       "italian.txt",
                                       "portuguese.txt",
                                       "czech.txt",
                                       "japanese.txt",
                                       "korean.txt",
                                       "chinese_simplified.txt",
                                       "chinese_traditional.txt"};

/**
 * @brief Binary search function for wordlists
 */
static int binary_search(const char *const *words, size_t count,
                         const char *word) {
  int left = 0;
  int right = count - 1;

  while (left <= right) {
    int mid = (left + right) / 2;
    int cmp = strcmp(words[mid], word);

    if (cmp == 0) {
      return mid;
    } else if (cmp < 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  return -1;
}

/**
 * @brief Convert bits to bytes
 */
static void bits_to_bytes(const bool *bits, size_t num_bits, uint8_t *bytes) {
  memset(bytes, 0, (num_bits + 7) / 8);

  for (size_t i = 0; i < num_bits; i++) {
    if (bits[i]) {
      bytes[i / 8] |= 1 << (7 - (i % 8));
    }
  }
}

/**
 * @brief Convert bytes to bits
 */
static void __attribute__((unused))
bytes_to_bits(const uint8_t *bytes, size_t num_bytes, bool *bits) {
  for (size_t i = 0; i < num_bytes * 8; i++) {
    bits[i] = (bytes[i / 8] >> (7 - (i % 8))) & 1;
  }
}

/**
 * @brief Calculate SHA-256 hash
 *
 * This is a simplified placeholder implementation.
 * In a real implementation, you would use a proper SHA-256 library.
 */
static void sha256(const uint8_t *input, size_t input_len, uint8_t *output) {
  // Simplified placeholder implementation
  // In a real implementation, you would use a proper SHA-256 library
  memset(output, 0, 32); // SHA-256 produces a 32-byte hash

  // Simple hash function for demonstration purposes only
  // DO NOT use this in production!
  for (size_t i = 0; i < input_len; i++) {
    output[i % 32] ^= input[i];
  }
}

/**
 * @brief Initialize the mnemonic module
 */
struct MnemonicContext *mnemonic_init(const char *wordlist_dir) {
  // Add debug message to track initialization
  fprintf(stderr, "Initializing mnemonic with directory: %s\n",
          wordlist_dir ? wordlist_dir : "NULL");

  // Validate wordlist_dir is not NULL
  if (!wordlist_dir) {
    fprintf(stderr, "Error: wordlist_dir is NULL\n");
    return NULL;
  }

  // Allocate context - single allocation point
  struct MnemonicContext *ctx =
      (struct MnemonicContext *)calloc(1, sizeof(struct MnemonicContext));
  if (!ctx) {
    fprintf(stderr, "Error: Failed to allocate memory for mnemonic context\n");
    return NULL;
  }

  // Use calloc instead of malloc+memset for safer initialization
  fprintf(stderr, "Allocated context at %p\n", (void *)ctx);

  // Duplicate the wordlist directory path
  ctx->wordlist_dir = strdup(wordlist_dir);
  if (!ctx->wordlist_dir) {
    fprintf(stderr, "Error: Failed to duplicate wordlist directory path\n");
    free(ctx); // Free the context if path duplication fails
    return NULL;
  }

  fprintf(stderr, "Using wordlist directory: %s\n", ctx->wordlist_dir);

  // Allocate memory for wordlists using calloc for zero-initialization
  ctx->wordlists = (Wordlist *)calloc(LANGUAGE_COUNT, sizeof(Wordlist));
  if (!ctx->wordlists) {
    fprintf(stderr, "Error: Failed to allocate memory for wordlists\n");
    free(ctx->wordlist_dir); // Free the path if wordlist allocation fails
    free(ctx);               // Free the context
    return NULL;
  }

  // No need for memset with calloc
  ctx->initialized = true;

  return ctx;
}

/**
 * @brief Clean up resources used by mnemonic module
 */
void mnemonic_cleanup(struct MnemonicContext *ctx) {
  if (!ctx) {
    return; // Nothing to clean up
  }

  // Free wordlists
  if (ctx->wordlists) {
    for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
      if (ctx->languages_loaded[i] && ctx->wordlists[i].words) {
        // Free each word
        for (size_t j = 0; j < ctx->wordlists[i].word_count; j++) {
          free(ctx->wordlists[i].words[j]);
          ctx->wordlists[i].words[j] = NULL; // Set to NULL after freeing
        }
        // Free the words array
        free(ctx->wordlists[i].words);
        ctx->wordlists[i].words = NULL; // Set to NULL after freeing
      }
    }
    // Free the wordlists array
    free(ctx->wordlists);
    ctx->wordlists = NULL; // Set to NULL after freeing
  }

  // Free the wordlist directory path
  if (ctx->wordlist_dir) {
    free(ctx->wordlist_dir);
    ctx->wordlist_dir = NULL; // Set to NULL after freeing
  }

  // Free the context itself
  free(ctx);
}

/**
 * @brief Load a wordlist from a file
 */
int mnemonic_load_wordlist(struct MnemonicContext *ctx,
                           MnemonicLanguage language) {
  if (!ctx) {
    fprintf(stderr, "Error: mnemonic context is NULL\n");
    return -1;
  }

  if (language >= LANGUAGE_COUNT) {
    fprintf(stderr, "Invalid language index: %d (max is %d)\n", language,
            LANGUAGE_COUNT - 1);
    return -1;
  }

  /* Check if already loaded */
  if (ctx->languages_loaded[language]) {
    fprintf(stderr, "Language %d already loaded, skipping\n", language);
    return 0;
  }

  /* Build the path to the wordlist file */
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", ctx->wordlist_dir,
           LANGUAGE_FILES[language]);

  /* Log the path we're trying to access */
  fprintf(stderr, "Attempting to load wordlist file: %s (language: %s)\n", path,
          LANGUAGE_NAMES[language]);

  /* Verify if directory exists */
  struct stat dir_stat;
  if (stat(ctx->wordlist_dir, &dir_stat) != 0) {
    fprintf(stderr,
            "Error: Wordlist directory does not exist: %s (errno: %d, %s)\n",
            ctx->wordlist_dir, errno, strerror(errno));
    return -1;
  }

  if (!S_ISDIR(dir_stat.st_mode)) {
    fprintf(stderr, "Error: %s is not a directory\n", ctx->wordlist_dir);
    return -1;
  }

  /* Open the file */
  FILE *file = fopen(path, "r");
  if (!file) {
    fprintf(stderr, "Error opening wordlist file: %s (errno: %d, %s)\n", path,
            errno, strerror(errno));
    return -1;
  }

  /* Initialize the wordlist */
  Wordlist *wordlist = &ctx->wordlists[language];
  memset(wordlist, 0, sizeof(Wordlist));
  wordlist->language = language;

  /* Allocate memory for words array */
  wordlist->words = (char **)malloc(MAX_WORDLIST_SIZE * sizeof(char *));
  if (!wordlist->words) {
    fprintf(stderr, "Error: Failed to allocate memory for words array\n");
    fclose(file);
    return -1;
  }

  /* Read all words */
  char line[MAX_WORD_LENGTH + 2]; /* +2 for newline and null terminator */
  size_t word_count = 0;

  while (fgets(line, sizeof(line), file) && word_count < MAX_WORDLIST_SIZE) {
    /* Remove trailing newline */
    size_t len = strlen(line);
    if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[len - 1] = '\0';
      if (len > 1 && line[len - 2] == '\r') {
        line[len - 2] = '\0';
      }
    }

    /* Skip empty lines */
    if (line[0] == '\0') {
      continue;
    }

    /* Allocate memory for this word */
    wordlist->words[word_count] = (char *)malloc(MAX_WORD_LENGTH);
    if (!wordlist->words[word_count]) {
      /* Clean up on failure */
      for (size_t i = 0; i < word_count; i++) {
        free(wordlist->words[i]);
      }
      free(wordlist->words);
      wordlist->words = NULL;
      fclose(file);
      return -1;
    }

    /* Copy the word */
    strncpy(wordlist->words[word_count], line, MAX_WORD_LENGTH - 1);
    wordlist->words[word_count][MAX_WORD_LENGTH - 1] = '\0';
    word_count++;
  }

  fclose(file);

  /* Make sure we have the correct number of words */
  if (word_count != 2048) {
    fprintf(stderr, "Warning: Wordlist %s has %zu words (expected 2048)\n",
            LANGUAGE_FILES[language], word_count);
  }

  wordlist->word_count = word_count;
  ctx->languages_loaded[language] = true;

  return 0;
}

/**
 * @brief Detect the language of a mnemonic phrase
 */
MnemonicLanguage mnemonic_detect_language(struct MnemonicContext *ctx,
                                          const char *mnemonic) {
  if (!ctx || !mnemonic) {
    return LANGUAGE_COUNT;
  }

  /* Get the first word */
  char first_word[MAX_WORD_LENGTH];
  const char *p = mnemonic;

  /* Skip leading spaces */
  while (*p && isspace(*p)) {
    p++;
  }

  /* Extract the first word */
  size_t i = 0;
  while (*p && !isspace(*p) && i < MAX_WORD_LENGTH - 1) {
    first_word[i++] = *p++;
  }
  first_word[i] = '\0';

  /* Check each loaded language */
  for (size_t lang = 0; lang < LANGUAGE_COUNT; lang++) {
    if (!ctx->languages_loaded[lang]) {
      continue;
    }

    const Wordlist *wordlist = &ctx->wordlists[lang];

    /* Check if the first word is in this wordlist */
    for (size_t j = 0; j < wordlist->word_count; j++) {
      if (strcmp(wordlist->words[j], first_word) == 0) {
        return lang;
      }
    }
  }

  return LANGUAGE_COUNT;
}

/**
 * @brief Find a word in a wordlist
 */
static int find_word_in_wordlist(const Wordlist *wordlist, const char *word) {
  /* Use binary search for large wordlists, linear search for small ones */
  if (wordlist->word_count > 100) {
    const char *words[MAX_WORDLIST_SIZE];
    for (size_t i = 0; i < wordlist->word_count; i++) {
      words[i] = wordlist->words[i];
    }
    return binary_search(words, wordlist->word_count, word);
  } else {
    for (size_t i = 0; i < wordlist->word_count; i++) {
      if (strcmp(wordlist->words[i], word) == 0) {
        return i;
      }
    }
    return -1;
  }
}

/**
 * @brief Validate a standard BIP-39 mnemonic
 */
static bool validate_bip39(struct MnemonicContext *ctx, const char *mnemonic,
                           MnemonicLanguage *language) {
  if (!ctx || !mnemonic) {
    return false;
  }

  /* Detect language if not specified */
  MnemonicLanguage detected_lang = mnemonic_detect_language(ctx, mnemonic);
  if (detected_lang == LANGUAGE_COUNT) {
    /* Try to load English as fallback */
    if (!ctx->languages_loaded[LANGUAGE_ENGLISH]) {
      mnemonic_load_wordlist(ctx, LANGUAGE_ENGLISH);
    }
    detected_lang = LANGUAGE_ENGLISH;
  }

  /* Set the detected language */
  if (language) {
    *language = detected_lang;
  }

  /* Make sure the language is loaded */
  if (!ctx->languages_loaded[detected_lang]) {
    return false;
  }

  const Wordlist *wordlist = &ctx->wordlists[detected_lang];

  /* Tokenize the mnemonic into words */
  char mnemonic_copy[1024];
  strncpy(mnemonic_copy, mnemonic, sizeof(mnemonic_copy) - 1);
  mnemonic_copy[sizeof(mnemonic_copy) - 1] = '\0';

  char *words[MAX_MNEMONIC_WORDS];
  size_t word_count = 0;

  char *token = strtok(mnemonic_copy, " ");
  while (token && word_count < MAX_MNEMONIC_WORDS) {
    words[word_count++] = token;
    token = strtok(NULL, " ");
  }

  /* Check word count */
  if (word_count != 12 && word_count != 15 && word_count != 18 &&
      word_count != 21 && word_count != 24) {
    return false;
  }

  /* Entropy + checksum length in bits */
  size_t total_bits = word_count * 11;
  size_t entropy_bits = total_bits - (total_bits / 33);

  /* Convert words to bits */
  bool bits[MAX_MNEMONIC_WORDS * 11];

  for (size_t i = 0; i < word_count; i++) {
    int index = find_word_in_wordlist(wordlist, words[i]);
    if (index < 0) {
      return false;
    }

    /* Convert the index to 11 bits */
    for (size_t j = 0; j < 11; j++) {
      bits[i * 11 + j] = (index >> (10 - j)) & 1;
    }
  }

  /* Extract entropy and checksum */
  uint8_t entropy[32];
  bits_to_bytes(bits, entropy_bits, entropy);

  /* Calculate checksum */
  uint8_t hash[32];
  sha256(entropy, entropy_bits / 8, hash);

  /* Verify checksum */
  size_t checksum_bits = total_bits - entropy_bits;
  for (size_t i = 0; i < checksum_bits; i++) {
    if (bits[entropy_bits + i] != ((hash[0] >> (7 - i)) & 1)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Validate a Monero 25-word seed phrase
 */
static bool validate_monero(struct MnemonicContext *ctx, const char *mnemonic,
                            MnemonicLanguage *language) {
  if (!ctx || !mnemonic) {
    return false;
  }

  /* Detect language if not specified */
  MnemonicLanguage detected_lang = mnemonic_detect_language(ctx, mnemonic);
  if (detected_lang == LANGUAGE_COUNT) {
    /* Try to load English as fallback */
    if (!ctx->languages_loaded[LANGUAGE_ENGLISH]) {
      mnemonic_load_wordlist(ctx, LANGUAGE_ENGLISH);
    }
    detected_lang = LANGUAGE_ENGLISH;
  }

  /* Set the detected language */
  if (language) {
    *language = detected_lang;
  }

  /* Make sure the language is loaded */
  if (!ctx->languages_loaded[detected_lang]) {
    return false;
  }

  const Wordlist *wordlist = &ctx->wordlists[detected_lang];

  /* Tokenize the mnemonic into words */
  char mnemonic_copy[1024];
  strncpy(mnemonic_copy, mnemonic, sizeof(mnemonic_copy) - 1);
  mnemonic_copy[sizeof(mnemonic_copy) - 1] = '\0';

  char *words[MAX_MNEMONIC_WORDS];
  size_t word_count = 0;

  char *token = strtok(mnemonic_copy, " ");
  while (token && word_count < MAX_MNEMONIC_WORDS) {
    words[word_count++] = token;
    token = strtok(NULL, " ");
  }

  /* Check word count */
  if (word_count != 25) {
    return false;
  }

  /* For Monero, we just check if all words are in the dictionary */
  /* A more comprehensive check would involve Monero-specific crypto */
  for (size_t i = 0; i < word_count; i++) {
    int index = find_word_in_wordlist(wordlist, words[i]);
    if (index < 0) {
      return false;
    }
  }

  /* Simple checksum for Monero: the last word is a checksum */
  /* For a real implementation, we would need the full Monero seed validation
   * logic */

  return true;
}

/**
 * @brief Validate a mnemonic phrase
 */
bool mnemonic_validate(struct MnemonicContext *ctx, const char *mnemonic,
                       MnemonicType *type, MnemonicLanguage *language) {
  if (!ctx || !mnemonic) {
    return false;
  }

  /* Copy the mnemonic for counting words */
  char mnemonic_copy[1024];
  strncpy(mnemonic_copy, mnemonic, sizeof(mnemonic_copy) - 1);
  mnemonic_copy[sizeof(mnemonic_copy) - 1] = '\0';

  /* Count words */
  size_t word_count = 0;
  char *token = strtok(mnemonic_copy, " ");
  while (token) {
    word_count++;
    token = strtok(NULL, " ");
  }

  /* Determine type based on word count */
  MnemonicType detected_type;
  if (word_count == 25) {
    detected_type = MNEMONIC_MONERO;
  } else {
    detected_type = MNEMONIC_BIP39;
  }

  /* Set the detected type */
  if (type) {
    *type = detected_type;
  }

  /* Validate based on type */
  bool valid;
  if (detected_type == MNEMONIC_MONERO) {
    valid = validate_monero(ctx, mnemonic, language);
  } else {
    valid = validate_bip39(ctx, mnemonic, language);
  }

  return valid;
}

/**
 * @brief Generate entropy from a mnemonic phrase
 */
int mnemonic_to_entropy(struct MnemonicContext *ctx, const char *mnemonic,
                        uint8_t *entropy, size_t *entropy_len) {
  if (!ctx || !mnemonic || !entropy || !entropy_len) {
    return -1;
  }

  /* Detect language */
  MnemonicLanguage lang = mnemonic_detect_language(ctx, mnemonic);
  if (lang == LANGUAGE_COUNT) {
    if (!ctx->languages_loaded[LANGUAGE_ENGLISH]) {
      mnemonic_load_wordlist(ctx, LANGUAGE_ENGLISH);
    }
    lang = LANGUAGE_ENGLISH;
  }

  /* Make sure the language is loaded */
  if (!ctx->languages_loaded[lang]) {
    return -1;
  }

  const Wordlist *wordlist = &ctx->wordlists[lang];

  /* Tokenize the mnemonic into words */
  char mnemonic_copy[1024];
  strncpy(mnemonic_copy, mnemonic, sizeof(mnemonic_copy) - 1);
  mnemonic_copy[sizeof(mnemonic_copy) - 1] = '\0';

  char *words[MAX_MNEMONIC_WORDS];
  size_t word_count = 0;

  char *token = strtok(mnemonic_copy, " ");
  while (token && word_count < MAX_MNEMONIC_WORDS) {
    words[word_count++] = token;
    token = strtok(NULL, " ");
  }

  /* Check word count */
  if (word_count != 12 && word_count != 15 && word_count != 18 &&
      word_count != 21 && word_count != 24) {
    return -1;
  }

  /* Entropy + checksum length in bits */
  size_t total_bits = word_count * 11;
  size_t entropy_bits = total_bits - (total_bits / 33);

  /* Convert words to bits */
  bool bits[MAX_MNEMONIC_WORDS * 11];

  for (size_t i = 0; i < word_count; i++) {
    int index = find_word_in_wordlist(wordlist, words[i]);
    if (index < 0) {
      return -1;
    }

    /* Convert the index to 11 bits */
    for (size_t j = 0; j < 11; j++) {
      bits[i * 11 + j] = (index >> (10 - j)) & 1;
    }
  }

  /* Extract entropy */
  bits_to_bytes(bits, entropy_bits, entropy);
  *entropy_len = entropy_bits / 8;

  return 0;
}

/**
 * @brief Generate a seed from a mnemonic phrase
 */
size_t mnemonic_to_seed(const char *phrase, const char *passphrase,
                        unsigned char *seed, size_t seed_len) {
  if (!phrase || !seed || seed_len < 64) {
    return 0;
  }

  /* Prepare salt */
  const char *salt_prefix = "mnemonic";
  char salt[1024];
  snprintf(salt, sizeof(salt), "%s%s", salt_prefix,
           passphrase ? passphrase : "");

  /* Simple placeholder implementation */
  /* In a real implementation, you would use PBKDF2-HMAC-SHA512 */
  memset(seed, 0, seed_len);

  /* Simple seed generation for demonstration purposes only */
  /* DO NOT use this in production! */
  for (size_t i = 0; i < strlen(phrase); i++) {
    seed[i % 64] ^= phrase[i];
  }

  for (size_t i = 0; i < strlen(salt); i++) {
    seed[(i + 32) % 64] ^= salt[i];
  }

  return 64;
}

/**
 * @brief Get a human-readable language name
 */
const char *mnemonic_language_name(MnemonicLanguage language) {
  if (language >= LANGUAGE_COUNT) {
    return "unknown";
  }

  return LANGUAGE_NAMES[language];
}

/**
 * @brief Check if a mnemonic phrase could be Monero (25 words)
 */
bool mnemonic_is_monero(struct MnemonicContext *ctx, const char *mnemonic) {
  if (!ctx || !mnemonic) {
    return false;
  }

  /* Count the words */
  size_t word_count = 0;
  const char *p = mnemonic;

  /* Skip leading spaces */
  while (*p && isspace(*p)) {
    p++;
  }

  while (*p) {
    /* Count a word */
    word_count++;

    /* Skip the word */
    while (*p && !isspace(*p)) {
      p++;
    }

    /* Skip spaces */
    while (*p && isspace(*p)) {
      p++;
    }
  }

  /* Monero uses 25 words */
  return word_count == 25;
}

/**
 * @brief Check if a word exists in a specific language wordlist
 */
bool mnemonic_word_exists(struct MnemonicContext *ctx,
                          MnemonicLanguage language, const char *word) {
  if (!ctx || !word || language >= LANGUAGE_COUNT) {
    return false;
  }

  /* Check if the language is loaded */
  if (!ctx->languages_loaded[language]) {
    /* Try to load the language */
    if (mnemonic_load_wordlist(ctx, language) != 0) {
      return false;
    }
  }

  /* Use binary search for large wordlists, linear search for small ones */
  const Wordlist *wordlist = &ctx->wordlists[language];

  if (wordlist->word_count > 100) {
    /* Use our internal binary search for consistency */
    int left = 0;
    int right = wordlist->word_count - 1;

    while (left <= right) {
      int mid = (left + right) / 2;
      int cmp = strcmp(wordlist->words[mid], word);

      if (cmp == 0) {
        return true;
      } else if (cmp < 0) {
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }

    return false;
  } else {
    for (size_t i = 0; i < wordlist->word_count; i++) {
      if (strcmp(wordlist->words[i], word) == 0) {
        return true;
      }
    }
    return false;
  }
}
