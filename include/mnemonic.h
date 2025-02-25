/**
 * @file mnemonic.h
 * @brief Functionality for working with BIP-39 and other mnemonic phrases
 *
 * This header provides functions for validating and working with mnemonic seed phrases.
 */

#ifndef MNEMONIC_H
#define MNEMONIC_H

#include <stddef.h>
#include <stdbool.h>

// Maximum number of words in a mnemonic phrase
#define MAX_MNEMONIC_WORDS 25

// Mnemonic types
typedef enum {
    MNEMONIC_INVALID = 0,
    MNEMONIC_BIP39 = 1,
    MNEMONIC_MONERO = 2
} MnemonicType;

// Supported languages
typedef enum {
    LANGUAGE_ENGLISH = 0,
    LANGUAGE_SPANISH = 1, 
    LANGUAGE_FRENCH = 2,
    LANGUAGE_ITALIAN = 3,
    LANGUAGE_PORTUGUESE = 4,
    LANGUAGE_CZECH = 5,
    LANGUAGE_JAPANESE = 6,
    LANGUAGE_CHINESE_SIMPLIFIED = 7,
    LANGUAGE_KOREAN = 8,
    LANGUAGE_CHINESE_TRADITIONAL = 9,
    LANGUAGE_MONERO_ENGLISH = 10,
    LANGUAGE_COUNT = 11
} MnemonicLanguage;

/**
 * Get human-readable name for a language
 *
 * @param language The language ID
 * @return The language name
 */
const char *mnemonic_language_name(MnemonicLanguage language);

/**
 * Structure for a wordlist
 */
typedef struct {
    char **words;                // Array of words
    size_t word_count;           // Number of words in the list
    MnemonicLanguage language;   // Language of the wordlist
} Wordlist;

/**
 * Structure for mnemonic context
 */
typedef struct {
    Wordlist **wordlists;        // Array of wordlists
    size_t wordlist_count;       // Number of loaded wordlists
    bool initialized;            // Whether the context is initialized
} MnemonicContext;

/**
 * Initialize the mnemonic subsystem
 *
 * @return 0 on success, non-zero on failure
 */
int mnemonic_init(void);

/**
 * Load a wordlist from a file
 *
 * @param language The language of the wordlist
 * @return 0 on success, non-zero on failure
 */
int mnemonic_load_wordlist(MnemonicLanguage language);

/**
 * Validate a mnemonic phrase
 *
 * @param phrase The mnemonic phrase to validate
 * @param type Output parameter to store the detected type
 * @param language Output parameter to store the detected language (can be NULL)
 * @return true if valid, false otherwise
 */
bool mnemonic_validate(const char *phrase, MnemonicType *type, MnemonicLanguage *language);

/**
 * Clean up the mnemonic subsystem
 */
void mnemonic_cleanup(void);

/**
 * Convert a mnemonic phrase to a binary seed
 *
 * @param phrase The mnemonic phrase
 * @param passphrase Optional passphrase (can be NULL)
 * @param seed Output buffer for the seed
 * @param seed_len Size of the output buffer
 * @return Actual length of the seed in bytes, or 0 on failure
 */
size_t mnemonic_to_seed(const char *phrase, const char *passphrase, 
                       unsigned char *seed, size_t seed_len);

#endif /* MNEMONIC_H */ 