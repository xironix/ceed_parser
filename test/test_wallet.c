#include "../include/unity.h"
#include "../include/wallet.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Track test environment setup status
static bool test_env_ready = false;

// Setup function to ensure test environment is ready
static void test_setup(void) {
  test_env_ready = true;
  printf("Wallet test environment initialized\n");
}

// Helper function to validate a Bitcoin address format
static bool is_valid_bitcoin_address(const char *address) {
  if (!address)
    return false;

  // Very basic validation - just check length and starting characters
  // In a real implementation, this would be more comprehensive
  size_t len = strlen(address);
  return (len >= 26 && len <= 35) && (address[0] == '1' || address[0] == '3' ||
                                      (address[0] == 'b' && address[1] == 'c'));
}

// Helper function to validate an Ethereum address format
static bool is_valid_ethereum_address(const char *address) {
  if (!address)
    return false;

  // Basic Ethereum address validation - should be 42 chars, starting with 0x
  // followed by 40 hex characters
  if (strlen(address) != 42 || address[0] != '0' || address[1] != 'x') {
    return false;
  }

  // Check if remaining characters are hex
  for (int i = 2; i < 42; i++) {
    if (!isxdigit((unsigned char)address[i])) {
      return false;
    }
  }

  return true;
}

// Test Bitcoin wallet generation
static void test_bitcoin_wallet_generation(void) {
  if (!test_env_ready) {
    printf("Skipping test_bitcoin_wallet_generation due to initialization "
           "failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *seed_phrase = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  Wallet wallet;
  memset(&wallet, 0, sizeof(wallet)); // Initialize wallet to zeros

  // Check result and provide better error reporting
  bool result = wallet_generate_from_seed(seed_phrase, WALLET_TYPE_BITCOIN,
                                          NULL, &wallet);
  if (!result) {
    printf("Failed to generate Bitcoin wallet from seed phrase\n");
  }
  TEST_ASSERT(result);

  // Check wallet type
  if (wallet.type != WALLET_TYPE_BITCOIN) {
    printf("Expected wallet type %d but got %d\n", WALLET_TYPE_BITCOIN,
           wallet.type);
  }
  TEST_ASSERT_EQUAL(WALLET_TYPE_BITCOIN, wallet.type);

  // Check address count
  if (wallet.address_count <= 0) {
    printf("No addresses generated for Bitcoin wallet\n");
  }
  TEST_ASSERT(wallet.address_count > 0);

  // Check address validity
  if (!is_valid_bitcoin_address(wallet.addresses[0])) {
    printf("Invalid Bitcoin address format: %s\n",
           wallet.addresses[0] ? wallet.addresses[0] : "NULL");
  }
  TEST_ASSERT(is_valid_bitcoin_address(wallet.addresses[0]));

  printf("✓ Bitcoin wallet test passed\n");
}

// Test Ethereum wallet generation
static void test_ethereum_wallet_generation(void) {
  if (!test_env_ready) {
    printf("Skipping test_ethereum_wallet_generation due to initialization "
           "failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *seed_phrase = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  Wallet wallet;
  memset(&wallet, 0, sizeof(wallet)); // Initialize wallet to zeros

  // Check result and provide better error reporting
  bool result = wallet_generate_from_seed(seed_phrase, WALLET_TYPE_ETHEREUM,
                                          NULL, &wallet);
  if (!result) {
    printf("Failed to generate Ethereum wallet from seed phrase\n");
  }
  TEST_ASSERT(result);

  // Check wallet type
  if (wallet.type != WALLET_TYPE_ETHEREUM) {
    printf("Expected wallet type %d but got %d\n", WALLET_TYPE_ETHEREUM,
           wallet.type);
  }
  TEST_ASSERT_EQUAL(WALLET_TYPE_ETHEREUM, wallet.type);

  // Check address count
  if (wallet.address_count <= 0) {
    printf("No addresses generated for Ethereum wallet\n");
  }
  TEST_ASSERT(wallet.address_count > 0);

  // Check address validity
  if (!is_valid_ethereum_address(wallet.addresses[0])) {
    printf("Invalid Ethereum address format: %s\n",
           wallet.addresses[0] ? wallet.addresses[0] : "NULL");
  }
  TEST_ASSERT(is_valid_ethereum_address(wallet.addresses[0]));

  printf("✓ Ethereum wallet test passed\n");
}

// Test Monero wallet generation
static void test_monero_wallet_generation(void) {
  if (!test_env_ready) {
    printf("Skipping test_monero_wallet_generation due to initialization "
           "failure\n");
    TEST_ASSERT(0); // Force test to fail
    return;
  }

  const char *seed_phrase =
      "gels zeal lucky jeers irony tamper older pests noggin orange android "
      "academy bailed mural tossed accent atlas layout drinks ozone academy "
      "academy avatar onset";
  Wallet wallet;
  memset(&wallet, 0, sizeof(wallet)); // Initialize wallet to zeros

  // Check result and provide better error reporting
  bool result =
      wallet_generate_from_seed(seed_phrase, WALLET_TYPE_MONERO, NULL, &wallet);
  if (!result) {
    printf("Failed to generate Monero wallet from seed phrase\n");
  }
  TEST_ASSERT(result);

  // Check wallet type
  if (wallet.type != WALLET_TYPE_MONERO) {
    printf("Expected wallet type %d but got %d\n", WALLET_TYPE_MONERO,
           wallet.type);
  }
  TEST_ASSERT_EQUAL(WALLET_TYPE_MONERO, wallet.type);

  // Check address count
  if (wallet.address_count <= 0) {
    printf("No addresses generated for Monero wallet\n");
  }
  TEST_ASSERT(wallet.address_count > 0);

  // Check address length
  if (wallet.addresses[0] == NULL || strlen(wallet.addresses[0]) == 0) {
    printf("Empty Monero wallet address\n");
  }
  TEST_ASSERT(strlen(wallet.addresses[0]) > 0);

  printf("✓ Monero wallet test passed\n");
}

// Run all wallet tests
bool run_wallet_tests(void) {
  UNITY_BEGIN_TEST_SUITE("Wallet Tests");

  printf("\n=== Running Wallet Tests ===\n");

  // Setup test environment
  test_setup();

  // Run tests
  UNITY_RUN_TEST(test_bitcoin_wallet_generation);
  UNITY_RUN_TEST(test_ethereum_wallet_generation);
  UNITY_RUN_TEST(test_monero_wallet_generation);

  UNITY_END_TEST_SUITE();
}
