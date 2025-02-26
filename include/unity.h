/**
 * @file unity.h
 * @brief Minimalist C testing framework
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Test context structure
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    const char *current_suite;
} UnityContext;

// Global test context
static UnityContext unity_ctx = {0};

// Global variables to store the last test suite results
extern int last_tests_run;
extern int last_tests_passed;
extern int last_tests_failed;

// Accessor functions
static inline int unity_get_run_count(void) { return unity_ctx.tests_run; }
static inline int unity_get_passed_count(void) { return unity_ctx.tests_passed; }
static inline int unity_get_failed_count(void) { return unity_ctx.tests_failed; }

// Functions to get the last test suite results
static inline int unity_get_last_run_count(void) { return last_tests_run; }
static inline int unity_get_last_passed_count(void) { return last_tests_passed; }
static inline int unity_get_last_failed_count(void) { return last_tests_failed; }

// Macros for test assertions
#define TEST_ASSERT(condition) \
    do { \
        unity_ctx.tests_run++; \
        if (!(condition)) { \
            printf("❌ FAIL: %s\n", #condition); \
            unity_ctx.tests_failed++; \
        } else { \
            unity_ctx.tests_passed++; \
            printf("✓ PASS: %s\n", #condition); \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        unity_ctx.tests_run++; \
        if ((expected) != (actual)) { \
            printf("❌ FAIL: %s == %s (expected: %d, actual: %d)\n", \
                   #expected, #actual, (int)(expected), (int)(actual)); \
            unity_ctx.tests_failed++; \
        } else { \
            unity_ctx.tests_passed++; \
            printf("✓ PASS: %s == %s (%d)\n", #expected, #actual, (int)(expected)); \
        } \
    } while (0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    do { \
        unity_ctx.tests_run++; \
        if (strcmp((expected), (actual)) != 0) { \
            printf("❌ FAIL: %s == %s (expected: %s, actual: %s)\n", \
                   #expected, #actual, (expected), (actual)); \
            unity_ctx.tests_failed++; \
        } else { \
            unity_ctx.tests_passed++; \
            printf("✓ PASS: %s == %s (%s)\n", #expected, #actual, (expected)); \
        } \
    } while (0)

// Macros for test suite management
#define UNITY_BEGIN_TEST_SUITE(name) \
    do { \
        unity_ctx.current_suite = name; \
        unity_ctx.tests_run = 0; \
        unity_ctx.tests_passed = 0; \
        unity_ctx.tests_failed = 0; \
        printf("\n==== Starting Test Suite: %s ====\n", name); \
    } while (0)

#define UNITY_RUN_TEST(test_func) \
    do { \
        printf("\nRunning test: %s\n", #test_func); \
        test_func(); \
    } while (0)

#define UNITY_END_TEST_SUITE() \
    do { \
        printf("\n---- End of Test Suite: %s ----\n", unity_ctx.current_suite); \
        printf("[DEBUG] Test stats: run=%d, passed=%d, failed=%d\n", \
               unity_ctx.tests_run, unity_ctx.tests_passed, unity_ctx.tests_failed); \
        last_tests_run = unity_ctx.tests_run; \
        last_tests_passed = unity_ctx.tests_passed; \
        last_tests_failed = unity_ctx.tests_failed; \
        bool result = unity_ctx.tests_failed == 0; \
        return result; \
    } while (0)

// Main test suite macros
#define TEST_SUITE_BEGIN() \
    do { \
        printf("\n==== Starting Test Suite Runner ====\n"); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf("\n==== Test Suite Runner Complete ====\n"); \
    } while (0)

#endif /* UNITY_H */ 
