#include "../include/unity.h"
#include <stdio.h>
#include <string.h>

// External test functions
void run_mnemonic_tests(void);
void run_wallet_tests(void);
void run_parser_tests(void);

int main(int argc, char *argv[]) {
    TEST_SUITE_BEGIN();
    
    if (argc < 2) {
        // Run all tests
        printf("Running all tests...\n\n");
        run_mnemonic_tests();
        run_wallet_tests();
        run_parser_tests();
    } else {
        // Run specific test suite
        if (strcmp(argv[1], "mnemonic") == 0) {
            printf("Running mnemonic tests...\n\n");
            run_mnemonic_tests();
        } else if (strcmp(argv[1], "wallet") == 0) {
            printf("Running wallet tests...\n\n");
            run_wallet_tests();
        } else if (strcmp(argv[1], "parser") == 0) {
            printf("Running parser tests...\n\n");
            run_parser_tests();
        } else {
            printf("Unknown test suite: %s\n", argv[1]);
            printf("Available test suites: mnemonic, wallet, parser\n");
            return 1;
        }
    }
    
    TEST_SUITE_END();
} 