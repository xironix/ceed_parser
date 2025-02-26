/**
 * @file memory_pool.h
 * @brief High-performance thread-local memory pool
 *
 * This file provides a thread-local memory pool implementation
 * for high-performance memory allocation and deallocation.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

// Default alignment for memory allocations
#define ALIGNMENT 16

// Default block size for the memory pool (64KB)
#define DEFAULT_BLOCK_SIZE (64 * 1024)

// Default maximum number of blocks
#define DEFAULT_MAX_BLOCKS 256

// Default small object size threshold
#define DEFAULT_SMALL_SIZE 256

// Default small object capacity per block
#define DEFAULT_SMALL_CAPACITY 1024

// Forward declaration
typedef struct memory_pool memory_pool_t;

// Memory block structure
typedef struct memory_block {
    char* memory;                       // Allocated memory
    char* data;                         // Pointer to the data area (same as memory, for API consistency)
    size_t size;                        // Size of allocated memory
    size_t used;                        // Amount of used memory
    struct memory_block* next;          // Next block in the list
} memory_block_t;

// Small memory block structure
typedef struct small_block {
    char* memory;                       // Allocated memory
    char* data;                         // Pointer to the data area
    size_t object_size;                 // Size of each object
    size_t capacity;                    // Maximum number of objects
    size_t used;                        // Number of used objects
    uint8_t* bitmap;                    // Bitmap of used objects
    struct small_block* next;           // Next small block
} small_block_t;

// Memory pool structure
struct memory_pool {
    memory_block_t* blocks;             // List of memory blocks
    memory_block_t* current_block;      // Current block for allocations
    small_block_t* small_blocks;        // List of small memory blocks
    size_t block_size;                  // Size of each memory block
    size_t max_blocks;                  // Maximum number of blocks
    size_t small_size;                  // Maximum size for small objects
    size_t small_capacity;              // Number of small objects per block
    size_t total_allocated;             // Total memory allocated
    size_t max_allocated;               // Maximum memory allocated
    size_t total_used;                  // Total memory used
    size_t block_count;                 // Number of blocks
    size_t small_block_count;           // Number of small blocks
    size_t allocations;                 // Number of allocations
    size_t num_allocs;                  // Number of allocations (alias)
    size_t num_frees;                   // Number of deallocations
    size_t small_allocations;           // Number of small allocations
    size_t small_used;                  // Number of small blocks used
    size_t cache_misses;                // Number of cache misses
    size_t wasted;                      // Amount of wasted memory
    bool enable_stats;                  // Enable statistics
    pthread_key_t tls_key;              // Thread-local storage key
};

// Memory pool statistics structure
typedef struct {
    size_t total_allocated;             // Total memory allocated
    size_t total_used;                  // Total memory used
    size_t block_size;                  // Size of each memory block
    size_t block_count;                 // Number of blocks
    size_t small_block_count;           // Number of small blocks
    size_t allocations;                 // Number of allocations
    size_t small_allocations;           // Number of small allocations
    size_t cache_misses;                // Number of cache misses
    size_t wasted;                      // Amount of wasted memory
    double fragmentation;               // Fragmentation ratio
    double efficiency;                  // Memory efficiency ratio
} memory_pool_stats_t;

/**
 * @brief Create a memory pool
 * 
 * @param block_size Size of each memory block
 * @param max_blocks Maximum number of blocks
 * @return Pointer to the created memory pool, or NULL on failure
 */
memory_pool_t* memory_pool_create(size_t block_size, size_t max_blocks);

/**
 * @brief Destroy a memory pool
 * 
 * @param pool Memory pool to destroy
 */
void memory_pool_destroy(memory_pool_t* pool);

/**
 * @brief Reset a memory pool
 * 
 * @param pool Memory pool to reset
 */
void memory_pool_reset(memory_pool_t* pool);

/**
 * @brief Allocate memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param size Size of memory to allocate
 * @return Pointer to the allocated memory, or NULL on failure
 */
void* memory_pool_malloc(memory_pool_t* pool, size_t size);

/**
 * @brief Allocate aligned memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param size Size of memory to allocate
 * @param alignment Alignment of memory
 * @return Pointer to the allocated memory, or NULL on failure
 */
void* memory_pool_aligned_malloc(memory_pool_t* pool, size_t size, size_t alignment);

/**
 * @brief Allocate and zero-initialize memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param nmemb Number of elements
 * @param size Size of each element
 * @return Pointer to the allocated memory, or NULL on failure
 */
void* memory_pool_calloc(memory_pool_t* pool, size_t nmemb, size_t size);

/**
 * @brief Duplicate a string using memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param str String to duplicate
 * @return Pointer to the duplicated string, or NULL on failure
 */
char* memory_pool_strdup(memory_pool_t* pool, const char* str);

/**
 * @brief Free memory allocated from a memory pool
 * 
 * @param pool Memory pool to free from
 * @param ptr Pointer to the memory to free
 */
void memory_pool_free(memory_pool_t* pool, void* ptr);

/**
 * @brief Get detailed statistics about a memory pool
 * 
 * @param pool Pointer to the memory pool
 * @param total_allocated Pointer to store the total allocated memory
 * @param max_allocated Pointer to store the maximum allocated memory
 * @param num_allocs Pointer to store the number of allocations
 * @param num_frees Pointer to store the number of deallocations
 * @param cache_misses Pointer to store the number of cache misses
 */
void memory_pool_get_detailed_stats(memory_pool_t* pool, size_t* total_allocated, 
                           size_t* max_allocated, size_t* num_allocs, 
                           size_t* num_frees, size_t* cache_misses);

/**
 * @brief Get statistics about a memory pool
 * 
 * @param pool Pointer to the memory pool
 * @param stats Pointer to the stats structure to fill
 */
void memory_pool_get_stats(memory_pool_t* pool, memory_pool_stats_t* stats);

/**
 * @brief Get the thread-local memory pool
 * 
 * This function returns the thread-local memory pool for the current thread.
 * If the pool doesn't exist, it creates a new one.
 * 
 * @return Pointer to the thread-local memory pool
 */
memory_pool_t* memory_pool_get_thread_local(void);

/**
 * @brief Set the thread-local memory pool
 * 
 * @param pool Memory pool to set as the thread-local pool
 */
void memory_pool_set_thread_local(memory_pool_t* pool);

/**
 * @brief Destroy the thread-local memory pool
 */
void memory_pool_destroy_thread_local(void);

// Macros for using the thread-local memory pool
#ifdef MEMORY_POOL_ENABLE_THREAD_LOCAL
    #define mp_malloc(size) memory_pool_malloc(memory_pool_get_thread_local(), size)
    #define mp_aligned_malloc(size, alignment) memory_pool_aligned_malloc(memory_pool_get_thread_local(), size, alignment)
    #define mp_calloc(nmemb, size) memory_pool_calloc(memory_pool_get_thread_local(), nmemb, size)
    #define mp_strdup(str) memory_pool_strdup(memory_pool_get_thread_local(), str)
    #define mp_free(ptr) memory_pool_free(memory_pool_get_thread_local(), ptr)
#else
    #define mp_malloc(size) malloc(size)
    #define mp_aligned_malloc(size, alignment) aligned_alloc(alignment, size)
    #define mp_calloc(nmemb, size) calloc(nmemb, size)
    #define mp_strdup(str) strdup(str)
    #define mp_free(ptr) free(ptr)
#endif

#endif // MEMORY_POOL_H 