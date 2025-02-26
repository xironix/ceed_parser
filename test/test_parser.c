#include "../include/seed_parser.h"
#include "../include/unity.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Global variables for testing
static SeedParserConfig config;
static SeedParserStats stats;
static char test_file_path[256];
static bool parser_initialized = false;

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
  const char *dirs[] = {"./data", "../data", "../../data", "data", "."};

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

  printf("ERROR: Could not find wordlist directory with english.txt!\n");
  printf("Tests require English wordlist file in ./data, ../data, or "
         "../../data\n");
  return NULL;
}

// Helper function to create a temporary test file with a seed phrase
static bool create_test_file(const char *seed_phrase) {
  sprintf(test_file_path, "/tmp/ceed_parser_test_%d.txt", (int)getpid());

  FILE *f = fopen(test_file_path, "w");
  if (!f) {
    printf("Error creating test file %s: %s\n", test_file_path,
           strerror(errno));
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

  // Find a valid wordlist directory
  char *wordlist_dir = find_wordlist_dir();
  if (!wordlist_dir) {
    printf("ERROR: Could not find wordlist directory!\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  // Get absolute path for wordlist directory for better debugging
  char abs_path[PATH_MAX];
  if (realpath(wordlist_dir, abs_path)) {
    // Free the old path and use the absolute path
    free(wordlist_dir);
    wordlist_dir = strdup(abs_path);
    printf("Using absolute wordlist directory path: %s\n", wordlist_dir);
  } else {
    printf("Warning: Could not get absolute path for wordlist directory: %s "
           "(error: %s)\n",
           wordlist_dir, strerror(errno));
  }

  // Setup minimal configuration with just the English wordlist
  char *english_path = (char *)malloc(256 * sizeof(char));
  if (!english_path) {
    printf("Failed to allocate memory for wordlist path\n");
    free(wordlist_dir);
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  snprintf(english_path, 256, "%s/english.txt", wordlist_dir);

  // Check if the file exists
  if (!file_exists(english_path)) {
    printf("ERROR: English wordlist file not found at %s\n", english_path);
    free(english_path);
    free(wordlist_dir);
    TEST_ASSERT(0); // Force test to fail
    return;
  } else {
    printf("Confirmed English wordlist file exists at: %s\n", english_path);
  }

  // For parser tests, we only need the English wordlist
  // Create a single wordlist path
  char **paths = (char **)malloc(sizeof(char *));
  if (!paths) {
    printf("Failed to allocate memory for wordlist paths\n");
    free(english_path);
    free(wordlist_dir);
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  paths[0] = english_path;

  // Setup configuration
  config.wordlist_paths = (const char **)paths;
  config.wordlist_count = 1; // Only using English
  config.fast_mode = true;
  config.max_wallets = 1;
  config.wordlist_dir = wordlist_dir; // Set the wordlist_dir directly

  // Output debug information to verify wordlist_dir is set correctly
  printf("Debug: Setting wordlist_dir to '%s'\n", config.wordlist_dir);

  // Reset stats
  memset(&stats, 0, sizeof(stats));

  // Initialize parser - CRITICAL: config.wordlist_dir must not be NULL
  if (!config.wordlist_dir) {
    printf("ERROR: wordlist_dir is NULL before initialization\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  // Check file access permissions on wordlist
  struct stat st;
  if (stat(english_path, &st) == 0) {
    printf("Wordlist file permissions: %o\n", st.st_mode & 0777);
  }

  bool init_result = seed_parser_init(&config);

  // Debug info for initialization result
  if (init_result) {
    printf("Parser initialized successfully\n");
    parser_initialized = true;
  } else {
    printf("Failed to initialize seed parser\n");

    // Additional diagnostics - avoid using g_parser as it's not accessible
    printf(
        "Diagnostic: Check if mnemonic_init returned NULL in seed_parser.c\n");
    printf("Wordlist dir passed to seed_parser_init: %s\n",
           config.wordlist_dir ? config.wordlist_dir : "NULL");
  }

  TEST_ASSERT(init_result);
}

// Teardown function for parser tests
static void test_teardown(void) {
  // Cleanup parser first
  if (parser_initialized) {
    seed_parser_cleanup();
    parser_initialized = false;
  }

  // Free the wordlist paths we allocated
  if (config.wordlist_paths) {
    if (config.wordlist_paths[0]) {
      free((void *)config.wordlist_paths[0]); // Free the english path
    }
    free(config.wordlist_paths); // Free the paths array
    config.wordlist_paths = NULL;
  }

  // Free the wordlist directory
  if (config.wordlist_dir) {
    free((void *)config.wordlist_dir);
    config.wordlist_dir = NULL;
  }

  remove_test_file();
}

// Test validating a BIP-39 mnemonic
static void test_validate_bip39(void) {
  if (!parser_initialized) {
    printf("Skipping test_validate_bip39 due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *valid_bip39 = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  MnemonicType type = MNEMONIC_INVALID;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  bool validate_result =
      seed_parser_validate_mnemonic(valid_bip39, &type, &language);
  if (!validate_result) {
    printf("Failed to validate BIP-39 mnemonic\n");
  }
  TEST_ASSERT(validate_result);

  if (type != MNEMONIC_BIP39) {
    printf("Expected mnemonic type %d but got %d\n", MNEMONIC_BIP39, type);
  }
  TEST_ASSERT_EQUAL(MNEMONIC_BIP39, type);

  printf("✓ BIP-39 validation test passed\n");
}

// Test validating a Monero mnemonic
static void test_validate_monero(void) {
  if (!parser_initialized) {
    printf("Skipping test_validate_monero due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *valid_monero =
      "gels zeal lucky jeers irony tamper older pests noggin orange android "
      "academy bailed mural tossed accent atlas layout drinks ozone academy "
      "academy avatar onset";
  MnemonicType type = MNEMONIC_INVALID;
  MnemonicLanguage language = LANGUAGE_ENGLISH;

  bool validate_result =
      seed_parser_validate_mnemonic(valid_monero, &type, &language);
  if (!validate_result) {
    printf("Failed to validate Monero mnemonic\n");
  }
  TEST_ASSERT(validate_result);

  if (type != MNEMONIC_MONERO) {
    printf("Expected mnemonic type %d but got %d\n", MNEMONIC_MONERO, type);
  }
  TEST_ASSERT_EQUAL(MNEMONIC_MONERO, type);

  printf("✓ Monero validation test passed\n");
}

// Test processing a file with a BIP-39 seed phrase
static void test_process_file_bip39(void) {
  if (!parser_initialized) {
    printf("Skipping test_process_file_bip39 due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *seed = "abandon abandon abandon abandon abandon abandon abandon "
                     "abandon abandon abandon abandon about";

  bool create_result = create_test_file(seed);
  if (!create_result) {
    printf("Failed to create test file for BIP-39 test\n");
  }
  TEST_ASSERT(create_result);

  bool process_result = seed_parser_process_file(test_file_path);
  if (!process_result) {
    printf("Failed to process test file with BIP-39 seed phrase: %s\n",
           test_file_path);
  }
  TEST_ASSERT(process_result);

  if (stats.bip39_phrases_found <= 0) {
    printf("No BIP-39 phrases found in test file\n");
  }
  TEST_ASSERT(stats.bip39_phrases_found > 0);

  printf("✓ BIP-39 file processing test passed\n");
}

// Test processing a file with a Monero seed phrase
static void test_process_file_monero(void) {
  if (!parser_initialized) {
    printf("Skipping test_process_file_monero due to initialization failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *seed = "gels zeal lucky jeers irony tamper older pests noggin "
                     "orange android academy bailed mural tossed accent atlas "
                     "layout drinks ozone academy academy avatar onset";

  bool create_result = create_test_file(seed);
  if (!create_result) {
    printf("Failed to create test file for Monero test\n");
  }
  TEST_ASSERT(create_result);

  bool process_result = seed_parser_process_file(test_file_path);
  if (!process_result) {
    printf("Failed to process test file with Monero seed phrase: %s\n",
           test_file_path);
  }
  TEST_ASSERT(process_result);

  if (stats.monero_phrases_found <= 0) {
    printf("No Monero phrases found in test file\n");
  }
  TEST_ASSERT(stats.monero_phrases_found > 0);

  printf("✓ Monero file processing test passed\n");
}

// Run all parser tests
bool run_parser_tests(void) {
  UNITY_BEGIN_TEST_SUITE("Parser Tests");

  printf("\n=== Running Parser Tests ===\n");

  // Setup
  test_setup();

  // Explicitly check if the test was initialized correctly
  bool init_result = false;

  if (parser_initialized) {
    init_result = true;
  } else {
    printf("Parser was not initialized correctly\n");

    // Let's try to initialize it here directly to see if it works
    SeedParserConfig config;
    memset(&config, 0, sizeof(config));

    char *wordlist_dir = find_wordlist_dir();
    if (!wordlist_dir) {
      printf("ERROR: Could not find wordlist directory even in "
             "run_parser_tests!\n");
      TEST_ASSERT(0); // Force test to fail
      UNITY_END_TEST_SUITE();
      return false;
    }

    // Setup configuration with English wordlist
    config.wordlist_dir = wordlist_dir;

    // Try initializing the parser again
    init_result = seed_parser_init(&config);
    if (init_result) {
      parser_initialized = true;
      printf("Parser initialized successfully in run_parser_tests\n");
    } else {
      printf("Failed to initialize parser even in run_parser_tests\n");
    }
  }

  TEST_ASSERT(init_result);

  // Run tests
  UNITY_RUN_TEST(test_validate_bip39);
  UNITY_RUN_TEST(test_validate_monero);
  UNITY_RUN_TEST(test_process_file_bip39);
  UNITY_RUN_TEST(test_process_file_monero);

  // Teardown
  test_teardown();

  UNITY_END_TEST_SUITE();
}
