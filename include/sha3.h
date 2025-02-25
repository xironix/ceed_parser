/**
 * @file sha3.h
 * @brief SHA3 (Keccak) hash algorithm declarations
 *
 * This header provides declarations for the SHA3-256 hash algorithm
 * which is used for Ethereum address generation.
 */

#ifndef SHA3_H
#define SHA3_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief SHA3 context structure
 */
typedef struct {
    uint64_t state[25];  /* 1600 bits = 25 * 64 bits */
    size_t pos;          /* Current position in the state */
    size_t rate;         /* Rate in bits */
    size_t capacity;     /* Capacity in bits */
} SHA3_CTX;

/**
 * @brief Initialize the SHA3-256 context
 * 
 * @param ctx SHA3 context to initialize
 */
void sha3_256_Init(SHA3_CTX *ctx);

/**
 * @brief Update the SHA3 context with new data
 * 
 * @param ctx SHA3 context to update
 * @param data Pointer to the input data
 * @param len Length of the input data in bytes
 */
void sha3_Update(SHA3_CTX *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize the SHA3 hash and output the digest
 * 
 * @param ctx SHA3 context to finalize
 * @param digest Pointer to the output buffer (32 bytes for SHA3-256)
 */
void sha3_Final(SHA3_CTX *ctx, uint8_t *digest);

#endif /* SHA3_H */ 