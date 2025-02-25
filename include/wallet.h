/**
 * @file wallet.h
 * @brief Cryptocurrency wallet functionality
 *
 * Functionality for generating cryptocurrency wallets from seed phrases.
 */

#ifndef WALLET_H
#define WALLET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Maximum wallet addresses to generate per seed
#define MAX_WALLET_ADDRESSES 10

// Maximum length of a cryptocurrency address
#define MAX_ADDRESS_LENGTH 108  // Monero addresses can be long

// Maximum length of a private key in hex format
#define MAX_PRIVATE_KEY_LENGTH 128

// Wallet types
typedef enum {
    WALLET_TYPE_BITCOIN   = 1,
    WALLET_TYPE_ETHEREUM  = 2,
    WALLET_TYPE_MONERO    = 3
} WalletType;

// Structure to hold wallet information
typedef struct {
    int type;                                      // Type of wallet (Bitcoin, Ethereum, Monero)
    char seed_phrase[1024];                        // Original seed phrase
    uint8_t seed[64];                              // Binary seed data
    size_t seed_length;                            // Length of the seed data
    char addresses[MAX_WALLET_ADDRESSES][MAX_ADDRESS_LENGTH]; // Generated addresses
    int address_count;                             // Number of addresses generated
    char private_keys[MAX_WALLET_ADDRESSES][MAX_PRIVATE_KEY_LENGTH]; // Private keys (if stored)
    bool has_private_keys;                         // Whether private keys are stored
} Wallet;

/**
 * Initialize the wallet system
 * 
 * @return 0 on success, non-zero on failure
 */
int wallet_init(void);

/**
 * Cleanup the wallet system
 */
void wallet_cleanup(void);

/**
 * Generate a wallet from a seed phrase
 * 
 * @param seed_phrase The seed phrase (mnemonic)
 * @param wallet_type Type of wallet to generate (WALLET_TYPE_*)
 * @param passphrase Optional passphrase (can be NULL)
 * @param wallet Pointer to wallet structure to store the results
 * @return true if wallet generation succeeded, false otherwise
 */
bool wallet_generate_from_seed(const char *seed_phrase, int wallet_type, 
                               const char *passphrase, Wallet *wallet);

/**
 * Generate multiple wallets from a seed phrase
 * 
 * @param seed_phrase The seed phrase (mnemonic)
 * @param wallet_types Array of wallet types to generate
 * @param wallet_count Number of wallet types in the array
 * @param passphrase Optional passphrase (can be NULL)
 * @param wallets Array of wallet structures to store the results
 * @return Number of successfully generated wallets
 */
int wallet_generate_multiple(const char *seed_phrase, const int *wallet_types, 
                             size_t wallet_count, const char *passphrase, 
                             Wallet *wallets);

/**
 * Validate a cryptocurrency address
 * 
 * @param address The address to validate
 * @param wallet_type Type of wallet/address
 * @return true if the address is valid, false otherwise
 */
bool wallet_validate_address(const char *address, int wallet_type);

/**
 * Get human-readable name for wallet type
 * 
 * @param wallet_type The wallet type
 * @return String with wallet type name
 */
const char *wallet_type_name(int wallet_type);

#endif /* WALLET_H */ 