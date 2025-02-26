#include "../include/seed_parser.h"
#include "../include/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Global variables for testing
static SeedParserConfig config;
static SeedParserStats stats;
static char test_file_path[256];

// Wordlist paths for testing
static const char *wordlist_paths[] = {
    "../data/english.txt", "../data/spanish.txt", "../data/monero_english.txt"};

// Helper function to create a temporary test file with a seed phrase
static bool create_test_file(const char *seed_phrase) {
  sprintf(test_file_path, "/tmp/ceed_parser_test_%d.txt", (int)getpid());

  FILE *f = fopen(test_file_path, "w");
  if (!f) {
    return false;
  }

  fprintf(f, "Some random text before the seed phrase.\n\n");
  fprintf(f, "%s\n", seed_phrase);
  fprintf(f, "\nSome random text after the seed phrase.\n");

  fclose(f);
  return true;
}

// Helper function to remove the temporary test file
static void remove_test_file(void) {
  if (test_file_path[0] != '\0') {
    unlink(test_file_path);
    test_file_path[0] = '\0';
  }
}

// Setup function for parser tests
static void test_setup(void) {
  // Setup a basic configuration
  memset(&config, 0, sizeof(config));
  config.wordlist_paths = wordlist_paths;
  config.wordlist_count = 3;
  config.fast_mode = true;
  config.max_wallets = 1;

  // Reset stats
  memset(&stats, 0, sizeof(stats));

  // Initialize parser
  TEST_ASSERT(seed_parser_init(&config));
}

// Teardown function for parser tests
static void test_teardown(void) {
  seed_parser_cleanup();
  remove_test_file();
}

// Test validating a BIP-39 mnemonic
static void test_validate_bip39(void) {
  const char *valid_bip39 = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  MnemonicType type = MNEMONIC_INVALID;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(seed_parser_validate_mnemonic(valid_bip39, &type, &language));
  TEST_ASSERT_EQUAL(MNEMONIC_BIP39, type);

  printf("✓ BIP-39 validation test passed\n");
}

// Test validating a Monero mnemonic
static void test_validate_monero(void) {
  const char *valid_monero =
      "gels zeal lucky jeers irony tamper older pests noggin orange android "
      "academy bailed mural tossed accent atlas layout drinks ozone academy "
      "academy avatar onset";
  MnemonicType type = MNEMONIC_INVALID;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  TEST_ASSERT(seed_parser_validate_mnemonic(valid_monero, &type, &language));
  TEST_ASSERT_EQUAL(MNEMONIC_MONERO, type);

  printf("✓ Monero validation test passed\n");
}

// Test processing a file with a BIP-39 seed phrase
static void test_process_file_bip39(void) {
  const char *seed = "abandon abandon abandon abandon abandon abandon abandon "
                     "abandon abandon abandon abandon about";

  TEST_ASSERT(create_test_file(seed));
  TEST_ASSERT(seed_parser_process_file(test_file_path));
  TEST_ASSERT(stats.bip39_phrases_found > 0);

  printf("✓ BIP-39 file processing test passed\n");
}

// Test processing a file with a Monero seed phrase
static void test_process_file_monero(void) {
  const char *seed = "gels zeal lucky jeers irony tamper older pests noggin "
                     "orange android academy bailed mural tossed accent atlas "
                     "layout drinks ozone academy academy avatar onset";

  TEST_ASSERT(create_test_file(seed));
  TEST_ASSERT(seed_parser_process_file(test_file_path));
  TEST_ASSERT(stats.monero_phrases_found > 0);

  printf("✓ Monero file processing test passed\n");
}

// Run all parser tests
bool run_parser_tests(void) {
  UNITY_BEGIN_TEST_SUITE("Parser Tests");

  printf("\n=== Running Parser Tests ===\n");

  // Setup
  test_setup();

  // Run tests
  UNITY_RUN_TEST(test_validate_bip39);
  UNITY_RUN_TEST(test_validate_monero);
  UNITY_RUN_TEST(test_process_file_bip39);
  UNITY_RUN_TEST(test_process_file_monero);

  // Teardown
  test_teardown();

  UNITY_END_TEST_SUITE();
}