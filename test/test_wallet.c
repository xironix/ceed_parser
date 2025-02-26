#include "../include/unity.h"
#include "../include/wallet.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Helper function to validate a Bitcoin address format
static bool is_valid_bitcoin_address(const char *address) {
  // Very basic validation - just check length and starting characters
  // In a real implementation, this would be more comprehensive
  size_t len = strlen(address);
  return (len >= 26 && len <= 35) && (address[0] == '1' || address[0] == '3' ||
                                      (address[0] == 'b' && address[1] == 'c'));
}

// Helper function to validate an Ethereum address format
static bool is_valid_ethereum_address(const char *address) {
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
  const char *seed_phrase = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  Wallet wallet;

  TEST_ASSERT(wallet_generate_from_seed(seed_phrase, WALLET_TYPE_BITCOIN, NULL,
                                        &wallet));
  TEST_ASSERT_EQUAL(WALLET_TYPE_BITCOIN, wallet.type);
  TEST_ASSERT(wallet.address_count > 0);
  TEST_ASSERT(is_valid_bitcoin_address(wallet.addresses[0]));

  printf("✓ Bitcoin wallet test passed\n");
}

// Test Ethereum wallet generation
static void test_ethereum_wallet_generation(void) {
  const char *seed_phrase = "abandon abandon abandon abandon abandon abandon "
                            "abandon abandon abandon abandon abandon about";
  Wallet wallet;

  TEST_ASSERT(wallet_generate_from_seed(seed_phrase, WALLET_TYPE_ETHEREUM, NULL,
                                        &wallet));
  TEST_ASSERT_EQUAL(WALLET_TYPE_ETHEREUM, wallet.type);
  TEST_ASSERT(wallet.address_count > 0);
  TEST_ASSERT(is_valid_ethereum_address(wallet.addresses[0]));

  printf("✓ Ethereum wallet test passed\n");
}

// Test Monero wallet generation
static void test_monero_wallet_generation(void) {
  const char *seed_phrase =
      "gels zeal lucky jeers irony tamper older pests noggin orange android "
      "academy bailed mural tossed accent atlas layout drinks ozone academy "
      "academy avatar onset";
  Wallet wallet;

  TEST_ASSERT(wallet_generate_from_seed(seed_phrase, WALLET_TYPE_MONERO, NULL,
                                        &wallet));
  TEST_ASSERT_EQUAL(WALLET_TYPE_MONERO, wallet.type);
  TEST_ASSERT(wallet.address_count > 0);
  TEST_ASSERT(strlen(wallet.addresses[0]) > 0);

  printf("✓ Monero wallet test passed\n");
}

// Run all wallet tests
bool run_wallet_tests(void) {
  UNITY_BEGIN_TEST_SUITE("Wallet Tests");

  printf("\n=== Running Wallet Tests ===\n");

  UNITY_RUN_TEST(test_bitcoin_wallet_generation);
  UNITY_RUN_TEST(test_ethereum_wallet_generation);
  UNITY_RUN_TEST(test_monero_wallet_generation);

  UNITY_END_TEST_SUITE();
}