/**
 * @file seed_parser.h
 * @brief High-performance cryptocurrency seed phrase parser
 *
 * A high-performance C implementation of a cryptocurrency seed phrase parser
 * for scanning directories and extracting cryptocurrency seed phrases and private keys.
 */

#ifndef SEED_PARSER_H
#define SEED_PARSER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "mnemonic.h"

/**
 * @brief Configuration parameters for the seed parser
 */
typedef struct {
    const char *source_dir;         /**< Source directory to scan */
    const char *log_dir;            /**< Directory for log files */
    const char *db_path;            /**< Path to the SQLite database */
    int threads;                    /**< Number of threads to use for processing */
    bool parse_eth;                 /**< Whether to parse Ethereum private keys */
    size_t chunk_size;              /**< File chunk size for reading */
    int exwords;                    /**< Maximum word repetition allowed in a phrase */
    const char *wordlist_dir;       /**< Directory with BIP39 wordlists */
    MnemonicLanguage languages[LANGUAGE_COUNT]; /**< Languages to enable (terminated with LANGUAGE_COUNT) */
    bool detect_monero;             /**< Whether to detect Monero seed phrases */
    size_t word_chain_sizes[10];    /**< Word chain sizes to look for (terminated with 0) */
} SeedParserConfig;

/**
 * @brief Statistics collected during parsing
 */
typedef struct {
    uint64_t files_processed;        /**< Number of files processed */
    uint64_t bytes_processed;        /**< Number of bytes processed */
    uint64_t phrases_found;          /**< Number of seed phrases found */
    uint64_t eth_keys_found;         /**< Number of Ethereum private keys found */
    uint64_t monero_phrases_found;   /**< Number of Monero seed phrases found */
    uint64_t errors;                 /**< Number of errors encountered */
} SeedParserStats;

/**
 * @brief Default configuration values
 * 
 * @param config Pointer to configuration struct to initialize
 * @return 0 on success, non-zero on failure
 */
int seed_parser_config_init(SeedParserConfig *config);

/**
 * @brief Initialize the seed parser
 * 
 * @param config Configuration parameters
 * @return 0 on success, non-zero on failure
 */
int seed_parser_init(const SeedParserConfig *config);

/**
 * @brief Start scanning for seed phrases
 * 
 * @return 0 on success, non-zero on failure
 */
int seed_parser_start(void);

/**
 * @brief Clean up resources used by the seed parser
 */
void seed_parser_cleanup(void);

/**
 * @brief Get current statistics
 * 
 * @param stats Pointer to statistics struct to fill
 */
void seed_parser_get_stats(SeedParserStats *stats);

/**
 * @brief Validate a mnemonic phrase
 * 
 * @param mnemonic The mnemonic phrase to validate
 * @param type Pointer to store the detected mnemonic type
 * @param language Pointer to store the detected language (can be NULL)
 * @return true if valid, false otherwise
 */
bool seed_parser_validate_mnemonic(const char *mnemonic, MnemonicType *type, MnemonicLanguage *language);

/**
 * @brief Process a file looking for seed phrases
 * 
 * @param filepath Path to the file to process
 * @return 0 on success, non-zero on failure
 */
int seed_parser_process_file(const char *filepath);

/**
 * @brief Handle SIGINT for graceful shutdown
 */
void seed_parser_handle_signal(int signum);

#endif /* SEED_PARSER_H */ 