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
#include "wallet.h"  // Added for WalletType

// Maximum number of word chain sizes to support
#define MAX_WORD_CHAIN_COUNT 10

// Maximum number of paths to scan
#define MAX_SCAN_PATHS 100

// Maximum path length if not defined by system
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Default scan path
#define DEFAULT_SCAN_PATH "."

// Maximum length for file paths
#define MAX_FILE_PATH 1024

/**
 * @brief Configuration options for the seed phrase parser
 */
typedef struct {
    size_t thread_count;             // Number of worker threads to use
    const char *source_dir;          // Legacy support - use paths[] instead
    const char *log_dir;             // Directory to store log files
    bool use_database;               // Whether to use a database for lookups
    
    size_t word_chain_sizes[MAX_WORD_CHAIN_COUNT]; // Sizes of word chains to validate
    size_t word_chain_count;         // Number of word chain sizes specified
    
    MnemonicLanguage languages[LANGUAGE_COUNT]; // Languages to enable
    size_t language_count;           // Number of languages enabled
    
    char paths[MAX_SCAN_PATHS][PATH_MAX]; // Paths to scan
    size_t path_count;               // Number of paths to scan
    
    size_t threads;                  // Legacy thread handling - use thread_count instead
    
    // Added fields needed by main.c
    bool recursive;                  // Whether to scan directories recursively
    bool detect_monero;              // Whether to detect Monero seed phrases
    bool fast_mode;                  // Whether to use fast scanning mode
    size_t max_wallets;              // Maximum number of wallet addresses to generate
    char output_file[MAX_FILE_PATH]; // Output file path
    char db_file[MAX_FILE_PATH];     // Database file path
    bool show_performance;           // Whether to show performance metrics
    bool show_cpu_info;              // Whether to show CPU information
    
    // Additional fields needed by seed_parser.c
    const char *db_path;             // Path to the database file (legacy - use db_file instead)
    bool parse_eth;                  // Whether to parse Ethereum private keys
    const char **exwords;            // Array of excluded words
    const char *wordlist_dir;        // Directory containing wordlists
} SeedParserConfig;

/**
 * @brief Statistics from the seed phrase parser
 */
typedef struct {
    size_t files_processed;         // Number of files processed
    size_t lines_processed;         // Number of lines processed
    size_t bytes_processed;         // Number of bytes processed
    size_t files_skipped;           // Number of files skipped
    
    uint64_t phrases_found;         // Legacy - use bip39_phrases_found and monero_phrases_found
    uint64_t bip39_phrases_found;   // Number of BIP-39 seed phrases found
    uint64_t eth_keys_found;        // Number of Ethereum private keys found
    uint64_t monero_phrases_found;  // Number of Monero seed phrases found
    uint64_t errors;                // Number of errors encountered
    
    double elapsed_time;            // Time elapsed during processing (in seconds)
} SeedParserStats;

/**
 * @brief Initialize the seed parser with the given configuration options
 * 
 * @param config Pointer to the configuration options
 * @return int 0 on success, non-zero on failure
 */
int seed_parser_init(const SeedParserConfig *config);

/**
 * @brief Validate a mnemonic phrase
 * 
 * @param mnemonic The mnemonic phrase to validate
 * @param type Pointer to store the type of mnemonic
 * @param language Pointer to store the language of the mnemonic
 * @return true if the mnemonic is valid, false otherwise
 */
bool seed_parser_validate_mnemonic(const char *mnemonic, MnemonicType *type, MnemonicLanguage *language);

/**
 * @brief Generate a wallet address from a seed phrase
 * 
 * @param seed_phrase The seed phrase to use
 * @param wallet_type The type of wallet to generate
 * @param address The buffer to store the wallet address
 * @param address_len The length of the address buffer
 * @return true on success, false on failure
 */
bool seed_parser_generate_wallet_address(const char *seed_phrase, WalletType wallet_type, char *address, size_t address_len);

/**
 * @brief Process a file for seed phrases
 * 
 * @param filename The file to process
 * @return int 0 on success, non-zero on failure
 */
int seed_parser_process_file(const char *filename);

/**
 * @brief Process a single line for seed phrases
 * 
 * @param line The line to process
 * @return true on success, false on failure
 */
bool seed_parser_process_line(const char *line);

/**
 * @brief Clean up the seed parser
 */
void seed_parser_cleanup(void);

/**
 * @brief Start the seed parser processing
 * 
 * @return 0 on success, non-zero on failure
 */
int seed_parser_start(void);

/**
 * @brief Stop the seed parser processing
 */
void seed_parser_stop(void);

/**
 * @brief Get the current statistics from the seed parser
 * 
 * @param stats Pointer to the statistics structure to fill
 */
void seed_parser_get_stats(SeedParserStats *stats);

/**
 * @brief Register a callback for progress updates
 * 
 * @param callback The callback function to register
 */
void seed_parser_register_progress_callback(void (*callback)(const char*, const SeedParserStats*));

/**
 * @brief Register a callback for seed phrase found events
 * 
 * @param callback The callback function to register
 */
void seed_parser_register_seed_found_callback(void (*callback)(const char*, const char*, MnemonicType, MnemonicLanguage, size_t));

/**
 * @brief Check if the seed parser has completed processing
 * 
 * @return true if processing is complete, false otherwise
 */
bool seed_parser_is_complete(void);

/**
 * @brief Handle signals for graceful shutdown
 * 
 * @param signum The signal number
 */
void seed_parser_handle_signal(int signum);

#endif /* SEED_PARSER_H */