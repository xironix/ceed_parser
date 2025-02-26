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

// Macros for test assertions
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("FAIL: %s\n", #condition); \
            unity_ctx.tests_failed++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("FAIL: %s == %s (expected: %d, actual: %d)\n", \
                   #expected, #actual, (int)(expected), (int)(actual)); \
            unity_ctx.tests_failed++; \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_STRING_EQUAL(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("FAIL: %s == %s (expected: %s, actual: %s)\n", \
                   #expected, #actual, (expected), (actual)); \
            unity_ctx.tests_failed++; \
            return; \
        } \
    } while (0)

// Macros for test suite management
#define UNITY_BEGIN_TEST_SUITE(name) \
    do { \
        unity_ctx.current_suite = name; \
        unity_ctx.tests_run = 0; \
        unity_ctx.tests_passed = 0; \
        unity_ctx.tests_failed = 0; \
        printf("\n---- Starting Test Suite: %s ----\n", name); \
    } while (0)

#define UNITY_RUN_TEST(test_func) \
    do { \
        unity_ctx.tests_run++; \
        test_func(); \
        if (unity_ctx.tests_failed == 0 || \
            (unity_ctx.tests_failed != unity_ctx.tests_run)) { \
            unity_ctx.tests_passed++; \
        } \
    } while (0)

#define UNITY_END_TEST_SUITE() \
    do { \
        printf("\n---- Test Suite Results: %s ----\n", unity_ctx.current_suite); \
        printf("Tests Run: %d\n", unity_ctx.tests_run); \
        printf("Tests Passed: %d\n", unity_ctx.tests_passed); \
        printf("Tests Failed: %d\n", unity_ctx.tests_failed); \
        printf("----------------------------------\n"); \
        return unity_ctx.tests_failed == 0; \
    } while (0)

// Main test suite macros
#define TEST_SUITE_BEGIN() \
    do { \
        printf("\n==== Starting Test Suite Runner ====\n"); \
    } while (0)

#define TEST_SUITE_END() \
    do { \
        printf("\n==== Test Suite Runner Complete ====\n"); \
        return 0; \
    } while (0)

#endif /* UNITY_H */ 