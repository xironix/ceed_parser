/**
 * @file seed_parser_optimized.h
 * @brief High-performance interface for seed phrase parsing and validation
 *
 * This file provides the interface for the optimized implementation of the seed parser
 * using various performance techniques like SIMD instructions,
 * thread pooling, memory pooling, and caching.
 */

#ifndef SEED_PARSER_OPTIMIZED_H
#define SEED_PARSER_OPTIMIZED_H

#include <stdbool.h>
#include <stddef.h>
#include "../include/cache.h"
#include "../include/mnemonic.h"
#include "../include/seed_parser.h"

// Maximum number of words in a seed phrase
#define MAX_WORDS 25

// Use the language type from mnemonic.h
typedef MnemonicLanguage language_t;

// Supported wallet types
typedef enum {
    WALLET_BITCOIN = 0,
    WALLET_ETHEREUM,
    WALLET_LITECOIN,
    WALLET_MONERO,
    WALLET_COUNT
} wallet_t;

// Wallet address structure
typedef struct {
    char address[128];          // Wallet address
    wallet_t type;              // Wallet type
    unsigned int index;         // Address index
} wallet_address_t;

// Validation result structure
typedef struct {
    bool is_valid;                       // Whether the phrase is valid
    size_t word_count;                   // Number of words in the phrase
    char* words[MAX_WORDS];              // Array of words
    size_t invalid_word_indices[MAX_WORDS]; // Indices of invalid words
    size_t invalid_count;                // Number of invalid words
    language_t language;                 // Detected language
} validation_result_t;

/**
 * @brief Initialize the optimized seed parser
 * 
 * @param config Pointer to configuration structure
 * @return true if initialization was successful
 */
bool seed_parser_opt_init(const SeedParserConfig *config);

/**
 * @brief Clean up the optimized seed parser
 */
void seed_parser_opt_cleanup(void);

/**
 * @brief Load all wordlists with SIMD and bloom filter optimizations
 * 
 * @param directory Directory containing wordlist files
 * @return true if all wordlists were loaded successfully
 */
bool seed_parser_opt_load_wordlists(const char* directory);

/**
 * @brief Parse and validate a seed phrase using optimized methods
 * 
 * @param phrase Seed phrase to validate
 * @param result Pointer to a validation_result_t structure to fill
 * @return true if the phrase is valid
 */
bool seed_parser_opt_validate_phrase(const char* phrase, validation_result_t* result);

/**
 * @brief Generate wallet addresses from a seed phrase with optimized methods
 * 
 * @param phrase Seed phrase to generate addresses from
 * @param wallet_type Type of wallet to generate addresses for
 * @param count Number of addresses to generate
 * @param addresses Array to store the generated addresses
 * @return Number of addresses generated
 */
size_t seed_parser_opt_generate_addresses(const char* phrase, wallet_t wallet_type, 
                                     unsigned int count, wallet_address_t* addresses);

/**
 * @brief Get SIMD capabilities string
 * 
 * @return String describing the available SIMD capabilities
 */
const char* seed_parser_opt_get_simd_capabilities(void);

/**
 * @brief Get thread pool information
 * 
 * @param num_threads Pointer to store the number of threads
 * @param tasks_queued Pointer to store the number of tasks queued
 * @param tasks_completed Pointer to store the number of tasks completed
 */
void seed_parser_opt_get_thread_stats(size_t* num_threads, size_t* tasks_queued, size_t* tasks_completed);

/**
 * @brief Get cache statistics
 * 
 * @param wordlist_stats Pointer to store wordlist cache statistics
 * @param address_stats Pointer to store address cache statistics
 */
void seed_parser_opt_get_cache_stats(cache_stats_t* wordlist_stats, cache_stats_t* address_stats);

/**
 * @brief Get memory pool statistics
 * 
 * @param total_allocated Pointer to store the total memory allocated
 * @param total_used Pointer to store the total memory used
 * @param block_size Pointer to store the block size
 * @param block_count Pointer to store the number of blocks
 */
void seed_parser_opt_get_memory_pool_stats(size_t* total_allocated, size_t* total_used,
                                      size_t* block_size, size_t* block_count);

#endif // SEED_PARSER_OPTIMIZED_H 
