/**
 * @file wallet.h
 * @brief Cryptocurrency wallet functionality
 *
 * Functionality for generating cryptocurrency wallets from seed phrases.
 */

#ifndef WALLET_H
#define WALLET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Maximum length of an address
 */
#define MAX_ADDRESS_LENGTH 128

/**
 * @brief Maximum length of a private key
 */
#define MAX_PRIVKEY_LENGTH 128

/**
 * @brief Supported cryptocurrency types
 */
typedef enum {
    CRYPTO_BTC,    /**< Bitcoin */
    CRYPTO_ETH,    /**< Ethereum */
    CRYPTO_LTC,    /**< Litecoin */
    CRYPTO_BCH,    /**< Bitcoin Cash */
    CRYPTO_BSV,    /**< Bitcoin SV */
    CRYPTO_BNB,    /**< Binance Chain */
    CRYPTO_DOGE,   /**< Dogecoin */
    CRYPTO_DASH,   /**< Dash */
    CRYPTO_ZEC,    /**< Zcash */
    CRYPTO_TRX,    /**< Tron */
    CRYPTO_ETC,    /**< Ethereum Classic */
    CRYPTO_XMR     /**< Monero */
} CryptoType;

/**
 * @brief Address types for different cryptocurrencies
 */
typedef enum {
    ADDRESS_P2PKH,    /**< Pay to Public Key Hash (Legacy) */
    ADDRESS_P2SH,     /**< Pay to Script Hash (Compatible SegWit) */
    ADDRESS_P2WPKH,   /**< Pay to Witness Public Key Hash (Native SegWit) */
    ADDRESS_STANDARD, /**< Standard address format (e.g., for ETH, XMR) */
    ADDRESS_SUBADDRESS /**< Subaddress (e.g., for Monero) */
} AddressType;

/**
 * @brief A generated wallet address
 */
typedef struct {
    char address[MAX_ADDRESS_LENGTH];     /**< Public address */
    char private_key[MAX_PRIVKEY_LENGTH]; /**< Private key */
    char path[128];                        /**< Derivation path */
    CryptoType type;                       /**< Crypto type */
    AddressType address_type;              /**< Address type */
} Wallet;

/**
 * @brief Initialize the wallet module
 * 
 * @return 0 on success, non-zero on failure
 */
int wallet_init(void);

/**
 * @brief Clean up resources used by wallet module
 */
void wallet_cleanup(void);

/**
 * @brief Generate a crypto wallet from a mnemonic phrase
 * 
 * @param mnemonic The BIP39 mnemonic phrase
 * @param type Cryptocurrency type
 * @param path Derivation path
 * @param wallet Pointer to Wallet struct to fill
 * @return 0 on success, non-zero on failure
 */
int wallet_from_mnemonic(const char *mnemonic, CryptoType type, const char *path, Wallet *wallet);

/**
 * @brief Generate a Monero wallet from a 25-word seed phrase
 * 
 * @param mnemonic The Monero 25-word seed phrase
 * @param wallet Pointer to Wallet struct to fill
 * @return 0 on success, non-zero on failure
 */
int wallet_monero_from_mnemonic(const char *mnemonic, Wallet *wallet);

/**
 * @brief Generate Monero subaddresses from a primary address
 * 
 * @param primary_wallet Primary Monero wallet
 * @param subaddresses Array of Wallet structs to fill with subaddresses
 * @param max_count Maximum number of subaddresses to generate
 * @param actual_count Pointer to store the actual number generated
 * @return 0 on success, non-zero on failure
 */
int wallet_monero_generate_subaddresses(const Wallet *primary_wallet, 
                                       Wallet *subaddresses, 
                                       size_t max_count, 
                                       size_t *actual_count);

/**
 * @brief Extract Ethereum address from private key
 * 
 * @param private_key Private key in hex format
 * @param address Buffer to store the address (should be at least MAX_ADDRESS_LENGTH)
 * @return 0 on success, non-zero on failure
 */
int wallet_eth_address_from_private_key(const char *private_key, char *address);

/**
 * @brief Generate wallets for multiple cryptocurrencies from a mnemonic
 * 
 * @param mnemonic The BIP39 mnemonic phrase
 * @param wallets Array of Wallet structs to fill
 * @param max_wallets Maximum number of wallets to generate
 * @param count Pointer to store actual count of wallets generated
 * @return 0 on success, non-zero on failure
 */
int wallet_generate_multiple(const char *mnemonic, Wallet *wallets, size_t max_wallets, size_t *count);

/**
 * @brief Print wallet information to a file
 * 
 * @param wallet The wallet to print
 * @param file File to print to
 * @return 0 on success, non-zero on failure
 */
int wallet_print(const Wallet *wallet, FILE *file);

#endif /* WALLET_H */ 