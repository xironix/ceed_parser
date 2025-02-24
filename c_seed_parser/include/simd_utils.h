/**
 * @file simd_utils.h
 * @brief SIMD optimization utilities for Ceed Parser
 *
 * This file provides SIMD-accelerated functions for word matching
 * and other performance-critical operations, with specializations
 * for both x86 (AVX/AVX2) and ARM (NEON) platforms.
 */

#ifndef SIMD_UTILS_H
#define SIMD_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * Architecture detection and SIMD instruction inclusion
 */
#if defined(__x86_64__) || defined(_M_X64)
    #define SIMD_ARCH_X86_64
    #if defined(__AVX2__)
        #define HAVE_AVX2
        #include <immintrin.h>
    #elif defined(__AVX__)
        #define HAVE_AVX
        #include <immintrin.h>
    #elif defined(__SSE4_2__)
        #define HAVE_SSE4_2
        #include <nmmintrin.h>
    #elif defined(__SSE4_1__)
        #define HAVE_SSE4_1
        #include <smmintrin.h>
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define SIMD_ARCH_ARM64
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define HAVE_NEON
        #include <arm_neon.h>
    #endif
#endif

/**
 * Alignment macros for optimal memory access
 */
#define ALIGN_16 __attribute__((aligned(16)))
#define ALIGN_32 __attribute__((aligned(32)))
#define ALIGN_64 __attribute__((aligned(64)))

/**
 * CPU feature detection at runtime
 */
typedef struct {
    bool has_avx;
    bool has_avx2;
    bool has_sse4_1;
    bool has_sse4_2;
    bool has_neon;
    int cache_line_size;
    int vector_width;
} simd_features_t;

/**
 * @brief Initialize SIMD features detection
 * 
 * @param features Pointer to features structure to fill
 */
void simd_detect_features(simd_features_t* features);

/**
 * @brief Check if SIMD optimizations are available
 * 
 * @return true if any SIMD feature is available
 */
bool simd_available(void);

/**
 * @brief Prefetch data into cache for read
 * 
 * @param addr Address to prefetch
 */
static inline void simd_prefetch(const void* addr) {
#if defined(SIMD_ARCH_X86_64) && (defined(HAVE_SSE4_1) || defined(HAVE_SSE4_2) || defined(HAVE_AVX) || defined(HAVE_AVX2))
    _mm_prefetch((const char*)addr, _MM_HINT_T0);
#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
    __builtin_prefetch(addr, 0, 3);
#else
    (void)addr; // Avoid unused parameter warning
#endif
}

/**
 * @brief SIMD-optimized string search
 * 
 * Searches for needle in haystack using SIMD instructions when available.
 * Falls back to standard strstr when SIMD is not available.
 * 
 * @param haystack String to search in
 * @param needle String to search for
 * @return Pointer to the found location or NULL
 */
const char* simd_strstr(const char* haystack, const char* needle);

/**
 * @brief SIMD-optimized string compare
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return Integer less than, equal to, or greater than zero if str1 is found 
 *         to be less than, equal to, or greater than str2
 */
int simd_strcmp(const char* str1, const char* str2);

/**
 * @brief SIMD-optimized binary search for word list lookups
 * 
 * This is an optimized binary search implementation that uses 
 * SIMD instructions to compare multiple words at once.
 * 
 * @param words Array of words (must be sorted)
 * @param word_count Number of words in the array
 * @param target Target word to find
 * @return true if the word was found, false otherwise
 */
bool simd_binary_search(const char** words, size_t word_count, const char* target);

/**
 * @brief SIMD-optimized case-insensitive string compare
 * 
 * @param str1 First string
 * @param str2 Second string
 * @return Integer less than, equal to, or greater than zero if str1 is found 
 *         to be less than, equal to, or greater than str2
 */
int simd_strcasecmp(const char* str1, const char* str2);

/**
 * @brief SIMD-optimized memory fill with zeros
 * 
 * @param dst Destination memory
 * @param size Size in bytes
 */
void simd_memzero(void* dst, size_t size);

/**
 * @brief SIMD-optimized memory copy
 * 
 * @param dst Destination memory
 * @param src Source memory
 * @param size Size in bytes
 * @return Destination pointer
 */
void* simd_memcpy(void* dst, const void* src, size_t size);

/**
 * @brief Bloom filter for word list lookups
 * 
 * A bloom filter is a space-efficient probabilistic data structure
 * used to test whether an element is a member of a set. It can have
 * false positives but no false negatives.
 */
typedef struct {
    uint64_t* bits;
    size_t size;
    size_t hash_functions;
} bloom_filter_t;

/**
 * @brief Initialize a bloom filter
 * 
 * @param filter Bloom filter to initialize
 * @param expected_items Expected number of items
 * @param false_positive_rate Desired false positive rate (0.0-1.0)
 * @return true if successful
 */
bool bloom_filter_init(bloom_filter_t* filter, size_t expected_items, double false_positive_rate);

/**
 * @brief Add a word to the bloom filter
 * 
 * @param filter Filter to add to
 * @param word Word to add
 */
void bloom_filter_add(bloom_filter_t* filter, const char* word);

/**
 * @brief Check if a word might be in the bloom filter
 * 
 * @param filter Filter to check
 * @param word Word to check
 * @return true if word might be in the filter, false if definitely not
 */
bool bloom_filter_might_contain(const bloom_filter_t* filter, const char* word);

/**
 * @brief Free bloom filter resources
 * 
 * @param filter Filter to free
 */
void bloom_filter_cleanup(bloom_filter_t* filter);

#endif /* SIMD_UTILS_H */ 