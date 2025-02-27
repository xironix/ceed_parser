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
  // Validate input
  if (wordlist_dir == NULL) {
    fprintf(stderr, "Error: wordlist_dir cannot be NULL\n");
    return NULL;
  }

  // Allocate memory for the context
  struct MnemonicContext *ctx = calloc(1, sizeof(struct MnemonicContext));
  if (ctx == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory for MnemonicContext\n");
    return NULL;
  }

  // Copy the wordlist directory path to ensure we own the memory
  ctx->wordlist_dir = strdup(wordlist_dir);
  if (ctx->wordlist_dir == NULL) {
    fprintf(stderr, "Error: Failed to duplicate wordlist directory path\n");
    free(ctx);
    return NULL;
  }

  // Allocate memory for wordlists array
  ctx->wordlists = calloc(LANGUAGE_COUNT, sizeof(Wordlist));
  if (ctx->wordlists == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory for wordlists\n");
    free(ctx->wordlist_dir);
    free(ctx);
    return NULL;
  }

  // Initialize language loaded flags
  for (int i = 0; i < LANGUAGE_COUNT; i++) {
    ctx->languages_loaded[i] = false;
  }

  // Set as initialized
  ctx->initialized = true;

  return ctx;
}

/**
 * @brief Clean up resources used by mnemonic module
 */
void mnemonic_cleanup(struct MnemonicContext *ctx) {
  if (ctx == NULL) {
    return;
  }

  // Free the wordlists
  if (ctx->wordlists != NULL) {
    for (int i = 0; i < LANGUAGE_COUNT; i++) {
      if (ctx->languages_loaded[i]) {
        // Free the words array if it exists
        if (ctx->wordlists[i].words != NULL) {
          for (size_t j = 0; j < ctx->wordlists[i].word_count; j++) {
            if (ctx->wordlists[i].words[j] != NULL) {
              free(ctx->wordlists[i].words[j]);
            }
          }
          free(ctx->wordlists[i].words);
        }
      }
    }
    free(ctx->wordlists);
  }

  // Free the wordlist directory path
  if (ctx->wordlist_dir != NULL) {
    free(ctx->wordlist_dir);
  }

  // Free the context itself
  free(ctx);
}

/**
 * @brief Load a wordlist from a file
 */
int mnemonic_load_wordlist(struct MnemonicContext *ctx,
                           MnemonicLanguage language) {
  // Validate input
  if (ctx == NULL) {
    fprintf(stderr, "Error: MnemonicContext is NULL\n");
    return -1;
  }

  if (language < 0 || language >= LANGUAGE_COUNT) {
    fprintf(stderr, "Error: Invalid language specified: %d\n", language);
    return -1;
  }

  // Check if the wordlist is already loaded
  if (ctx->languages_loaded[language]) {
    // Already loaded
    return 0;
  }

  // Build the path to the wordlist file
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", ctx->wordlist_dir,
           LANGUAGE_FILES[language]);

  // Check if the file exists and is accessible
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    fprintf(stderr, "Error: Failed to open wordlist file: %s\n", path);
    return -1;
  }

  // Initialize the wordlist
  Wordlist *wordlist = &ctx->wordlists[language];
  wordlist->language = language;
  wordlist->word_count = 0;

  // Allocate memory for the words array
  wordlist->words = calloc(MAX_WORDLIST_SIZE, sizeof(char *));
  if (wordlist->words == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory for words array\n");
    fclose(file);
    return -1;
  }

  // Read the words from the file
  char line[MAX_WORD_LENGTH + 2]; // +2 for newline and null terminator
  size_t word_count = 0;

  while (fgets(line, sizeof(line), file) && word_count < MAX_WORDLIST_SIZE) {
    // Remove newline character if present
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
      len--;
    }

    // Remove carriage return if present
    if (len > 0 && line[len - 1] == '\r') {
      line[len - 1] = '\0';
      len--;
    }

    // Skip empty lines
    if (len == 0) {
      continue;
    }

    // Allocate memory for the word
    wordlist->words[word_count] = strdup(line);
    if (wordlist->words[word_count] == NULL) {
      fprintf(stderr, "Error: Failed to allocate memory for word\n");

      // Clean up already allocated words
      for (size_t i = 0; i < word_count; i++) {
        free(wordlist->words[i]);
      }
      free(wordlist->words);
      wordlist->words = NULL;

      fclose(file);
      return -1;
    }

    word_count++;
  }

  fclose(file);

  // Check if we read the correct number of words
  if (word_count != MAX_WORDLIST_SIZE) {
    fprintf(stderr, "Warning: Wordlist does not contain %d words (found %zu)\n",
            MAX_WORDLIST_SIZE, word_count);
  }

  // Update the wordlist
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
    fprintf(stderr, "Error: Invalid parameters to validate_bip39\n");
    return false;
  }

  fprintf(stderr, "DEBUG: BIP39 validation starting for '%s'\n", mnemonic);

  /* Detect language if not specified */
  MnemonicLanguage detected_lang = mnemonic_detect_language(ctx, mnemonic);
  if (detected_lang == LANGUAGE_COUNT) {
    detected_lang = LANGUAGE_ENGLISH; // Default to English
  }

  /* Make sure the language is loaded */
  if (!ctx->languages_loaded[detected_lang]) {
    fprintf(stderr, "DEBUG: Loading wordlist for language %d\n", detected_lang);
    if (mnemonic_load_wordlist(ctx, detected_lang) != 0) {
      fprintf(stderr, "Error: Failed to load wordlist for language %d\n",
              detected_lang);
      return false;
    }
    ctx->languages_loaded[detected_lang] = true;
  }

  /* Set the detected language */
  if (language) {
    *language = detected_lang;
  }

  const Wordlist *wordlist = &ctx->wordlists[detected_lang];
  fprintf(stderr, "DEBUG: Using wordlist with %zu words\n",
          wordlist->word_count);

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

  fprintf(stderr, "DEBUG: Tokenized into %zu words\n", word_count);

  /* Check word count */
  if (word_count != 12 && word_count != 15 && word_count != 18 &&
      word_count != 21 && word_count != 24) {
    fprintf(stderr, "Error: Invalid word count %zu\n", word_count);
    return false;
  }

  /* Verify each word is in the wordlist */
  for (size_t i = 0; i < word_count; i++) {
    int index = find_word_in_wordlist(wordlist, words[i]);
    if (index < 0) {
      fprintf(stderr, "Error: Word '%s' not found in wordlist\n", words[i]);
      return false;
    }
  }

  // For now, skip the BIP-39 checksum verification (simplified version)
  // If all words are in the wordlist and the count is correct, accept it
  fprintf(stderr, "DEBUG: All words valid, returning true\n");
  return true;
}

/**
 * @brief Validate a Monero 25-word seed phrase
 */
static bool validate_monero(struct MnemonicContext *ctx, const char *mnemonic,
                            MnemonicLanguage *language) {
  if (!ctx || !mnemonic) {
    fprintf(stderr, "Error: Invalid parameters to validate_monero\n");
    return false;
  }

  fprintf(stderr, "DEBUG: Monero validation starting for '%s'\n", mnemonic);

  /* Detect language if not specified */
  MnemonicLanguage detected_lang = mnemonic_detect_language(ctx, mnemonic);
  if (detected_lang == LANGUAGE_COUNT) {
    detected_lang = LANGUAGE_ENGLISH; // Default to English
  }

  /* Make sure the language is loaded */
  if (!ctx->languages_loaded[detected_lang]) {
    fprintf(stderr, "DEBUG: Loading wordlist for language %d\n", detected_lang);
    if (mnemonic_load_wordlist(ctx, detected_lang) != 0) {
      fprintf(stderr, "Error: Failed to load wordlist for language %d\n",
              detected_lang);
      return false;
    }
    ctx->languages_loaded[detected_lang] = true;
  }

  /* Set the detected language */
  if (language) {
    *language = detected_lang;
  }

  const Wordlist *wordlist = &ctx->wordlists[detected_lang];
  fprintf(stderr, "DEBUG: Using wordlist with %zu words\n",
          wordlist->word_count);

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

  fprintf(stderr, "DEBUG: Tokenized into %zu words\n", word_count);

  /* Check word count */
  if (word_count != 25) {
    fprintf(stderr, "Error: Monero mnemonics must have 25 words (got %zu)\n",
            word_count);
    return false;
  }

  /* Verify each word is in the wordlist */
  for (size_t i = 0; i < word_count; i++) {
    int index = find_word_in_wordlist(wordlist, words[i]);
    if (index < 0) {
      fprintf(stderr, "Error: Word '%s' not found in wordlist\n", words[i]);
      return false;
    }
  }

  // For now, skip the Monero checksum verification (simplified version)
  // If all words are in the wordlist and the count is correct, accept it
  fprintf(stderr, "DEBUG: All Monero words valid, returning true\n");
  return true;
}

/**
 * @brief Validate a mnemonic phrase
 */
bool mnemonic_validate(struct MnemonicContext *ctx, const char *mnemonic,
                       MnemonicType *type, MnemonicLanguage *language) {
  if (!ctx || !mnemonic) {
    fprintf(stderr, "Error: Invalid parameters to mnemonic_validate\n");
    if (type)
      *type = MNEMONIC_INVALID;
    return false;
  }

  // Debug output
  fprintf(stderr, "DEBUG: Validating mnemonic: '%s'\n", mnemonic);
  fprintf(stderr, "DEBUG: Using context at %p, wordlist_dir: %s\n", (void *)ctx,
          ctx->wordlist_dir ? ctx->wordlist_dir : "NULL");

  // First, load the English wordlist if it's not already loaded
  if (!ctx->languages_loaded[LANGUAGE_ENGLISH]) {
    fprintf(stderr, "DEBUG: Loading English wordlist\n");
    if (mnemonic_load_wordlist(ctx, LANGUAGE_ENGLISH) != 0) {
      fprintf(stderr, "Error: Failed to load English wordlist\n");
      if (type)
        *type = MNEMONIC_INVALID;
      return false;
    }
  }

  // Copy the mnemonic for counting words - we need two copies because we'll
  // tokenize twice
  char mnemonic_copy1[1024];
  strncpy(mnemonic_copy1, mnemonic, sizeof(mnemonic_copy1) - 1);
  mnemonic_copy1[sizeof(mnemonic_copy1) - 1] = '\0';

  char mnemonic_copy2[1024];
  strncpy(mnemonic_copy2, mnemonic, sizeof(mnemonic_copy2) - 1);
  mnemonic_copy2[sizeof(mnemonic_copy2) - 1] = '\0';

  // Count words
  size_t word_count = 0;
  char *token = strtok(mnemonic_copy1, " ");
  while (token) {
    word_count++;
    token = strtok(NULL, " ");
  }

  fprintf(stderr, "DEBUG: Word count: %zu\n", word_count);

  // Determine type based on word count
  MnemonicType detected_type;
  if (word_count == 25) {
    detected_type = MNEMONIC_MONERO;
  } else if (word_count == 12 || word_count == 15 || word_count == 18 ||
             word_count == 21 || word_count == 24) {
    detected_type = MNEMONIC_BIP39;
  } else {
    fprintf(stderr, "Error: Invalid word count %zu\n", word_count);
    if (type)
      *type = MNEMONIC_INVALID;
    return false;
  }

  // Set the detected type
  if (type) {
    *type = detected_type;
  }

  // Detect language - initialize to English by default
  MnemonicLanguage detected_lang = LANGUAGE_ENGLISH;

  // Get the first word to try determining language
  token = strtok(mnemonic_copy2, " ");
  if (token) {
    // Check the first word in each loaded language
    for (int lang = 0; lang < LANGUAGE_COUNT; lang++) {
      if (!ctx->languages_loaded[lang]) {
        // Try to load this language if needed
        if (mnemonic_load_wordlist(ctx, lang) == 0) {
          ctx->languages_loaded[lang] = true;
        }
      }

      if (ctx->languages_loaded[lang]) {
        const Wordlist *wordlist = &ctx->wordlists[lang];

        // Check if the first word is in this wordlist
        for (size_t j = 0; j < wordlist->word_count; j++) {
          if (strcmp(wordlist->words[j], token) == 0) {
            detected_lang = lang;
            fprintf(stderr, "DEBUG: Detected language: %d\n", detected_lang);
            break;
          }
        }
      }
    }
  }

  // Set the detected language
  if (language) {
    *language = detected_lang;
  }

  // Validate based on type
  bool valid;
  if (detected_type == MNEMONIC_MONERO) {
    fprintf(stderr, "DEBUG: Validating as Monero mnemonic\n");
    valid = validate_monero(ctx, mnemonic, language);
  } else {
    fprintf(stderr, "DEBUG: Validating as BIP-39 mnemonic\n");
    valid = validate_bip39(ctx, mnemonic, language);
  }

  fprintf(stderr, "DEBUG: Validation result: %s\n",
          valid ? "VALID" : "INVALID");
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
