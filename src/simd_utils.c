/**
 * @file simd_utils.c
 * @brief SIMD optimization utilities implementation for Ceed Parser
 *
 * This file implements SIMD-accelerated functions for word matching
 * and other performance-critical operations, with specializations
 * for both x86 (AVX/AVX2) and ARM (NEON) platforms.
 */

#include "../include/simd_utils.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(SIMD_ARCH_X86_64)
#include <cpuid.h>
#endif

/**
 * @brief Initialize SIMD features detection
 */
bool simd_detect_features(simd_features_t *features) {
  memset(features, 0, sizeof(simd_features_t));

  // Default cache line size
  features->cache_line_size = 64;
  features->vector_width = 16; // SSE width by default

#if defined(SIMD_ARCH_X86_64)
  uint32_t eax, ebx, ecx, edx;

  // Check for SSE4.1, SSE4.2
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    features->has_sse4_1 = (ecx & bit_SSE4_1) != 0;
    features->has_sse4_2 = (ecx & bit_SSE4_2) != 0;
  }

  // Check for AVX, AVX2
  if (__get_cpuid(7, &eax, &ebx, &ecx, &edx)) {
    features->has_avx = (ecx & bit_AVX) != 0;
    features->has_avx2 = (ebx & bit_AVX2) != 0;

    if (features->has_avx2) {
      features->vector_width = 32; // AVX/AVX2 width
    }
  }

  // Get cache line size
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    // CPUID.01H:EBX[15:8] is the Cache Line Size
    features->cache_line_size = ((ebx >> 8) & 0xff) * 8;
  }

#elif defined(SIMD_ARCH_ARM64)
  // ARM NEON is always available on ARM64
  features->has_neon = true;
  features->vector_width = 16; // NEON width

  // Cache line size is typically 64 bytes on ARM64
  features->cache_line_size = 64;

  // We could read this from system information if needed
  // For example, on Linux:
  // /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size
#endif

  return true;
}

/**
 * @brief Check if SIMD optimizations are available
 */
bool simd_available(void) {
  simd_features_t features;
  simd_detect_features(&features);

  return features.has_avx || features.has_avx2 || features.has_sse4_1 ||
         features.has_sse4_2 || features.has_neon;
}

/**
 * @brief SIMD-optimized string search implementation
 */
const char *simd_strstr(const char *haystack, const char *needle) {
  // Fall back to standard implementation if no SIMD is available
  if (!simd_available()) {
    return strstr(haystack, needle);
  }

  size_t needle_len = strlen(needle);

  // Use standard strstr for small strings
  if (needle_len < 4) {
    return strstr(haystack, needle);
  }

#if defined(SIMD_ARCH_X86_64) && defined(HAVE_AVX2)
  // Implementation for AVX2
  __m256i first = _mm256_set1_epi8(needle[0]);
  __m256i second = _mm256_set1_epi8(needle[1]);
  __m256i third = _mm256_set1_epi8(needle[2]);
  __m256i fourth = _mm256_set1_epi8(needle[3]);

  while (strlen(haystack) >= 32) {
    __m256i block = _mm256_loadu_si256((__m256i *)haystack);

    __m256i eq_first = _mm256_cmpeq_epi8(block, first);
    __m256i eq_second = _mm256_cmpeq_epi8(_mm256_srli_si256(block, 1), second);
    __m256i eq_third = _mm256_cmpeq_epi8(_mm256_srli_si256(block, 2), third);
    __m256i eq_fourth = _mm256_cmpeq_epi8(_mm256_srli_si256(block, 3), fourth);

    __m256i match = _mm256_and_si256(
        eq_first,
        _mm256_and_si256(eq_second, _mm256_and_si256(eq_third, eq_fourth)));

    uint32_t mask = _mm256_movemask_epi8(match);

    if (mask != 0) {
      // Found a potential match
      int pos = __builtin_ctz(mask);
      if (strncmp(haystack + pos, needle, needle_len) == 0) {
        return haystack + pos;
      }
    }

    haystack += 32 - 3; // Overlap to catch patterns that cross boundaries
  }

#elif defined(SIMD_ARCH_X86_64) && defined(HAVE_SSE4_2)
  // Use SSE4.2's string instructions
  const size_t len = strlen(haystack);
  if (len < needle_len) {
    return NULL;
  }

  for (size_t i = 0; i <= len - needle_len; i += 16) {
    size_t block_len = (len - i >= 16) ? 16 : len - i;

    // Use _mm_cmpestri to search for needle
    const int mode = _SIDD_CMP_EQUAL_ORDERED | _SIDD_UBYTE_OPS;
    int idx = _mm_cmpestri(_mm_loadu_si128((__m128i *)(needle)), needle_len,
                           _mm_loadu_si128((__m128i *)(haystack + i)),
                           block_len, mode);

    if (idx < 16) {
      // Verify the match
      if (strncmp(haystack + i + idx, needle, needle_len) == 0) {
        return haystack + i + idx;
      }
    }
  }

#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
  // Implementation for ARM NEON
  uint8x16_t v_first = vdupq_n_u8(needle[0]);
  uint8x16_t v_second = vdupq_n_u8(needle[1]);
  uint8x16_t v_third = vdupq_n_u8(needle[2]);
  uint8x16_t v_fourth = vdupq_n_u8(needle[3]);

  while (strlen(haystack) >= 16) {
    uint8x16_t block = vld1q_u8((const uint8_t *)haystack);

    uint8x16_t eq_first = vceqq_u8(block, v_first);

    // Extract potentially matching positions
    uint64x2_t match_long = vreinterpretq_u64_u8(eq_first);
    uint64_t match_hi = vgetq_lane_u64(match_long, 1);
    uint64_t match_lo = vgetq_lane_u64(match_long, 0);
    uint64_t match_mask = match_hi | match_lo;

    // If there's any potential match
    if (match_mask) {
      for (int pos = 0; pos < 16; pos++) {
        if (haystack[pos] == needle[0]) {
          if (strncmp(haystack + pos, needle, needle_len) == 0) {
            return haystack + pos;
          }
        }
      }
    }

    haystack += 13; // Overlap to catch patterns that cross boundaries
  }
#endif

  // Fall back to standard impl for the remaining part
  return strstr(haystack, needle);
}

/**
 * @brief SIMD-optimized string compare
 */
int simd_strcmp(const char *str1, const char *str2) {
  // Fall back to standard implementation if no SIMD is available
  if (!simd_available()) {
    return strcmp(str1, str2);
  }

#if defined(SIMD_ARCH_X86_64) && defined(HAVE_AVX2)
  // Implementation for AVX2
  while (1) {
    __m256i a = _mm256_loadu_si256((__m256i *)str1);
    __m256i b = _mm256_loadu_si256((__m256i *)str2);

    __m256i eq = _mm256_cmpeq_epi8(a, b);
    uint32_t mask = ~_mm256_movemask_epi8(eq);

    if (mask != 0) {
      // Found a difference
      int pos = __builtin_ctz(mask);
      return (int)str1[pos] - (int)str2[pos];
    }

    // Check for null terminator
    __m256i zero = _mm256_setzero_si256();
    __m256i has_zero = _mm256_cmpeq_epi8(a, zero);
    if (_mm256_movemask_epi8(has_zero) != 0) {
      return 0; // Strings are equal
    }

    str1 += 32;
    str2 += 32;
  }

#elif defined(SIMD_ARCH_X86_64) && defined(HAVE_SSE4_1)
  // Implementation for SSE4.1
  while (1) {
    __m128i a = _mm_loadu_si128((__m128i *)str1);
    __m128i b = _mm_loadu_si128((__m128i *)str2);

    __m128i eq = _mm_cmpeq_epi8(a, b);
    uint16_t mask = ~_mm_movemask_epi8(eq);

    if (mask != 0) {
      // Found a difference
      int pos = __builtin_ctz(mask);
      return (int)str1[pos] - (int)str2[pos];
    }

    // Check for null terminator
    __m128i zero = _mm_setzero_si128();
    __m128i has_zero = _mm_cmpeq_epi8(a, zero);
    if (_mm_movemask_epi8(has_zero) != 0) {
      return 0; // Strings are equal
    }

    str1 += 16;
    str2 += 16;
  }

#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
  // Implementation for ARM NEON
  while (1) {
    uint8x16_t a = vld1q_u8((const uint8_t *)str1);
    uint8x16_t b = vld1q_u8((const uint8_t *)str2);

    uint8x16_t eq = vceqq_u8(a, b);
    uint8x16_t zero = vdupq_n_u8(0);
    uint8x16_t has_zero = vceqq_u8(a, zero);

    // Check if all bytes are equal
    uint64x2_t eq_merged =
        vpaddq_u64(vreinterpretq_u64_u8(eq), vreinterpretq_u64_u8(eq));
    if (vgetq_lane_u64(eq_merged, 0) != 0xFFFFFFFFFFFFFFFF) {
      // There is a difference, find it
      for (int i = 0; i < 16; i++) {
        if (str1[i] != str2[i]) {
          return (int)str1[i] - (int)str2[i];
        }
        if (str1[i] == '\0') {
          return 0;
        }
      }
    }

    // Check for null terminator
    uint64x2_t zero_merged = vpaddq_u64(vreinterpretq_u64_u8(has_zero),
                                        vreinterpretq_u64_u8(has_zero));
    if (vgetq_lane_u64(zero_merged, 0) != 0) {
      return 0; // Strings are equal
    }

    str1 += 16;
    str2 += 16;
  }
#endif

  // Fall back to standard implementation for platforms without SIMD
  return strcmp(str1, str2);
}

/**
 * @brief SIMD-optimized binary search for word list lookups
 */
bool simd_binary_search(const char **words, size_t word_count,
                        const char *target) {
  size_t left = 0;
  size_t right = word_count - 1;

  while (left <= right) {
    size_t mid = left + (right - left) / 2;
    int result = simd_strcmp(target, words[mid]);

    if (result == 0) {
      return true; // Word found
    }

    if (result < 0) {
      if (mid == 0) {
        return false;
      }
      right = mid - 1;
    } else {
      left = mid + 1;
    }
  }

  return false; // Word not found
}

/**
 * @brief SIMD-optimized case-insensitive string compare
 */
int simd_strcasecmp(const char *str1, const char *str2) {
  // Fall back to standard implementation if no SIMD is available
  if (!simd_available()) {
    return strcasecmp(str1, str2);
  }

#if defined(SIMD_ARCH_X86_64) && defined(HAVE_AVX2)
  // Implementation for AVX2
  __m256i mask_az = _mm256_set1_epi8(0xDF);

  while (1) {
    __m256i a = _mm256_loadu_si256((__m256i *)str1);
    __m256i b = _mm256_loadu_si256((__m256i *)str2);

    // Convert to uppercase for case-insensitive comparison
    __m256i a_upper = _mm256_and_si256(a, mask_az);
    __m256i b_upper = _mm256_and_si256(b, mask_az);

    __m256i eq = _mm256_cmpeq_epi8(a_upper, b_upper);
    uint32_t mask = ~_mm256_movemask_epi8(eq);

    if (mask != 0) {
      // Found a difference
      int pos = __builtin_ctz(mask);
      return (int)((str1[pos] & 0xDF) - (str2[pos] & 0xDF));
    }

    // Check for null terminator
    __m256i zero = _mm256_setzero_si256();
    __m256i has_zero = _mm256_cmpeq_epi8(a, zero);
    if (_mm256_movemask_epi8(has_zero) != 0) {
      return 0; // Strings are equal
    }

    str1 += 32;
    str2 += 32;
  }

#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
  // Implementation for ARM NEON
  uint8x16_t mask_az = vdupq_n_u8(0xDF);

  while (1) {
    uint8x16_t a = vld1q_u8((const uint8_t *)str1);
    uint8x16_t b = vld1q_u8((const uint8_t *)str2);

    // Convert to uppercase for case-insensitive comparison
    uint8x16_t a_upper = vandq_u8(a, mask_az);
    uint8x16_t b_upper = vandq_u8(b, mask_az);

    uint8x16_t eq = vceqq_u8(a_upper, b_upper);
    uint8x16_t zero = vdupq_n_u8(0);
    uint8x16_t has_zero = vceqq_u8(a, zero);

    // Check if all bytes are equal
    uint64x2_t eq_merged =
        vpaddq_u64(vreinterpretq_u64_u8(eq), vreinterpretq_u64_u8(eq));
    if (vgetq_lane_u64(eq_merged, 0) != 0xFFFFFFFFFFFFFFFF) {
      // There is a difference, find it
      for (int i = 0; i < 16; i++) {
        char c1 = str1[i] & 0xDF;
        char c2 = str2[i] & 0xDF;
        if (c1 != c2) {
          return (int)c1 - (int)c2;
        }
        if (str1[i] == '\0') {
          return 0;
        }
      }
    }

    // Check for null terminator
    uint64x2_t zero_merged = vpaddq_u64(vreinterpretq_u64_u8(has_zero),
                                        vreinterpretq_u64_u8(has_zero));
    if (vgetq_lane_u64(zero_merged, 0) != 0) {
      return 0; // Strings are equal
    }

    str1 += 16;
    str2 += 16;
  }
#endif

  // Fall back to standard implementation
  return strcasecmp(str1, str2);
}

/**
 * @brief SIMD-optimized memory fill with zeros
 */
void simd_memzero(void *dst, size_t size) {
  // Fall back to standard implementation if no SIMD is available
  if (!simd_available() || size < 16) {
    memset(dst, 0, size);
    return;
  }

#if defined(SIMD_ARCH_X86_64) && defined(HAVE_AVX2)
  // Implementation for AVX2
  __m256i zero = _mm256_setzero_si256();

  // Align to 32-byte boundary
  size_t misalign = (size_t)dst & 0x1F;
  if (misalign != 0) {
    size_t presize = 32 - misalign;
    if (presize > size) {
      presize = size;
    }
    memset(dst, 0, presize);
    dst = (char *)dst + presize;
    size -= presize;
  }

  // Process 32-byte blocks
  while (size >= 32) {
    _mm256_store_si256((__m256i *)dst, zero);
    dst = (char *)dst + 32;
    size -= 32;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memset(dst, 0, size);
  }

#elif defined(SIMD_ARCH_X86_64) && defined(HAVE_SSE4_1)
  // Implementation for SSE4.1
  __m128i zero = _mm_setzero_si128();

  // Align to 16-byte boundary
  size_t misalign = (size_t)dst & 0xF;
  if (misalign != 0) {
    size_t presize = 16 - misalign;
    if (presize > size) {
      presize = size;
    }
    memset(dst, 0, presize);
    dst = (char *)dst + presize;
    size -= presize;
  }

  // Process 16-byte blocks
  while (size >= 16) {
    _mm_store_si128((__m128i *)dst, zero);
    dst = (char *)dst + 16;
    size -= 16;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memset(dst, 0, size);
  }

#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
  // Implementation for ARM NEON
  uint8x16_t zero = vdupq_n_u8(0);

  // Align to 16-byte boundary
  size_t misalign = (size_t)dst & 0xF;
  if (misalign != 0) {
    size_t presize = 16 - misalign;
    if (presize > size) {
      presize = size;
    }
    memset(dst, 0, presize);
    dst = (char *)dst + presize;
    size -= presize;
  }

  // Process 16-byte blocks
  while (size >= 16) {
    vst1q_u8((uint8_t *)dst, zero);
    dst = (char *)dst + 16;
    size -= 16;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memset(dst, 0, size);
  }
#else
  // Fall back to standard implementation
  memset(dst, 0, size);
#endif
}

/**
 * @brief SIMD-optimized memory copy
 */
void *simd_memcpy(void *dst, const void *src, size_t size) {
  // Fall back to standard implementation if no SIMD is available
  if (!simd_available() || size < 16) {
    return memcpy(dst, src, size);
  }

  // Get original destination for return value
  void *original_dst = dst;

#if defined(SIMD_ARCH_X86_64) && defined(HAVE_AVX2)
  // Implementation for AVX2

  // Handle small copies and misaligned start
  if (size < 32) {
    return memcpy(dst, src, size);
  }

  // Prefetch source data
  _mm_prefetch((const char *)src, _MM_HINT_T0);
  _mm_prefetch((const char *)src + 64, _MM_HINT_T0);

  // Process 32-byte blocks
  while (size >= 32) {
    _mm256_storeu_si256((__m256i *)dst, _mm256_loadu_si256((__m256i *)src));
    dst = (char *)dst + 32;
    src = (const char *)src + 32;
    size -= 32;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memcpy(dst, src, size);
  }

#elif defined(SIMD_ARCH_X86_64) && defined(HAVE_SSE4_1)
  // Implementation for SSE4.1

  // Handle small copies and misaligned start
  if (size < 16) {
    return memcpy(dst, src, size);
  }

  // Prefetch source data
  _mm_prefetch((const char *)src, _MM_HINT_T0);
  _mm_prefetch((const char *)src + 64, _MM_HINT_T0);

  // Process 16-byte blocks
  while (size >= 16) {
    _mm_storeu_si128((__m128i *)dst, _mm_loadu_si128((__m128i *)src));
    dst = (char *)dst + 16;
    src = (const char *)src + 16;
    size -= 16;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memcpy(dst, src, size);
  }

#elif defined(SIMD_ARCH_ARM64) && defined(HAVE_NEON)
  // Implementation for ARM NEON

  // Handle small copies and misaligned start
  if (size < 16) {
    return memcpy(dst, src, size);
  }

  // Prefetch source data
  __builtin_prefetch(src, 0, 3);
  __builtin_prefetch((const char *)src + 64, 0, 3);

  // Process 16-byte blocks
  while (size >= 16) {
    vst1q_u8((uint8_t *)dst, vld1q_u8((const uint8_t *)src));
    dst = (char *)dst + 16;
    src = (const char *)src + 16;
    size -= 16;
  }

  // Handle the remaining bytes
  if (size > 0) {
    memcpy(dst, src, size);
  }
#else
  // Fall back to standard implementation
  memcpy(dst, src, size);
#endif

  return original_dst;
}

/*
 * Bloom filter implementation
 */

// Hash function 1: Jenkins hash
static uint32_t bloom_hash1(const char *str) {
  uint32_t hash = 0;

  while (*str) {
    hash += *str++;
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }

  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);

  return hash;
}

// Hash function 2: DJB2 hash
static uint32_t bloom_hash2(const char *str) {
  uint32_t hash = 5381;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

// Hash function 3: Murmur3 hash (simplified)
static uint32_t __attribute__((unused)) bloom_hash3(const char *str) {
  uint32_t h = 0x12345678;
  for (; *str; ++str) {
    h ^= *str;
    h *= 0x5bd1e995;
    h ^= h >> 15;
  }
  return h;
}

/**
 * @brief Create a bloom filter
 */
bloom_filter_t bloom_filter_create(size_t size, double error_rate) {
  bloom_filter_t filter;
  memset(&filter, 0, sizeof(bloom_filter_t));

  // Calculate optimal number of hash functions based on error rate
  // Using formula: k = -log2(error_rate)
  size_t hash_functions = (size_t)(-log2(error_rate));
  if (hash_functions < 1)
    hash_functions = 1;

  // Round up size to the next multiple of 64 bits
  size = ((size + 63) / 64) * 64;

  // Allocate the bit array
  filter.bits = (uint64_t *)calloc(size / 64, sizeof(uint64_t));
  if (!filter.bits) {
    // Return empty filter on allocation failure
    return filter;
  }

  filter.size = size;
  filter.hash_funcs = hash_functions;
  filter.error_rate = error_rate;

  return filter;
}

/**
 * @brief Add a word to the bloom filter
 */
void bloom_filter_add(bloom_filter_t *filter, const void *item, size_t size) {
  // Hash the item using our hash functions
  const char *data = (const char *)item;
  uint32_t hash1 = 0;
  uint32_t hash2 = 0;

  // If it's a string (null-terminated), use our string hash functions
  if (size == 0 && data != NULL) {
    hash1 = bloom_hash1(data);
    hash2 = bloom_hash2(data);
  } else {
    // Simple hash for binary data
    for (size_t i = 0; i < size; i++) {
      hash1 = hash1 * 31 + data[i];
      hash2 = hash2 * 37 + data[i];
    }
  }

  for (size_t i = 0; i < filter->hash_funcs; i++) {
    uint32_t hash = (hash1 + i * hash2) % filter->size;
    filter->bits[hash / 64] |= (1ULL << (hash % 64));
  }
}

/**
 * @brief Check if a word might be in the bloom filter
 */
bool bloom_filter_check(const bloom_filter_t *filter, const void *item,
                        size_t size) {
  // Hash the item using our hash functions
  const char *data = (const char *)item;
  uint32_t hash1 = 0;
  uint32_t hash2 = 0;

  // If it's a string (null-terminated), use our string hash functions
  if (size == 0 && data != NULL) {
    hash1 = bloom_hash1(data);
    hash2 = bloom_hash2(data);
  } else {
    // Simple hash for binary data
    for (size_t i = 0; i < size; i++) {
      hash1 = hash1 * 31 + data[i];
      hash2 = hash2 * 37 + data[i];
    }
  }

  for (size_t i = 0; i < filter->hash_funcs; i++) {
    uint32_t hash = (hash1 + i * hash2) % filter->size;
    if (!(filter->bits[hash / 64] & (1ULL << (hash % 64)))) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Free bloom filter resources
 */
void bloom_filter_destroy(bloom_filter_t *filter) {
  if (filter->bits) {
    free(filter->bits);
    filter->bits = NULL;
  }

  filter->size = 0;
  filter->hash_funcs = 0;
}