#include "../include/unity.h"
#include <stdio.h>
#include <string.h>

// External test functions
bool run_mnemonic_tests(void);
bool run_wallet_tests(void);
bool run_parser_tests(void);

int main(int argc, char *argv[]) {
  TEST_SUITE_BEGIN();
  bool all_passed = true;

  if (argc < 2) {
    // Run all tests
    printf("Running all tests...\n\n");
    all_passed &= run_mnemonic_tests();
    all_passed &= run_wallet_tests();
    all_passed &= run_parser_tests();
  } else {
    // Run specific test suite
    if (strcmp(argv[1], "mnemonic") == 0) {
      printf("Running mnemonic tests...\n\n");
      all_passed = run_mnemonic_tests();
    } else if (strcmp(argv[1], "wallet") == 0) {
      printf("Running wallet tests...\n\n");
      all_passed = run_wallet_tests();
    } else if (strcmp(argv[1], "parser") == 0) {
      printf("Running parser tests...\n\n");
      all_passed = run_parser_tests();
    } else {
      printf("Unknown test suite: %s\n", argv[1]);
      printf("Available test suites: mnemonic, wallet, parser\n");
      return 1;
    }
  }

  printf("\nTest summary: %s\n",
         all_passed ? "ALL PASSED" : "SOME TESTS FAILED");
  TEST_SUITE_END();
}