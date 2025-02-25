/**
 * @file simd_utils.h
 * @brief SIMD optimization utilities for both x86-64 (AVX2) and ARM64 (NEON)
 *
 * This file provides various SIMD-accelerated functions for string and memory operations,
 * as well as CPU feature detection.
 */

#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X86_64
    #include <immintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64
    #include <arm_neon.h>
#endif

// Alignment macros
#define CACHE_LINE_SIZE 64
#define ALIGN_TO_CACHE_LINE __attribute__((aligned(CACHE_LINE_SIZE)))

// SIMD features structure
typedef struct {
    bool has_avx2;       // Advanced Vector Extensions 2
    bool has_avx;        // Advanced Vector Extensions
    bool has_sse4_2;     // SSE 4.2
    bool has_sse4_1;     // SSE 4.1
    bool has_ssse3;      // Supplemental SSE 3
    bool has_sse3;       // SSE 3
    bool has_sse2;       // SSE 2
    bool has_sse;        // SSE
    bool has_neon;       // ARM NEON
    bool has_sve;        // ARM Scalable Vector Extension
    size_t cache_line_size; // CPU cache line size
    size_t vector_width;  // Width of vector registers in bytes
} simd_features_t;

// Bloom filter structure
typedef struct {
    uint8_t* bits;       // Bit array
    size_t size;         // Size of bit array in bits
    size_t hash_funcs;   // Number of hash functions
    size_t items;        // Number of items in the filter
    double error_rate;   // Desired false positive rate
} bloom_filter_t;

/**
 * @brief Initialize SIMD feature detection
 * 
 * @param features Pointer to a simd_features_t structure to fill
 * @return true if detection was successful
 */
bool simd_detect_features(simd_features_t* features);

/**
 * @brief Check if SIMD is available
 * 
 * @return true if SIMD is available
 */
bool simd_available(void);

/**
 * @brief Prefetch data into cache
 * 
 * @param addr Address to prefetch
 * @param rw Read/write hint (0 = read, 1 = write)
 * @param locality Locality hint (0 = none, 1 = low, 2 = medium, 3 = high)
 */
void simd_prefetch(const void* addr, int rw, int locality);

/**
 * @brief SIMD-accelerated string search
 * 
 * @param haystack String to search in
 * @param needle String to search for
 * @return Pointer to the first occurrence of needle in haystack, or NULL if not found
 */
const char* simd_strstr(const char* haystack, const char* needle);

/**
 * @brief SIMD-accelerated string comparison
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if the strings are equal, <0 if str1 < str2, >0 if str1 > str2
 */
int simd_strcmp(const char* str1, const char* str2);

/**
 * @brief SIMD-accelerated binary search
 * 
 * @param array Array of strings
 * @param n Number of elements
 * @param search String to search for
 * @return true if found
 */
bool simd_binary_search(char** array, size_t n, const char* search);

/**
 * @brief SIMD-accelerated case-insensitive string comparison
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return 0 if the strings are equal (ignoring case), <0 if str1 < str2, >0 if str1 > str2
 */
int simd_strcasecmp(const char* str1, const char* str2);

/**
 * @brief SIMD-accelerated memory fill with zeros
 * 
 * @param dest Destination memory
 * @param n Number of bytes
 */
void simd_memzero(void* dest, size_t n);

/**
 * @brief SIMD-accelerated memory copy
 * 
 * @param dest Destination memory
 * @param src Source memory
 * @param n Number of bytes
 * @return Destination memory
 */
void* simd_memcpy(void* dest, const void* src, size_t n);

/**
 * @brief Create a bloom filter
 * 
 * @param size Size of bit array in bits
 * @param error_rate Desired false positive rate (0.0 - 1.0)
 * @return Bloom filter
 */
bloom_filter_t bloom_filter_create(size_t size, double error_rate);

/**
 * @brief Add an item to a bloom filter
 * 
 * @param filter Bloom filter
 * @param item Item to add
 * @param size Size of item in bytes
 */
void bloom_filter_add(bloom_filter_t* filter, const void* item, size_t size);

/**
 * @brief Check if an item is in a bloom filter
 * 
 * @param filter Bloom filter
 * @param item Item to check
 * @param size Size of item in bytes
 * @return true if the item might be in the filter
 */
bool bloom_filter_check(const bloom_filter_t* filter, const void* item, size_t size);

/**
 * @brief Destroy a bloom filter
 * 
 * @param filter Bloom filter
 */
void bloom_filter_destroy(bloom_filter_t* filter);

#endif // SIMD_UTILS_H 