/**
 * @file unity.c
 * @brief Implementation of the minimalist C testing framework
 */

#include "../include/unity.h"

// Define the global variables here to avoid duplicate symbol errors
int last_tests_run = 0;
int last_tests_passed = 0;
int last_tests_failed = 0;