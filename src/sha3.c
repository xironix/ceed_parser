/**
 * @file sha3.c
 * @brief Implementation of the SHA3 (Keccak) hash algorithm
 * 
 * This file provides an implementation of the SHA3-256 hash algorithm
 * which is used for Ethereum address generation.
 */

#include <string.h>
#include <stdint.h>
#include "sha3.h"

/* Constants for SHA3-256 */
#define KECCAK_ROUNDS 24
#define KECCAK_RATE 136  /* 1088 bits / 8 = 136 bytes */

/* Rotation constants for Keccak */
static const int keccakf_rotc[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44
};

/* Permutation constants for Keccak */
static const int keccakf_piln[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1
};

/* Round constants for Keccak */
static const uint64_t keccakf_rndc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

/* Rotate left macro */
#define ROL64(a, b) (((a) << (b)) | ((a) >> (64 - (b))))

/**
 * @brief Perform the Keccak-f[1600] permutation
 */
static void keccakf(uint64_t state[25]) {
    uint64_t t, bc[5];
    int i, j, round;

    for (round = 0; round < KECCAK_ROUNDS; round++) {
        /* Theta step */
        for (i = 0; i < 5; i++) {
            bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }

        for (i = 0; i < 5; i++) {
            t = bc[(i + 4) % 5] ^ ROL64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5) {
                state[j + i] ^= t;
            }
        }

        /* Rho and Pi steps */
        t = state[1];
        for (i = 0; i < 24; i++) {
            j = keccakf_piln[i];
            bc[0] = state[j];
            state[j] = ROL64(t, keccakf_rotc[i]);
            t = bc[0];
        }

        /* Chi step */
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++) {
                bc[i] = state[j + i];
            }
            for (i = 0; i < 5; i++) {
                state[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
        }

        /* Iota step */
        state[0] ^= keccakf_rndc[round];
    }
}

/**
 * @brief Initialize the SHA3 context
 */
void sha3_256_Init(SHA_CTX *ctx) {
    memset(ctx, 0, sizeof(SHA_CTX));
    ctx->capacity = 512; /* SHA3-256 has 512 bits of capacity */
    ctx->rate = 1088;    /* 1600 - 512 = 1088 bits of rate */
}

/**
 * @brief Update the SHA3 context with new data
 */
void sha3_Update(SHA_CTX *ctx, const uint8_t *data, size_t len) {
    size_t i;
    uint8_t *state = (uint8_t *)ctx->state;
    
    /* Absorb the input data */
    for (i = 0; i < len; i++) {
        /* XOR in the new byte */
        state[ctx->pos] ^= data[i];
        ctx->pos++;
        
        /* If we've filled the rate, apply the permutation */
        if (ctx->pos == ctx->rate / 8) {
            keccakf(ctx->state);
            ctx->pos = 0;
        }
    }
}

/**
 * @brief Finalize the SHA3 hash and output the digest
 */
void sha3_Final(SHA_CTX *ctx, uint8_t *digest) {
    uint8_t *state = (uint8_t *)ctx->state;
    
    /* Add the final bit of padding */
    state[ctx->pos] ^= 0x06;  /* Ethereum uses 0x06 for SHA3-256 */
    
    /* Add the final bit */
    state[ctx->rate / 8 - 1] ^= 0x80;
    
    /* Apply the final permutation */
    keccakf(ctx->state);
    
    /* Copy the output digest */
    memcpy(digest, state, 32); /* 256 bits = 32 bytes */
} 