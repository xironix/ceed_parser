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

// Maximum length of a file path
#define MAX_FILE_PATH 256

// Wallet types
typedef enum {
    WALLET_TYPE_BITCOIN   = 1,
    WALLET_TYPE_ETHEREUM  = 2,
    WALLET_TYPE_MONERO    = 3
} WalletType;

/**
 * @brief Cryptocurrency type
 */
typedef enum {
    CRYPTO_UNKNOWN = 0, // Unknown
    CRYPTO_BTC = 1,  // Bitcoin
    CRYPTO_ETH = 2,  // Ethereum
    CRYPTO_XMR = 3,  // Monero
    CRYPTO_ETC = 4,  // Ethereum Classic
    CRYPTO_LTC = 5,  // Litecoin
    CRYPTO_BCH = 6,  // Bitcoin Cash
    CRYPTO_BSV = 7,  // Bitcoin SV
    CRYPTO_BNB = 8,  // Binance Chain
    CRYPTO_DOGE = 9, // Dogecoin
    CRYPTO_DASH = 10, // Dash
    CRYPTO_ZEC = 11,  // Zcash
    CRYPTO_TRX = 12   // Tron
} CryptoType;

/**
 * @brief Address type
 */
typedef enum {
    ADDRESS_UNKNOWN = 0,   // Unknown
    ADDRESS_STANDARD = 1,  // Standard address
    ADDRESS_P2PKH = 2,     // Pay to Public Key Hash (Legacy)
    ADDRESS_P2SH = 3,      // Pay to Script Hash (SegWit-compatible)
    ADDRESS_P2WPKH = 4,    // Pay to Witness Public Key Hash (Native SegWit)
    ADDRESS_SUBADDRESS = 5 // Subaddress (e.g., Monero)
} AddressType;

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
    AddressType address_type;                      // Type of address generated
    char path[MAX_FILE_PATH];                      // Path to the wallet file
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
 * @param wallets Array of wallet structures to store the results
 * @param max_wallets Maximum number of wallets to generate
 * @param count Pointer to store the number of wallets generated
 * @return 0 on success, non-zero on failure
 */
int wallet_generate_multiple(const char *seed_phrase, Wallet *wallets, 
                             size_t max_wallets, size_t *count);

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

/**
 * Generate a Monero wallet from a mnemonic phrase
 * 
 * @param mnemonic The Monero mnemonic phrase
 * @param wallet Pointer to wallet structure to store the results
 * @return 0 on success, non-zero on failure
 */
int wallet_monero_from_mnemonic(const char *mnemonic, Wallet *wallet);

#endif /* WALLET_H */ 
