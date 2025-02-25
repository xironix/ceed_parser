#include "../include/mnemonic.h"
#include "../include/unity.h"
#include <string.h>
#include <stdlib.h>

// Static mnemonic context for tests
static MnemonicContext ctx;
static bool initialized = false;

// Test wordlist paths
static const char *wordlist_paths[] = {
    "../data/english.txt",
    "../data/spanish.txt",
    "../data/monero_english.txt"
};

// Setup function for mnemonic tests
static void test_setup(void) {
    if (!initialized) {
        TEST_ASSERT(mnemonic_init(&ctx, wordlist_paths, 3));
        initialized = true;
    }
}

// Teardown function for mnemonic tests
static void test_teardown(void) {
    // Cleanup will be done at the end of all tests
}

// Test valid BIP-39 mnemonic validation
static void test_valid_bip39_mnemonic(void) {
    const char *valid_mnemonic = "abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about";
    int type = 0;
    
    TEST_ASSERT(mnemonic_validate(&ctx, valid_mnemonic, MNEMONIC_BIP39));
    printf("✓ Valid BIP-39 mnemonic test passed\n");
}

// Test invalid BIP-39 mnemonic validation
static void test_invalid_bip39_mnemonic(void) {
    const char *invalid_mnemonic = "abandon invalid mnemonic phrase that should not validate properly abandon";
    
    TEST_ASSERT(!mnemonic_validate(&ctx, invalid_mnemonic, MNEMONIC_BIP39));
    printf("✓ Invalid BIP-39 mnemonic test passed\n");
}

// Test valid Monero mnemonic validation
static void test_valid_monero_mnemonic(void) {
    const char *valid_monero = "gels zeal lucky jeers irony tamper older pests noggin orange android academy bailed mural tossed accent atlas layout drinks ozone academy academy avatar onset";
    
    TEST_ASSERT(mnemonic_validate(&ctx, valid_monero, MNEMONIC_MONERO));
    printf("✓ Valid Monero mnemonic test passed\n");
}

// Run all mnemonic tests
void run_mnemonic_tests(void) {
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
        mnemonic_cleanup(&ctx);
        initialized = false;
    }
    
    UNITY_END_TEST_SUITE();
} 