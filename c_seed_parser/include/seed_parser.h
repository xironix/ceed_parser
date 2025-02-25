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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "mnemonic.h"

/**
 * @brief Configuration parameters for the seed parser
 */
typedef struct {
    const char **input_paths;       /**< Array of paths to scan */
    size_t input_path_count;        /**< Number of paths in the array */
    const char **wordlist_paths;    /**< Array of wordlist paths */
    size_t wordlist_count;          /**< Number of wordlists */
    int thread_count;               /**< Number of threads to use */
    bool recursive;                 /**< Whether to scan directories recursively */
    char output_file[256];          /**< Path to output file */
    char db_file[256];              /**< Path to database file */
    bool fast_mode;                 /**< Fast mode (fewer wallet types) */
    int max_wallets;                /**< Maximum wallets to generate per seed */
    size_t word_chain_count;        /**< Number of consecutive words required */
    size_t language_count;          /**< Number of languages supported */
    bool show_performance;          /**< Show performance statistics */
    bool show_cpu_info;             /**< Show CPU information */
    
    /* Original fields */
    int threads;                    /**< Number of threads to use for processing */
    bool parse_eth;                 /**< Whether to parse Ethereum private keys */
    size_t chunk_size;              /**< File chunk size for reading */
    int exwords;                    /**< Maximum word repetition allowed in a phrase */
    const char *wordlist_dir;       /**< Directory with BIP39 wordlists */
    bool detect_monero;             /**< Whether to detect Monero seed phrases */
} SeedParserConfig;

/**
 * @brief Statistics collected during parsing
 */
typedef struct {
    size_t files_processed;         /**< Number of files processed */
    size_t files_skipped;           /**< Number of files skipped */
    size_t seeds_found;             /**< Number of seed phrases found */
    size_t wallets_generated;       /**< Number of wallets generated */
    double processing_time;         /**< Total processing time in seconds */
    time_t start_time;              /**< Start time of processing */
    time_t end_time;                /**< End time of processing */
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
 * @param config Configuration for the parser
 * @param stats Pointer to stats structure (or NULL)
 * @return true on success, false on failure
 */
bool seed_parser_init(const SeedParserConfig *config, SeedParserStats *stats);

/**
 * @brief Start the parsing process
 * 
 * @return true on success, false on failure
 */
bool seed_parser_start(void);

/**
 * @brief Stop the parsing process
 * 
 * @return true on success, false on failure
 */
bool seed_parser_stop(void);

/**
 * @brief Clean up resources used by the parser
 */
void seed_parser_cleanup(void);

/**
 * @brief Validate a mnemonic phrase
 * 
 * @param phrase The mnemonic phrase to validate
 * @param type Pointer to store the detected mnemonic type
 * @return true if valid, false otherwise
 */
bool seed_parser_validate_mnemonic(const char *phrase, int *type);

/**
 * @brief Generate wallets from a valid mnemonic
 * 
 * @param phrase The validated mnemonic phrase
 * @param type The mnemonic type
 * @param max_wallets Maximum number of wallets to generate
 * @param fast_mode Whether to use fast mode (fewer wallet types)
 * @return true on success, false on failure
 */
bool seed_parser_generate_wallets(const char *phrase, int type, int max_wallets, bool fast_mode);

/**
 * @brief Process a single file
 * 
 * @param filepath Path to the file
 * @return true on success, false on failure
 */
bool seed_parser_process_file(const char *filepath);

/**
 * @brief Process a directory
 * 
 * @param dirpath Path to the directory
 * @param recursive Whether to process subdirectories
 * @return true on success, false on failure
 */
bool seed_parser_process_directory(const char *dirpath, bool recursive);

/**
 * @brief Print statistics
 * 
 * @param stats The statistics structure
 * @param file File to print to (stdout if NULL)
 */
void seed_parser_print_stats(const SeedParserStats *stats, FILE *file);

/**
 * @brief Handle SIGINT for graceful shutdown
 */
void seed_parser_handle_signal(int signum);

#endif /* SEED_PARSER_H */ 