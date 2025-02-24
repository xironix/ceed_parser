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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Maximum supported word list size
 */
#define MAX_WORDLIST_SIZE 2048

/**
 * @brief Maximum word length in characters
 */
#define MAX_WORD_LENGTH 16

/**
 * @brief Maximum mnemonic length in words
 */
#define MAX_MNEMONIC_WORDS 25

/**
 * @brief Supported BIP-39 languages
 */
typedef enum {
    LANGUAGE_ENGLISH,
    LANGUAGE_SPANISH,
    LANGUAGE_FRENCH,
    LANGUAGE_ITALIAN,
    LANGUAGE_PORTUGUESE,
    LANGUAGE_CZECH,
    LANGUAGE_JAPANESE,
    LANGUAGE_KOREAN,
    LANGUAGE_CHINESE_SIMPLIFIED,
    LANGUAGE_CHINESE_TRADITIONAL,
    LANGUAGE_COUNT
} MnemonicLanguage;

/**
 * @brief Supported mnemonic types
 */
typedef enum {
    MNEMONIC_TYPE_BIP39,  /**< BIP-39 mnemonic (12, 15, 18, 21, 24 words) */
    MNEMONIC_TYPE_MONERO  /**< Monero mnemonic (25 words) */
} MnemonicType;

/**
 * @brief A loaded wordlist
 */
typedef struct {
    char words[MAX_WORDLIST_SIZE][MAX_WORD_LENGTH]; /**< Array of words */
    size_t count;                                   /**< Number of words */
    MnemonicLanguage language;                      /**< Language of wordlist */
} Wordlist;

/**
 * @brief Mnemonic module context
 */
typedef struct MnemonicContext MnemonicContext;

/**
 * @brief Initialize the mnemonic module
 * 
 * @param wordlist_dir Directory containing wordlist files
 * @return Mnemonic context or NULL on failure
 */
MnemonicContext* mnemonic_init(const char *wordlist_dir);

/**
 * @brief Clean up resources used by mnemonic module
 * 
 * @param context Mnemonic context to clean up
 */
void mnemonic_cleanup(MnemonicContext *context);

/**
 * @brief Load a wordlist for a specific language
 * 
 * @param context Mnemonic context
 * @param language Language to load
 * @return 0 on success, non-zero on failure
 */
int mnemonic_load_wordlist(MnemonicContext *context, MnemonicLanguage language);

/**
 * @brief Validate a mnemonic phrase
 * 
 * @param context Mnemonic context
 * @param mnemonic The mnemonic phrase to validate
 * @param type Pointer to store the detected mnemonic type
 * @param language Pointer to store the detected language (can be NULL)
 * @return true if valid, false otherwise
 */
bool mnemonic_validate(MnemonicContext *context, const char *mnemonic, 
                       MnemonicType *type, MnemonicLanguage *language);

/**
 * @brief Generate entropy from a mnemonic phrase
 * 
 * @param context Mnemonic context
 * @param mnemonic The mnemonic phrase
 * @param entropy Buffer to store entropy
 * @param entropy_len Pointer to store entropy length
 * @return 0 on success, non-zero on failure
 */
int mnemonic_to_entropy(MnemonicContext *context, const char *mnemonic, 
                        uint8_t *entropy, size_t *entropy_len);

/**
 * @brief Generate a seed from a mnemonic phrase
 * 
 * @param context Mnemonic context
 * @param mnemonic The mnemonic phrase
 * @param passphrase Optional passphrase (can be NULL)
 * @param seed Buffer to store the seed (should be at least 64 bytes)
 * @param seed_len Pointer to store the seed length
 * @return 0 on success, non-zero on failure
 */
int mnemonic_to_seed(MnemonicContext *context, const char *mnemonic, 
                    const char *passphrase, uint8_t *seed, size_t *seed_len);

/**
 * @brief Detect the language of a mnemonic phrase
 * 
 * @param context Mnemonic context
 * @param mnemonic The mnemonic phrase
 * @return Detected language or LANGUAGE_COUNT if not detected
 */
MnemonicLanguage mnemonic_detect_language(MnemonicContext *context, const char *mnemonic);

/**
 * @brief Check if a mnemonic phrase could be Monero (25 words)
 * 
 * @param context Mnemonic context
 * @param mnemonic The mnemonic phrase
 * @return true if potentially Monero, false otherwise
 */
bool mnemonic_is_monero(MnemonicContext *context, const char *mnemonic);

/**
 * @brief Get a human-readable language name
 * 
 * @param language Language enum value
 * @return Language name string
 */
const char* mnemonic_language_name(MnemonicLanguage language);

#endif /* MNEMONIC_H */ 