/**
 * @file mnemonic.h
 * @brief Cryptocurrency mnemonic phrase functionality
 *
 * Functionality for validating and processing BIP-39 mnemonic phrases
 * and Monero seed phrases.
 */

#ifndef MNEMONIC_H
#define MNEMONIC_H

#include <stdbool.h>
#include <stddef.h>

// Mnemonic types
#define MNEMONIC_BIP39 1
#define MNEMONIC_MONERO 2

// Language identifiers
#define LANGUAGE_ENGLISH 1
#define LANGUAGE_SPANISH 2
#define LANGUAGE_MONERO_ENGLISH 3

// Maximum number of words in a mnemonic phrase
#define MAX_MNEMONIC_WORDS 24

// Maximum length of a word in a wordlist
#define MAX_WORD_LENGTH 16

// Structure to hold wordlist data
typedef struct {
    char **words;
    size_t count;
    int language_id;
} Wordlist;

// Structure for mnemonic context
typedef struct {
    Wordlist **wordlists;
    size_t wordlist_count;
    bool initialized;
} MnemonicContext;

/**
 * Initialize the mnemonic context with specified wordlists
 * 
 * @param ctx The mnemonic context to initialize
 * @param wordlist_paths Array of file paths to wordlists
 * @param count Number of wordlists
 * @return true if initialization succeeded, false otherwise
 */
bool mnemonic_init(MnemonicContext *ctx, const char **wordlist_paths, size_t count);

/**
 * Load a wordlist from a file
 * 
 * @param filename Path to the wordlist file
 * @param language_id Identifier for the language
 * @return Pointer to the loaded wordlist, or NULL on failure
 */
Wordlist *mnemonic_load_wordlist(const char *filename, int language_id);

/**
 * Validate a mnemonic phrase against loaded wordlists
 * 
 * @param ctx The mnemonic context
 * @param phrase The mnemonic phrase to validate
 * @param mnemonic_type Type of mnemonic (BIP-39 or Monero)
 * @return true if the mnemonic is valid, false otherwise
 */
bool mnemonic_validate(MnemonicContext *ctx, const char *phrase, int mnemonic_type);

/**
 * Clean up and free resources used by the mnemonic context
 * 
 * @param ctx The mnemonic context to clean up
 */
void mnemonic_cleanup(MnemonicContext *ctx);

/**
 * Free a loaded wordlist
 * 
 * @param wordlist The wordlist to free
 */
void mnemonic_free_wordlist(Wordlist *wordlist);

#endif /* MNEMONIC_H */ 