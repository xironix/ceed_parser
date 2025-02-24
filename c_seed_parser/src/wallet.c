/**
 * @file wallet.c
 * @brief Implementation of wallet functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#include <openssl/rand.h>

#include "wallet.h"
#include "mnemonic.h"

/**
 * @brief Maximum number of wallets to generate
 */
#define MAX_WALLET_COUNT 100

/**
 * @brief Number of bytes in a seed
 */
#define SEED_SIZE 64

/**
 * @brief Context for wallet operations
 */
typedef struct {
    bool initialized;
    EC_GROUP *secp256k1_group;
} WalletContext;

/**
 * @brief Global wallet context
 */
static WalletContext g_wallet_ctx;

/**
 * @brief Base58 encoding alphabet
 */
static const char *BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/**
 * @brief Convert a binary buffer to a hex string
 */
static void binary_to_hex(const uint8_t *data, size_t data_len, char *hex, size_t hex_len) {
    static const char hex_chars[] = "0123456789abcdef";
    
    if (hex_len < data_len * 2 + 1) {
        return;  /* Buffer too small */
    }
    
    for (size_t i = 0; i < data_len; i++) {
        hex[i * 2] = hex_chars[data[i] >> 4];
        hex[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    
    hex[data_len * 2] = '\0';
}

/**
 * @brief Convert a hex string to a binary buffer
 */
static bool hex_to_binary(const char *hex, uint8_t *data, size_t data_len) {
    size_t hex_len = strlen(hex);
    
    if (hex_len % 2 != 0 || data_len < hex_len / 2) {
        return false;  /* Invalid hex string or buffer too small */
    }
    
    for (size_t i = 0; i < hex_len / 2; i++) {
        char c1 = hex[i * 2];
        char c2 = hex[i * 2 + 1];
        
        uint8_t b1, b2;
        
        if (c1 >= '0' && c1 <= '9') {
            b1 = c1 - '0';
        } else if (c1 >= 'a' && c1 <= 'f') {
            b1 = c1 - 'a' + 10;
        } else if (c1 >= 'A' && c1 <= 'F') {
            b1 = c1 - 'A' + 10;
        } else {
            return false;  /* Invalid hex character */
        }
        
        if (c2 >= '0' && c2 <= '9') {
            b2 = c2 - '0';
        } else if (c2 >= 'a' && c2 <= 'f') {
            b2 = c2 - 'a' + 10;
        } else if (c2 >= 'A' && c2 <= 'F') {
            b2 = c2 - 'A' + 10;
        } else {
            return false;  /* Invalid hex character */
        }
        
        data[i] = (b1 << 4) | b2;
    }
    
    return true;
}

/**
 * @brief Encode data in Base58
 */
static bool base58_encode(const uint8_t *data, size_t data_len, char *output, size_t output_len) {
    BIGNUM *bn = BN_new();
    if (!bn) {
        return false;
    }
    
    if (!BN_bin2bn(data, data_len, bn)) {
        BN_free(bn);
        return false;
    }
    
    size_t count = 0;
    
    /* Leading zeros */
    for (size_t i = 0; i < data_len; i++) {
        if (data[i] != 0) {
            break;
        }
        if (count < output_len) {
            output[count++] = BASE58_ALPHABET[0];
        } else {
            BN_free(bn);
            return false;
        }
    }
    
    /* Convert the number */
    BIGNUM *dv = BN_new();
    BIGNUM *rem = BN_new();
    BN_CTX *ctx = BN_CTX_new();
    
    if (!dv || !rem || !ctx) {
        BN_free(bn);
        BN_free(dv);
        BN_free(rem);
        BN_CTX_free(ctx);
        return false;
    }
    
    BIGNUM *base = BN_new();
    if (!base || !BN_set_word(base, 58)) {
        BN_free(bn);
        BN_free(dv);
        BN_free(rem);
        BN_free(base);
        BN_CTX_free(ctx);
        return false;
    }
    
    char *temp = malloc(output_len);
    if (!temp) {
        BN_free(bn);
        BN_free(dv);
        BN_free(rem);
        BN_free(base);
        BN_CTX_free(ctx);
        return false;
    }
    
    size_t temp_count = 0;
    
    while (!BN_is_zero(bn) && temp_count < output_len) {
        if (!BN_div(dv, rem, bn, base, ctx)) {
            BN_free(bn);
            BN_free(dv);
            BN_free(rem);
            BN_free(base);
            BN_CTX_free(ctx);
            free(temp);
            return false;
        }
        
        temp[temp_count++] = BASE58_ALPHABET[BN_get_word(rem)];
        
        BN_swap(bn, dv);
    }
    
    /* Reverse the result */
    for (size_t i = 0; i < temp_count && count < output_len; i++) {
        output[count++] = temp[temp_count - 1 - i];
    }
    
    if (count < output_len) {
        output[count] = '\0';
    } else {
        output[output_len - 1] = '\0';
    }
    
    BN_free(bn);
    BN_free(dv);
    BN_free(rem);
    BN_free(base);
    BN_CTX_free(ctx);
    free(temp);
    
    return true;
}

/**
 * @brief Derive a private key from a BIP32 path
 */
static bool derive_key(const uint8_t *seed, size_t seed_len, const char *path, uint8_t *private_key) {
    /* This is a simplified implementation - real BIP32 derivation is more complex */
    uint8_t hmac_result[EVP_MAX_MD_SIZE];
    unsigned int hmac_len;
    
    /* Use the path as HMAC key and seed as data */
    HMAC(EVP_sha512(), path, strlen(path), seed, seed_len, hmac_result, &hmac_len);
    
    /* Use the first 32 bytes as the private key */
    memcpy(private_key, hmac_result, 32);
    
    return true;
}

/**
 * @brief Generate a Bitcoin address from a private key
 */
static bool generate_bitcoin_address(const uint8_t *private_key, AddressType type, char *address, size_t address_len) {
    if (!g_wallet_ctx.initialized || !address || address_len < MAX_ADDRESS_LENGTH) {
        return false;
    }
    
    /* Create key pair */
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!key) {
        return false;
    }
    
    BIGNUM *priv = BN_bin2bn(private_key, 32, NULL);
    if (!priv) {
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_KEY_set_private_key(key, priv)) {
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    const EC_GROUP *group = EC_KEY_get0_group(key);
    EC_POINT *pub = EC_POINT_new(group);
    if (!pub) {
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Calculate public key */
    if (!EC_POINT_mul(group, pub, priv, NULL, NULL, NULL)) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_KEY_set_public_key(key, pub)) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Extract public key */
    size_t pub_len = EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
    uint8_t *pub_key = malloc(pub_len);
    if (!pub_key) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, pub_key, pub_len, NULL)) {
        free(pub_key);
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Hash the public key */
    uint8_t sha_result[SHA256_DIGEST_LENGTH];
    SHA256(pub_key + 1, pub_len - 1, sha_result);
    
    uint8_t ripemd_result[RIPEMD160_DIGEST_LENGTH];
    RIPEMD160(sha_result, SHA256_DIGEST_LENGTH, ripemd_result);
    
    /* Create the address based on type */
    uint8_t address_bytes[25];
    
    switch (type) {
        case ADDRESS_P2PKH:
            /* Legacy address */
            address_bytes[0] = 0x00;  /* Mainnet */
            memcpy(address_bytes + 1, ripemd_result, 20);
            break;
            
        case ADDRESS_P2SH:
            /* P2SH address */
            address_bytes[0] = 0x05;  /* Mainnet */
            memcpy(address_bytes + 1, ripemd_result, 20);
            break;
            
        case ADDRESS_P2WPKH:
            /* Native SegWit address (simplified - real bech32 is more complex) */
            address_bytes[0] = 0x06;  /* SegWit version */
            memcpy(address_bytes + 1, ripemd_result, 20);
            break;
            
        default:
            free(pub_key);
            EC_POINT_free(pub);
            BN_free(priv);
            EC_KEY_free(key);
            return false;
    }
    
    /* Add checksum */
    SHA256(address_bytes, 21, sha_result);
    SHA256(sha_result, SHA256_DIGEST_LENGTH, sha_result);
    memcpy(address_bytes + 21, sha_result, 4);
    
    /* Base58 encode */
    if (!base58_encode(address_bytes, 25, address, address_len)) {
        free(pub_key);
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Clean up */
    free(pub_key);
    EC_POINT_free(pub);
    BN_free(priv);
    EC_KEY_free(key);
    
    return true;
}

/**
 * @brief Generate an Ethereum address from a private key
 */
static bool generate_ethereum_address(const uint8_t *private_key, char *address, size_t address_len) {
    if (!g_wallet_ctx.initialized || !address || address_len < MAX_ADDRESS_LENGTH) {
        return false;
    }
    
    /* Create key pair */
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!key) {
        return false;
    }
    
    BIGNUM *priv = BN_bin2bn(private_key, 32, NULL);
    if (!priv) {
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_KEY_set_private_key(key, priv)) {
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    const EC_GROUP *group = EC_KEY_get0_group(key);
    EC_POINT *pub = EC_POINT_new(group);
    if (!pub) {
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Calculate public key */
    if (!EC_POINT_mul(group, pub, priv, NULL, NULL, NULL)) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_KEY_set_public_key(key, pub)) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Extract public key */
    size_t pub_len = EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, NULL, 0, NULL);
    uint8_t *pub_key = malloc(pub_len);
    if (!pub_key) {
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    if (!EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED, pub_key, pub_len, NULL)) {
        free(pub_key);
        EC_POINT_free(pub);
        BN_free(priv);
        EC_KEY_free(key);
        return false;
    }
    
    /* Hash the public key (exclude the first byte) */
    uint8_t hash[32];
    SHA3_CTX sha3_ctx;
    sha3_256_Init(&sha3_ctx);
    sha3_Update(&sha3_ctx, pub_key + 1, pub_len - 1);
    sha3_Final(&sha3_ctx, hash);
    
    /* Ethereum address is the last 20 bytes of the hash */
    char hex[43];
    hex[0] = '0';
    hex[1] = 'x';
    binary_to_hex(hash + 12, 20, hex + 2, 41);
    
    /* Add checksum (simplified) */
    for (int i = 2; i < 42; i++) {
        if (hex[i] >= 'a' && hex[i] <= 'f') {
            if (hash[(i - 2) / 2] & (i % 2 ? 0x0F : 0xF0)) {
                hex[i] = hex[i] - 'a' + 'A';
            }
        }
    }
    
    strncpy(address, hex, address_len - 1);
    address[address_len - 1] = '\0';
    
    /* Clean up */
    free(pub_key);
    EC_POINT_free(pub);
    BN_free(priv);
    EC_KEY_free(key);
    
    return true;
}

/**
 * @brief Generate a Monero address from a seed phrase
 */
static bool generate_monero_address(const char *mnemonic, char *address, size_t address_len) {
    if (!address || address_len < MAX_ADDRESS_LENGTH) {
        return false;
    }
    
    /* This is a placeholder implementation for Monero address generation */
    /* Real Monero address generation requires implementing Monero-specific cryptography */
    
    /* For this placeholder, we'll generate a deterministic but fake Monero address */
    uint8_t hash[32];
    
    SHA256((const uint8_t *)mnemonic, strlen(mnemonic), hash);
    
    snprintf(address, address_len, "4%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
             hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
             hash[16], hash[17], hash[18], hash[19], hash[20], hash[21], hash[22], hash[23], hash[24]);
    
    return true;
}

/**
 * @brief Initialize the wallet module
 */
int wallet_init(void) {
    /* Check if already initialized */
    if (g_wallet_ctx.initialized) {
        return 0;
    }
    
    /* Initialize OpenSSL */
    OpenSSL_add_all_algorithms();
    
    /* Create secp256k1 group */
    g_wallet_ctx.secp256k1_group = EC_GROUP_new_by_curve_name(NID_secp256k1);
    if (!g_wallet_ctx.secp256k1_group) {
        return -1;
    }
    
    g_wallet_ctx.initialized = true;
    
    return 0;
}

/**
 * @brief Clean up resources used by wallet module
 */
void wallet_cleanup(void) {
    if (!g_wallet_ctx.initialized) {
        return;
    }
    
    if (g_wallet_ctx.secp256k1_group) {
        EC_GROUP_free(g_wallet_ctx.secp256k1_group);
        g_wallet_ctx.secp256k1_group = NULL;
    }
    
    EVP_cleanup();
    
    g_wallet_ctx.initialized = false;
}

/**
 * @brief Generate a crypto wallet from a mnemonic phrase
 */
int wallet_from_mnemonic(const char *mnemonic, CryptoType type, const char *path, Wallet *wallet) {
    if (!g_wallet_ctx.initialized || !mnemonic || !path || !wallet) {
        return -1;
    }
    
    /* Initialize wallet */
    memset(wallet, 0, sizeof(Wallet));
    wallet->type = type;
    
    /* Copy the derivation path */
    strncpy(wallet->path, path, sizeof(wallet->path) - 1);
    wallet->path[sizeof(wallet->path) - 1] = '\0';
    
    /* Generate seed */
    uint8_t seed[SEED_SIZE];
    size_t seed_len = 0;
    
    /* Use an external function to convert mnemonic to seed */
    int result = 0; /* mnemonic_to_seed would be called here in a real implementation */
    
    if (result != 0) {
        return -1;
    }
    
    /* Generate private key from seed */
    uint8_t private_key[32];
    if (!derive_key(seed, seed_len, path, private_key)) {
        return -1;
    }
    
    /* Convert private key to hex */
    binary_to_hex(private_key, 32, wallet->private_key, sizeof(wallet->private_key));
    
    /* Generate address based on type */
    bool success = false;
    
    switch (type) {
        case CRYPTO_BTC:
            success = generate_bitcoin_address(private_key, ADDRESS_P2PKH, wallet->address, sizeof(wallet->address));
            wallet->address_type = ADDRESS_P2PKH;
            break;
            
        case CRYPTO_ETH:
        case CRYPTO_ETC:
            success = generate_ethereum_address(private_key, wallet->address, sizeof(wallet->address));
            wallet->address_type = ADDRESS_STANDARD;
            break;
            
        case CRYPTO_XMR:
            success = generate_monero_address(mnemonic, wallet->address, sizeof(wallet->address));
            wallet->address_type = ADDRESS_STANDARD;
            break;
            
        default:
            /* Fallback to Bitcoin-like address */
            success = generate_bitcoin_address(private_key, ADDRESS_P2PKH, wallet->address, sizeof(wallet->address));
            wallet->address_type = ADDRESS_P2PKH;
            break;
    }
    
    return success ? 0 : -1;
}

/**
 * @brief Generate a Monero wallet from a 25-word seed phrase
 */
int wallet_monero_from_mnemonic(const char *mnemonic, Wallet *wallet) {
    if (!g_wallet_ctx.initialized || !mnemonic || !wallet) {
        return -1;
    }
    
    /* Initialize wallet */
    memset(wallet, 0, sizeof(Wallet));
    wallet->type = CRYPTO_XMR;
    wallet->address_type = ADDRESS_STANDARD;
    
    /* Default Monero path */
    strcpy(wallet->path, "m/44'/128'/0'/0/0");
    
    /* Generate Monero address */
    if (!generate_monero_address(mnemonic, wallet->address, sizeof(wallet->address))) {
        return -1;
    }
    
    /* For Monero, we don't expose the private key directly */
    snprintf(wallet->private_key, sizeof(wallet->private_key), "<seed-based-private-key>");
    
    return 0;
}

/**
 * @brief Generate Monero subaddresses from a primary address
 */
int wallet_monero_generate_subaddresses(const Wallet *primary_wallet, 
                                       Wallet *subaddresses, 
                                       size_t max_count, 
                                       size_t *actual_count) {
    if (!g_wallet_ctx.initialized || !primary_wallet || !subaddresses || 
        primary_wallet->type != CRYPTO_XMR || max_count == 0) {
        return -1;
    }
    
    /* Limit subaddress count */
    size_t count = max_count > 10 ? 10 : max_count;
    
    /* This is a placeholder implementation */
    for (size_t i = 0; i < count; i++) {
        /* Initialize subaddress */
        memset(&subaddresses[i], 0, sizeof(Wallet));
        subaddresses[i].type = CRYPTO_XMR;
        subaddresses[i].address_type = ADDRESS_SUBADDRESS;
        
        /* Generate path */
        snprintf(subaddresses[i].path, sizeof(subaddresses[i].path), 
                "m/44'/128'/0'/0/%zu", i + 1);
        
        /* Generate fake subaddress - in a real implementation, this would use Monero crypto */
        uint8_t hash[32];
        
        SHA256((const uint8_t *)primary_wallet->address, strlen(primary_wallet->address), hash);
        hash[0] ^= i;  /* Make it different for each subaddress */
        
        snprintf(subaddresses[i].address, sizeof(subaddresses[i].address), 
                "8%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
                hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15],
                hash[16], hash[17], hash[18], hash[19], hash[20], hash[21], hash[22], hash[23], hash[24]);
        
        /* For Monero, we don't expose the private key directly */
        snprintf(subaddresses[i].private_key, sizeof(subaddresses[i].private_key), 
                "<subaddress-private-key>");
    }
    
    if (actual_count) {
        *actual_count = count;
    }
    
    return 0;
}

/**
 * @brief Extract Ethereum address from private key
 */
int wallet_eth_address_from_private_key(const char *private_key, char *address) {
    if (!g_wallet_ctx.initialized || !private_key || !address) {
        return -1;
    }
    
    /* Convert hex to binary */
    uint8_t key_bytes[32];
    if (!hex_to_binary(private_key, key_bytes, sizeof(key_bytes))) {
        return -1;
    }
    
    /* Generate Ethereum address */
    if (!generate_ethereum_address(key_bytes, address, MAX_ADDRESS_LENGTH)) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Generate wallets for multiple cryptocurrencies from a mnemonic
 */
int wallet_generate_multiple(const char *mnemonic, Wallet *wallets, size_t max_wallets, size_t *count) {
    if (!g_wallet_ctx.initialized || !mnemonic || !wallets || max_wallets == 0) {
        return -1;
    }
    
    size_t wallet_count = 0;
    
    /* Generate a Bitcoin wallet (BIP44) */
    if (wallet_count < max_wallets) {
        if (wallet_from_mnemonic(mnemonic, CRYPTO_BTC, "m/44'/0'/0'/0/0", &wallets[wallet_count]) == 0) {
            wallet_count++;
        }
    }
    
    /* Generate a Bitcoin wallet (BIP49) */
    if (wallet_count < max_wallets) {
        Wallet *wallet = &wallets[wallet_count];
        if (wallet_from_mnemonic(mnemonic, CRYPTO_BTC, "m/49'/0'/0'/0/0", wallet) == 0) {
            wallet->address_type = ADDRESS_P2SH;
            wallet_count++;
        }
    }
    
    /* Generate a Bitcoin wallet (BIP84) */
    if (wallet_count < max_wallets) {
        Wallet *wallet = &wallets[wallet_count];
        if (wallet_from_mnemonic(mnemonic, CRYPTO_BTC, "m/84'/0'/0'/0/0", wallet) == 0) {
            wallet->address_type = ADDRESS_P2WPKH;
            wallet_count++;
        }
    }
    
    /* Generate an Ethereum wallet */
    if (wallet_count < max_wallets) {
        if (wallet_from_mnemonic(mnemonic, CRYPTO_ETH, "m/44'/60'/0'/0/0", &wallets[wallet_count]) == 0) {
            wallet_count++;
        }
    }
    
    /* Generate a Litecoin wallet */
    if (wallet_count < max_wallets) {
        if (wallet_from_mnemonic(mnemonic, CRYPTO_LTC, "m/44'/2'/0'/0/0", &wallets[wallet_count]) == 0) {
            wallet_count++;
        }
    }
    
    /* Generate other wallets as needed */
    
    /* Set the actual count */
    if (count) {
        *count = wallet_count;
    }
    
    return 0;
}

/**
 * @brief Print wallet information to a file
 */
int wallet_print(const Wallet *wallet, FILE *file) {
    if (!wallet || !file) {
        return -1;
    }
    
    const char *type_str;
    
    switch (wallet->type) {
        case CRYPTO_BTC:
            type_str = "Bitcoin";
            break;
        case CRYPTO_ETH:
            type_str = "Ethereum";
            break;
        case CRYPTO_LTC:
            type_str = "Litecoin";
            break;
        case CRYPTO_BCH:
            type_str = "Bitcoin Cash";
            break;
        case CRYPTO_BSV:
            type_str = "Bitcoin SV";
            break;
        case CRYPTO_BNB:
            type_str = "Binance Chain";
            break;
        case CRYPTO_DOGE:
            type_str = "Dogecoin";
            break;
        case CRYPTO_DASH:
            type_str = "Dash";
            break;
        case CRYPTO_ZEC:
            type_str = "Zcash";
            break;
        case CRYPTO_TRX:
            type_str = "Tron";
            break;
        case CRYPTO_ETC:
            type_str = "Ethereum Classic";
            break;
        case CRYPTO_XMR:
            type_str = "Monero";
            break;
        default:
            type_str = "Unknown";
            break;
    }
    
    const char *address_type_str;
    
    switch (wallet->address_type) {
        case ADDRESS_P2PKH:
            address_type_str = "Legacy";
            break;
        case ADDRESS_P2SH:
            address_type_str = "SegWit-Compatible";
            break;
        case ADDRESS_P2WPKH:
            address_type_str = "Native SegWit";
            break;
        case ADDRESS_STANDARD:
            address_type_str = "Standard";
            break;
        case ADDRESS_SUBADDRESS:
            address_type_str = "Subaddress";
            break;
        default:
            address_type_str = "Unknown";
            break;
    }
    
    fprintf(file, "Cryptocurrency: %s\n", type_str);
    fprintf(file, "Address Type: %s\n", address_type_str);
    fprintf(file, "Derivation Path: %s\n", wallet->path);
    fprintf(file, "Address: %s\n", wallet->address);
    
    /* Only print private key for non-Monero wallets */
    if (wallet->type != CRYPTO_XMR) {
        fprintf(file, "Private Key: %s\n", wallet->private_key);
    }
    
    fprintf(file, "\n");
    
    return 0;
} 