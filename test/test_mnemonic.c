#include "../include/mnemonic.h"
#include "../include/unity.h"
#include <stdlib.h>
#include <string.h>

// Static mnemonic context for tests
static struct MnemonicContext ctx;
static bool initialized = false;

// Test wordlist paths (for reference)
static const char *__attribute__((unused)) wordlist_paths[] = {
    "./data/english.txt", "./data/spanish.txt", "./data/monero_english.txt"};

// Setup function for mnemonic tests
static void test_setup(void) {
  if (!initialized) {
    struct MnemonicContext *ctx_ptr = mnemonic_init("./data");
    if (ctx_ptr) {
      ctx = *ctx_ptr;
      free(ctx_ptr);
      initialized = true;
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
  const char *invalid_mnemonic = "abandon invalid mnemonic phrase that should "
                                 "not validate properly abandon";
  MnemonicType type = MNEMONIC_BIP39;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(!mnemonic_validate(&ctx, invalid_mnemonic, &type, &language));
  printf("✓ Invalid BIP-39 mnemonic test passed\n");
}

// Test valid Monero mnemonic validation
static void test_valid_monero_mnemonic(void) {
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