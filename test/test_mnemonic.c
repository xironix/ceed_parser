#include "../include/mnemonic.h"
#include "../include/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Static mnemonic context for tests
static struct MnemonicContext ctx;
static bool initialized = false;

// Test wordlist paths
static const char *wordlist_paths[] = {
    "./data/english.txt", "../data/english.txt", "../../data/english.txt",
    "data/english.txt"};

// Check if a file exists
static bool file_exists(const char *path) {
  struct stat buffer;
  return (stat(path, &buffer) == 0);
}

// Check if a directory exists
static bool dir_exists(const char *path) {
  struct stat buffer;
  return (stat(path, &buffer) == 0 && S_ISDIR(buffer.st_mode));
}

// Find a valid wordlist directory
static char *find_wordlist_dir(void) {
  const char *dirs[] = {"./data", "../data", "../../data", "data"};

  for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
    if (dir_exists(dirs[i])) {
      printf("Found wordlist directory: %s\n", dirs[i]);

      // Check if english.txt exists in this directory
      char path[256];
      snprintf(path, sizeof(path), "%s/english.txt", dirs[i]);

      if (file_exists(path)) {
        printf("Found wordlist file: %s\n", path);
        return strdup(dirs[i]);
      }
    }
  }

  return NULL;
}

// Setup function for mnemonic tests
static void test_setup(void) {
  if (!initialized) {
    // Find a valid wordlist directory
    char *wordlist_dir = find_wordlist_dir();

    if (!wordlist_dir) {
      printf("ERROR: Could not find wordlist directory!\n");
      printf(
          "Test requires wordlist files in ./data, ../data, or ../../data\n");
      return;
    }

    struct MnemonicContext *ctx_ptr = mnemonic_init(wordlist_dir);
    free(wordlist_dir); // Free the directory path

    if (ctx_ptr) {
      ctx = *ctx_ptr;
      free(ctx_ptr);
      initialized = true;
      printf("Mnemonic context initialized successfully\n");
    } else {
      printf("Failed to initialize mnemonic context\n");
    }
  }
}

// Teardown function for mnemonic tests
static void test_teardown(void) {
  // Cleanup will be done at the end of all tests
}

// Test valid BIP-39 mnemonic validation
static void test_valid_bip39_mnemonic(void) {
  if (!initialized) {
    printf(
        "Skipping test_valid_bip39_mnemonic due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *valid_mnemonic =
      "abandon abandon abandon abandon abandon abandon abandon abandon abandon "
      "abandon abandon about";
  MnemonicType type = MNEMONIC_BIP39;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(mnemonic_validate(&ctx, valid_mnemonic, &type, &language));
  printf("✓ Valid BIP-39 mnemonic test passed\n");
}

// Test invalid BIP-39 mnemonic validation
static void test_invalid_bip39_mnemonic(void) {
  if (!initialized) {
    printf(
        "Skipping test_invalid_bip39_mnemonic due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *invalid_mnemonic = "abandon invalid mnemonic phrase that should "
                                 "not validate properly abandon";
  MnemonicType type = MNEMONIC_BIP39;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(!mnemonic_validate(&ctx, invalid_mnemonic, &type, &language));
  printf("✓ Invalid BIP-39 mnemonic test passed\n");
}

// Test valid Monero mnemonic validation
static void test_valid_monero_mnemonic(void) {
  if (!initialized) {
    printf(
        "Skipping test_valid_monero_mnemonic due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *valid_monero =
      "gels zeal lucky jeers irony tamper older pests noggin orange android "
      "academy bailed mural tossed accent atlas layout drinks ozone academy "
      "academy avatar onset";

  MnemonicType type = MNEMONIC_MONERO;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(mnemonic_validate(&ctx, valid_monero, &type, &language));
  printf("✓ Valid Monero mnemonic test passed\n");
}

// Run all mnemonic tests
bool run_mnemonic_tests(void) {
  UNITY_BEGIN_TEST_SUITE("Mnemonic Tests");

  printf("\n=== Running Mnemonic Tests ===\n");

  // Setup
  test_setup();

  // Run tests
  UNITY_RUN_TEST(test_valid_bip39_mnemonic);
  UNITY_RUN_TEST(test_invalid_bip39_mnemonic);
  UNITY_RUN_TEST(test_valid_monero_mnemonic);

  // Don't teardown after each test, just at the end
  test_teardown();

  if (initialized) {
    // We only need to free wordlists and wordlist_dir, not the ctx itself
    // since it's a stack variable now
    if (ctx.wordlists) {
      for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
        if (ctx.languages_loaded[i] && ctx.wordlists[i].words) {
          // Free each word
          for (size_t j = 0; j < ctx.wordlists[i].word_count; j++) {
            free(ctx.wordlists[i].words[j]);
          }
          // Free the words array
          free(ctx.wordlists[i].words);
        }
      }
      // Free the wordlists array
      free(ctx.wordlists);
    }
    // Free the wordlist directory path
    free(ctx.wordlist_dir);

    initialized = false;
  }

  UNITY_END_TEST_SUITE();
}
