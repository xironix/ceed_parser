#include "../include/unity.h"
#include <stdio.h>
#include <string.h>

// External test functions
extern bool run_mnemonic_tests(void);
extern bool run_wallet_tests(void);
extern bool run_parser_tests(void);

// Structure to track test suite results
typedef struct {
  const char *name;
  bool passed;
  int assertions_run;
  int assertions_passed;
  int assertions_failed;
} TestSuiteResult;

int main(int argc, char *argv[]) {
  TEST_SUITE_BEGIN();
  bool all_passed = true;

  // Array to store results of each test suite
  TestSuiteResult results[3] = {{"Mnemonic Tests", false, 0, 0, 0},
                                {"Wallet Tests", false, 0, 0, 0},
                                {"Parser Tests", false, 0, 0, 0}};
  int suite_count = 0;
  int suites_passed = 0;

  if (argc < 2) {
    // Run all tests
    printf("Running all tests...\n\n");

    // Run each test suite and store results
    results[0].passed = run_mnemonic_tests();
    results[0].assertions_run = unity_get_last_run_count();
    results[0].assertions_passed = unity_get_last_passed_count();
    results[0].assertions_failed = unity_get_last_failed_count();
    printf("[DEBUG] After Mnemonic Tests: run=%d, passed=%d, failed=%d\n",
           results[0].assertions_run, results[0].assertions_passed,
           results[0].assertions_failed);
    suite_count++;
    if (results[0].passed)
      suites_passed++;

    results[1].passed = run_wallet_tests();
    results[1].assertions_run = unity_get_last_run_count();
    results[1].assertions_passed = unity_get_last_passed_count();
    results[1].assertions_failed = unity_get_last_failed_count();
    printf("[DEBUG] After Wallet Tests: run=%d, passed=%d, failed=%d\n",
           results[1].assertions_run, results[1].assertions_passed,
           results[1].assertions_failed);
    suite_count++;
    if (results[1].passed)
      suites_passed++;

    results[2].passed = run_parser_tests();
    results[2].assertions_run = unity_get_last_run_count();
    results[2].assertions_passed = unity_get_last_passed_count();
    results[2].assertions_failed = unity_get_last_failed_count();
    printf("[DEBUG] After Parser Tests: run=%d, passed=%d, failed=%d\n",
           results[2].assertions_run, results[2].assertions_passed,
           results[2].assertions_failed);
    suite_count++;
    if (results[2].passed)
      suites_passed++;

    all_passed = results[0].passed && results[1].passed && results[2].passed;
  } else {
    // Run specific test suite
    if (strcmp(argv[1], "mnemonic") == 0) {
      printf("Running mnemonic tests...\n\n");
      results[0].name = "Mnemonic Tests";
      results[0].passed = run_mnemonic_tests();
      results[0].assertions_run = unity_get_last_run_count();
      results[0].assertions_passed = unity_get_last_passed_count();
      results[0].assertions_failed = unity_get_last_failed_count();
      suite_count = 1;
      if (results[0].passed)
        suites_passed++;
      all_passed = results[0].passed;
    } else if (strcmp(argv[1], "wallet") == 0) {
      printf("Running wallet tests...\n\n");
      results[0].name = "Wallet Tests";
      results[0].passed = run_wallet_tests();
      results[0].assertions_run = unity_get_last_run_count();
      results[0].assertions_passed = unity_get_last_passed_count();
      results[0].assertions_failed = unity_get_last_failed_count();
      suite_count = 1;
      if (results[0].passed)
        suites_passed++;
      all_passed = results[0].passed;
    } else if (strcmp(argv[1], "parser") == 0) {
      printf("Running parser tests...\n\n");
      results[0].name = "Parser Tests";
      results[0].passed = run_parser_tests();
      results[0].assertions_run = unity_get_last_run_count();
      results[0].assertions_passed = unity_get_last_passed_count();
      results[0].assertions_failed = unity_get_last_failed_count();
      suite_count = 1;
      if (results[0].passed)
        suites_passed++;
      all_passed = results[0].passed;
    } else {
      printf("Unknown test suite: %s\n", argv[1]);
      printf("Available test suites: mnemonic, wallet, parser\n");
      return 1;
    }
  }

  // Calculate totals for summary
  int total_assertions_run = 0;
  int total_assertions_passed = 0;
  int total_assertions_failed = 0;

  for (int i = 0; i < suite_count; i++) {
    total_assertions_run += results[i].assertions_run;
    total_assertions_passed += results[i].assertions_passed;
    total_assertions_failed += results[i].assertions_failed;
  }

  // Print test suite summary
  printf("\n╔═══════════════════════════════════════════════════╗\n");
  printf("║                TEST SUITE SUMMARY                 ║\n");
  printf("╠═══════════════════════════════════════════════════╣\n");

  // Print per-suite results directly from our result array
  for (int i = 0; i < suite_count; i++) {
    float pass_rate = results[i].assertions_run > 0
                          ? (float)results[i].assertions_passed /
                                results[i].assertions_run * 100.0f
                          : 0.0f;

    printf("║ %s: %s%*s ║\n", results[i].name,
           results[i].passed ? "✅ PASSED" : "❌ FAILED",
           (int)(45 - strlen(results[i].name) - (results[i].passed ? 9 : 9)),
           "");

    printf("║   Assertions: %3d run, %3d passed, %3d failed (%5.1f%%)  ║\n",
           results[i].assertions_run, results[i].assertions_passed,
           results[i].assertions_failed, pass_rate);
  }

  // Calculate pass rate for overall summary
  float pass_rate = 0.0f;
  if (total_assertions_run > 0) {
    pass_rate = (float)total_assertions_passed / total_assertions_run * 100.0f;
  }

  printf("╠═══════════════════════════════════════════════════╣\n");
  printf("║ OVERALL: %s%*s ║\n", all_passed ? "✅ PASSED" : "❌ FAILED",
         all_passed ? 39 : 39, "");
  printf(
      "║ Test Suites: %d/%d passed%*s ║\n", suites_passed, suite_count,
      (int)(35 - (suites_passed >= 10 ? 2 : 1) - (suite_count >= 10 ? 2 : 1)),
      "");
  printf("║ Assertions: %3d total, %3d passed, %3d failed (%5.1f%%) ║\n",
         total_assertions_run, total_assertions_passed, total_assertions_failed,
         pass_rate);
  printf("╚═══════════════════════════════════════════════════╝\n");

  TEST_SUITE_END();

  // Return appropriate exit code
  return all_passed ? 0 : 1;
}
