#include "../include/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for test suites
extern void run_mnemonic_tests(void);
extern void run_wallet_tests(void);
extern void run_parser_tests(void);
extern void run_memory_tests(void);

// Define the global debug flag needed by other modules
extern bool g_debug_enabled;

// Test statistics
typedef struct {
  int run;
  int passed;
  int failed;
} TestStats;

// Global test statistics
static TestStats g_stats = {0, 0, 0};
static TestStats g_suite_stats = {0, 0, 0};
static const char *g_current_suite = NULL;

// Reset suite statistics
void reset_suite_stats(void) {
  g_suite_stats.run = 0;
  g_suite_stats.passed = 0;
  g_suite_stats.failed = 0;
}

// Update global statistics with suite statistics
void update_global_stats(void) {
  g_stats.run += g_suite_stats.run;
  g_stats.passed += g_suite_stats.passed;
  g_stats.failed += g_suite_stats.failed;
}

// Print test suite header
void print_suite_header(const char *suite_name) {
  g_current_suite = suite_name;
  printf("\n\n==== Starting Test Suite: %s ====\n\n", suite_name);
  printf("=== Running %s ===\n", suite_name);
}

// Print test suite footer with statistics
void print_suite_footer(void) {
  printf("\n---- End of Test Suite: %s ----\n", g_current_suite);
  printf("[DEBUG] Test stats: run=%d, passed=%d, failed=%d\n",
         g_suite_stats.run, g_suite_stats.passed, g_suite_stats.failed);

  if (g_current_suite) {
    printf("[DEBUG] After %s: run=%d, passed=%d, failed=%d\n", g_current_suite,
           g_stats.run, g_stats.passed, g_stats.failed);
  }
}

// Custom test function type
typedef void (*TestFunction)(void);

// Custom test runner that tracks statistics
void custom_test_runner(TestFunction test) {
  g_suite_stats.run++;

  // Run the test and check if it failed
  int prev_failed = unity_get_failed_count();
  test();
  int curr_failed = unity_get_failed_count();

  if (curr_failed > prev_failed) {
    g_suite_stats.failed++;
  } else {
    g_suite_stats.passed++;
  }
}

// Print overall test summary
void print_test_summary(void) {
  float pass_percentage =
      g_stats.run > 0 ? (float)g_stats.passed / g_stats.run * 100.0f : 0.0f;

  printf("\n╔═══════════════════════════════════════════════════╗\n");
  printf("║                TEST SUITE SUMMARY                 ║\n");
  printf("╠═══════════════════════════════════════════════════╣\n");

  // Print individual suite results
  if (g_stats.run > 0) {
    printf("║ Mnemonic Tests: %s                       ║\n",
           g_stats.failed > 0 ? "❌ FAILED" : "✅ PASSED");
    printf("║   Assertions: %3d run, %3d passed, %3d failed (%5.1f%%)  ║\n",
           g_suite_stats.run, g_suite_stats.passed, g_suite_stats.failed,
           g_suite_stats.run > 0
               ? (float)g_suite_stats.passed / g_suite_stats.run * 100.0f
               : 0.0f);
  }

  printf("╠═══════════════════════════════════════════════════╣\n");
  printf("║ OVERALL: %s                                        ║\n",
         g_stats.failed > 0 ? "❌ FAILED" : "✅ PASSED");
  printf("║ Test Suites: %d/%d passed                                  ║\n",
         g_stats.failed > 0 ? 0 : 1, 1);
  printf("║ Assertions: %3d total, %3d passed, %3d failed (%5.1f%%) ║\n",
         g_stats.run, g_stats.passed, g_stats.failed, pass_percentage);
  printf("╚═══════════════════════════════════════════════════╝\n");
}

// Main test runner
int main(int argc, char **argv) {
  printf("\n==== Starting Test Suite Runner ====\n");

  // Initialize debug flag
  g_debug_enabled = true;

  // Determine which tests to run based on command line arguments
  if (argc > 1) {
    if (strcmp(argv[1], "mnemonic") == 0) {
      printf("Running mnemonic tests...\n");
      reset_suite_stats();
      run_mnemonic_tests();
      update_global_stats();
    } else if (strcmp(argv[1], "wallet") == 0) {
      printf("Running wallet tests...\n");
      reset_suite_stats();
      run_wallet_tests();
      update_global_stats();
    } else if (strcmp(argv[1], "parser") == 0) {
      printf("Running parser tests...\n");
      reset_suite_stats();
      run_parser_tests();
      update_global_stats();
    } else if (strcmp(argv[1], "memory") == 0) {
      printf("Running memory tests...\n");
      reset_suite_stats();
      run_memory_tests();
      update_global_stats();
    } else {
      printf("Unknown test suite: %s\n", argv[1]);
      return 1;
    }
  } else {
    printf("Running all tests...\n");

    // Run all test suites
    reset_suite_stats();
    run_mnemonic_tests();
    update_global_stats();

    reset_suite_stats();
    run_wallet_tests();
    update_global_stats();

    reset_suite_stats();
    run_parser_tests();
    update_global_stats();

    reset_suite_stats();
    run_memory_tests();
    update_global_stats();
  }

  // Print overall summary
  print_test_summary();

  printf("\n==== Test Suite Runner Complete ====\n");

  // Return failure if any tests failed
  return g_stats.failed > 0 ? 1 : 0;
}
