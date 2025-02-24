/**
 * @file memory_pool.h
 * @brief High-performance thread-local memory pool/allocator
 *
 * This file provides a thread-local memory pool implementation
 * that significantly reduces the overhead of frequent allocations
 * and deallocations, especially for small objects.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

// Disable memory pool implementation for debugging 
// #define DISABLE_MEMORY_POOL

#ifndef DISABLE_MEMORY_POOL

/**
 * Default values for memory pool configuration
 */
#define DEFAULT_BLOCK_SIZE       (4 * 1024)      // 4KB blocks
#define DEFAULT_MAX_BLOCKS       1024            // Maximum blocks per pool
#define DEFAULT_SMALL_SIZE       128             // Max size for small object pool
#define DEFAULT_SMALL_CAPACITY   1024            // Capacity of small object pool
#define ALIGNMENT                16              // Memory alignment

/**
 * Memory block structure
 */
typedef struct memory_block {
    size_t size;                     // Block size
    size_t used;                     // Used bytes
    struct memory_block* next;       // Next block
    char data[];                     // Block data (flexible array)
} memory_block_t;

/**
 * Pre-sized block for small allocations (fewer than 128 bytes)
 */
typedef struct small_block {
    bool used;                       // Whether the block is in use
    char data[DEFAULT_SMALL_SIZE];   // Block data
} small_block_t;

/**
 * Memory pool structure
 */
typedef struct memory_pool {
    size_t block_size;               // Size of each block
    size_t max_blocks;               // Maximum number of blocks
    size_t block_count;              // Current number of blocks
    size_t total_allocated;          // Total allocated memory
    size_t max_allocated;            // Maximum memory ever allocated
    
    memory_block_t* current_block;   // Current block for allocations
    memory_block_t* blocks;          // List of blocks
    
    // Pool of pre-sized blocks for small allocations
    small_block_t* small_blocks;     // Array of small blocks
    size_t small_capacity;           // Capacity of small blocks array
    size_t small_used;               // Number of used small blocks
    
    // Thread-local storage key
    pthread_key_t tls_key;
    
    // Performance statistics
    size_t num_allocs;               // Number of allocations
    size_t num_frees;                // Number of frees
    size_t cache_misses;             // Number of cache misses
} memory_pool_t;

/**
 * @brief Initialize a memory pool
 * 
 * @param pool Memory pool to initialize
 * @param block_size Size of each block
 * @param max_blocks Maximum number of blocks
 * @param small_capacity Capacity of small object pool
 * @return true if successful
 */
bool memory_pool_init(memory_pool_t* pool, size_t block_size, size_t max_blocks, size_t small_capacity);

/**
 * @brief Destroy a memory pool and free all memory
 * 
 * @param pool Memory pool to destroy
 */
void memory_pool_destroy(memory_pool_t* pool);

/**
 * @brief Reset a memory pool, keeping allocated blocks
 * 
 * This function resets the memory pool to its initial state,
 * but keeps the allocated blocks for reuse. This is faster
 * than destroying and recreating the pool.
 * 
 * @param pool Memory pool to reset
 */
void memory_pool_reset(memory_pool_t* pool);

/**
 * @brief Allocate memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param size Size to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* memory_pool_alloc(memory_pool_t* pool, size_t size);

/**
 * @brief Allocate aligned memory from a memory pool
 * 
 * @param pool Memory pool to allocate from
 * @param size Size to allocate
 * @param alignment Alignment boundary
 * @return Pointer to allocated memory, or NULL on failure
 */
void* memory_pool_aligned_alloc(memory_pool_t* pool, size_t size, size_t alignment);

/**
 * @brief Free memory allocated from a memory pool
 * 
 * Note: This only truly frees memory for large allocations.
 * Small allocations are returned to the pool for reuse.
 * 
 * @param pool Memory pool the memory was allocated from
 * @param ptr Pointer to memory to free
 */
void memory_pool_free(memory_pool_t* pool, void* ptr);

/**
 * @brief Allocate memory from the thread-local memory pool
 * 
 * @param size Size to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void* tls_pool_alloc(size_t size);

/**
 * @brief Free memory allocated from the thread-local memory pool
 * 
 * @param ptr Pointer to memory to free
 */
void tls_pool_free(void* ptr);

/**
 * @brief Initialize the thread-local memory pool for the current thread
 */
void tls_pool_init_thread(void);

/**
 * @brief Clean up the thread-local memory pool for the current thread
 */
void tls_pool_cleanup_thread(void);

/**
 * @brief Initialize the global thread-local memory pool system
 * 
 * @return true if successful
 */
bool tls_pool_init(void);

/**
 * @brief Destroy the global thread-local memory pool system
 */
void tls_pool_destroy(void);

/**
 * @brief Get statistics about the thread-local memory pool
 * 
 * @param total_allocated Pointer to store total allocated memory
 * @param max_allocated Pointer to store maximum allocated memory
 * @param num_allocs Pointer to store number of allocations
 * @param num_frees Pointer to store number of frees
 * @param cache_misses Pointer to store number of cache misses
 */
void tls_pool_get_stats(size_t* total_allocated, size_t* max_allocated,
                        size_t* num_allocs, size_t* num_frees,
                        size_t* cache_misses);

/**
 * Wrapper macros for standard memory functions
 */
#define fast_malloc(size)              tls_pool_alloc(size)
#define fast_free(ptr)                 tls_pool_free(ptr)
#define fast_calloc(count, size)       memset(tls_pool_alloc((count) * (size)), 0, (count) * (size))
#define fast_realloc(ptr, size)        realloc(ptr, size)  // Falls back to standard realloc
#define fast_strdup(str)               strcpy(tls_pool_alloc(strlen(str) + 1), str)

#else  // DISABLE_MEMORY_POOL

/**
 * If memory pool is disabled, use standard memory functions
 */
#define fast_malloc(size)              malloc(size)
#define fast_free(ptr)                 free(ptr)
#define fast_calloc(count, size)       calloc(count, size)
#define fast_realloc(ptr, size)        realloc(ptr, size)
#define fast_strdup(str)               strdup(str)

#define tls_pool_init()                true
#define tls_pool_destroy()             ((void)0)
#define tls_pool_init_thread()         ((void)0)
#define tls_pool_cleanup_thread()      ((void)0)
#define tls_pool_get_stats(a,b,c,d,e)  ((void)0)

#endif  // DISABLE_MEMORY_POOL

#endif  // MEMORY_POOL_H 