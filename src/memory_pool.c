/**
 * @file memory_pool.c
 * @brief High-performance thread-local memory pool/allocator
 *
 * Implementation of the memory pool for efficient memory management in
 * performance-critical applications.
 */

#include "memory_pool.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DISABLE_MEMORY_POOL

// Global memory pool for thread-local storage
static memory_pool_t *g_global_pool = NULL;
static pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;

/**
 * @brief Round up to the next multiple of alignment
 */
static inline size_t align_size(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Initialize a memory pool
 */
bool memory_pool_init(memory_pool_t *pool, size_t block_size, size_t max_blocks,
                      size_t small_capacity) {
  if (!pool) {
    return false;
  }

  // Initialize pool structure
  memset(pool, 0, sizeof(memory_pool_t));
  pool->block_size = block_size > 0 ? block_size : DEFAULT_BLOCK_SIZE;
  pool->max_blocks = max_blocks > 0 ? max_blocks : DEFAULT_MAX_BLOCKS;
  pool->small_capacity =
      small_capacity > 0 ? small_capacity : DEFAULT_SMALL_CAPACITY;

  // Allocate small blocks array
  pool->small_blocks =
      (small_block_t *)calloc(pool->small_capacity, sizeof(small_block_t));
  if (!pool->small_blocks) {
    return false;
  }

  // Allocate first block
  pool->blocks =
      (memory_block_t *)malloc(sizeof(memory_block_t) + pool->block_size);
  if (!pool->blocks) {
    free(pool->small_blocks);
    return false;
  }

  // Initialize first block
  pool->blocks->size = pool->block_size;
  pool->blocks->used = 0;
  pool->blocks->next = NULL;
  pool->current_block = pool->blocks;
  pool->block_count = 1;

  // Initialize stats
  pool->total_allocated = sizeof(memory_block_t) + pool->block_size +
                          pool->small_capacity * sizeof(small_block_t);
  pool->max_allocated = pool->total_allocated;

  return true;
}

/**
 * @brief Destroy a memory pool and free all memory
 */
void memory_pool_destroy(memory_pool_t *pool) {
  if (!pool) {
    return;
  }

  // Free all blocks
  memory_block_t *block = pool->blocks;
  while (block) {
    memory_block_t *next = block->next;
    free(block);
    block = next;
  }

  // Free small blocks array
  if (pool->small_blocks) {
    free(pool->small_blocks);
  }

  // Reset pool
  memset(pool, 0, sizeof(memory_pool_t));
}

/**
 * @brief Reset a memory pool, keeping allocated blocks
 */
void memory_pool_reset(memory_pool_t *pool) {
  if (!pool) {
    return;
  }

  // Reset all blocks
  memory_block_t *block = pool->blocks;
  while (block) {
    block->used = 0;
    block = block->next;
  }

  // Reset current block to first block
  pool->current_block = pool->blocks;

  // Reset all small blocks
  for (size_t i = 0; i < pool->small_capacity; i++) {
    pool->small_blocks[i].used = false;
  }

  pool->small_used = 0;

  // Keep total_allocated and max_allocated for statistics
}

/**
 * @brief Allocate a new block for a memory pool
 */
static memory_block_t *memory_pool_allocate_block(memory_pool_t *pool,
                                                  size_t min_size) {
  // Check if we have reached the maximum number of blocks
  if (pool->block_count >= pool->max_blocks) {
    return NULL;
  }

  // Determine the size of the new block (max of min_size and block_size)
  size_t block_size = min_size > pool->block_size ? min_size : pool->block_size;

  // Allocate new block
  memory_block_t *block =
      (memory_block_t *)malloc(sizeof(memory_block_t) + block_size);
  if (!block) {
    return NULL;
  }

  // Initialize new block
  block->size = block_size;
  block->used = 0;
  block->next = NULL;

  // Add to the list of blocks
  memory_block_t *last = pool->blocks;
  while (last->next) {
    last = last->next;
  }
  last->next = block;

  // Update statistics
  pool->block_count++;
  pool->total_allocated += sizeof(memory_block_t) + block_size;
  if (pool->total_allocated > pool->max_allocated) {
    pool->max_allocated = pool->total_allocated;
  }

  return block;
}

/**
 * @brief Allocate memory from a memory pool
 */
void *memory_pool_alloc(memory_pool_t *pool, size_t size) {
  if (!pool || size == 0) {
    return NULL;
  }

  // Update statistics
  pool->num_allocs++;

  // Align the size to ensure proper alignment
  size = align_size(size, ALIGNMENT);

  // Check if the allocation fits in the small block pool
  if (size <= DEFAULT_SMALL_SIZE) {
    // Find an unused small block
    for (size_t i = 0; i < pool->small_capacity; i++) {
      if (!pool->small_blocks[i].used) {
        pool->small_blocks[i].used = true;
        pool->small_used++;
        return pool->small_blocks[i].data;
      }
    }
    // No free small blocks, continue with normal allocation
    pool->cache_misses++;
  }

  // Check if the current block has enough space
  if (!pool->current_block ||
      pool->current_block->used + size > pool->current_block->size) {
    // Try to find a block with enough space
    memory_block_t *block = pool->blocks;
    while (block) {
      if (block->used + size <= block->size) {
        pool->current_block = block;
        break;
      }
      block = block->next;
    }

    // If no block has enough space, allocate a new one
    if (!pool->current_block ||
        pool->current_block->used + size > pool->current_block->size) {
      memory_block_t *new_block = memory_pool_allocate_block(pool, size);
      if (!new_block) {
        return NULL; // Failed to allocate new block
      }
      pool->current_block = new_block;
    }
  }

  // Allocate from the current block
  void *ptr = pool->current_block->data + pool->current_block->used;
  pool->current_block->used += size;

  return ptr;
}

/**
 * @brief Allocate aligned memory from a memory pool
 */
void *memory_pool_aligned_alloc(memory_pool_t *pool, size_t size,
                                size_t alignment) {
  if (!pool || size == 0) {
    return NULL;
  }

  // Update statistics
  pool->num_allocs++;

  // Check if this is a small allocation and alignment is compatible
  if (size <= DEFAULT_SMALL_SIZE && alignment <= ALIGNMENT) {
    return memory_pool_alloc(pool, size);
  }

  // Ensure alignment is a power of 2
  if ((alignment & (alignment - 1)) != 0) {
    alignment = ALIGNMENT; // Fall back to default alignment
  }

  // Find a block with enough space for the aligned allocation
  memory_block_t *block = pool->blocks;
  void *ptr = NULL;

  while (block) {
    // Calculate aligned address
    uintptr_t addr = (uintptr_t)(block->data + block->used);
    uintptr_t aligned_addr = align_size(addr, alignment);
    size_t padding = aligned_addr - addr;

    // Check if there is enough space in this block
    if (block->used + padding + size <= block->size) {
      // Allocate from this block
      block->used += padding; // Skip to aligned address
      ptr = block->data + block->used;
      block->used += size;
      pool->current_block = block;
      return ptr;
    }

    block = block->next;
  }

  // No existing block has enough space, allocate a new one
  size_t block_size = size + alignment - 1; // Ensure enough space for alignment
  memory_block_t *new_block = memory_pool_allocate_block(pool, block_size);
  if (!new_block) {
    return NULL; // Failed to allocate new block
  }

  // Calculate aligned address in the new block
  uintptr_t addr = (uintptr_t)(new_block->data);
  uintptr_t aligned_addr = align_size(addr, alignment);
  size_t padding = aligned_addr - addr;

  // Allocate from the new block
  new_block->used += padding; // Skip to aligned address
  ptr = new_block->data + new_block->used;
  new_block->used += size;
  pool->current_block = new_block;

  return ptr;
}

/**
 * @brief Free memory allocated from a memory pool
 *
 * Note: This doesn't actually free the memory in most cases,
 * it just marks small blocks as unused for reuse.
 */
void memory_pool_free(memory_pool_t *pool, void *ptr) {
  if (!pool || !ptr) {
    return;
  }

  // Update statistics
  pool->num_frees++;

  // Check if this is a small block
  for (size_t i = 0; i < pool->small_capacity; i++) {
    if (ptr == pool->small_blocks[i].data) {
      pool->small_blocks[i].used = false;
      pool->small_used--;
      return;
    }
  }

  // Not a small block, we can't free it immediately
  // It will be freed when the pool is destroyed or reset
}

/**
 * @brief Thread-specific cleanup function for TLS
 */
static void tls_pool_thread_cleanup(void *data) {
  memory_pool_t *pool = (memory_pool_t *)data;
  if (pool) {
    memory_pool_destroy(pool);
    free(pool);
  }
}

/**
 * @brief Initialize function for pthread_once
 */
static void tls_pool_init_once(void) {
  // Allocate and initialize the global pool
  g_global_pool = (memory_pool_t *)malloc(sizeof(memory_pool_t));
  if (!g_global_pool) {
    fprintf(stderr, "Failed to allocate global memory pool\n");
    return;
  }

  // Initialize the pool with default values
  if (!memory_pool_init(g_global_pool, DEFAULT_BLOCK_SIZE, DEFAULT_MAX_BLOCKS,
                        DEFAULT_SMALL_CAPACITY)) {
    free(g_global_pool);
    g_global_pool = NULL;
    fprintf(stderr, "Failed to initialize global memory pool\n");
    return;
  }

  // Create thread-local storage key
  if (pthread_key_create(&g_global_pool->tls_key, tls_pool_thread_cleanup) !=
      0) {
    memory_pool_destroy(g_global_pool);
    free(g_global_pool);
    g_global_pool = NULL;
    fprintf(stderr, "Failed to create thread-local storage key\n");
    return;
  }
}

/**
 * @brief Initialize the global thread-local memory pool system
 */
bool tls_pool_init(void) {
  pthread_once(&g_init_once, tls_pool_init_once);
  return g_global_pool != NULL;
}

/**
 * @brief Destroy the global thread-local memory pool system
 */
void tls_pool_destroy(void) {
  pthread_mutex_lock(&g_pool_mutex);

  if (g_global_pool) {
    // Don't destroy the TLS key; let the thread cleanup functions handle their
    // pools
    memory_pool_destroy(g_global_pool);
    free(g_global_pool);
    g_global_pool = NULL;
  }

  pthread_mutex_unlock(&g_pool_mutex);
}

/**
 * @brief Get the thread-local memory pool for the current thread
 */
static memory_pool_t *get_thread_pool(void) {
  if (!g_global_pool) {
    // Initialize the global pool if not done yet
    if (!tls_pool_init()) {
      return NULL;
    }
  }

  // Get the thread-local pool from TLS
  memory_pool_t *pool =
      (memory_pool_t *)pthread_getspecific(g_global_pool->tls_key);
  if (!pool) {
    // Create a new pool for this thread
    pool = (memory_pool_t *)malloc(sizeof(memory_pool_t));
    if (!pool) {
      return NULL;
    }

    // Initialize the pool with default values
    if (!memory_pool_init(pool, DEFAULT_BLOCK_SIZE, DEFAULT_MAX_BLOCKS,
                          DEFAULT_SMALL_CAPACITY)) {
      free(pool);
      return NULL;
    }

    // Set the thread-local value
    pthread_setspecific(g_global_pool->tls_key, pool);
  }

  return pool;
}

/**
 * @brief Initialize the thread-local memory pool for the current thread
 */
void tls_pool_init_thread(void) {
  // Just call get_thread_pool to initialize the thread-local pool
  get_thread_pool();
}

/**
 * @brief Clean up the thread-local memory pool for the current thread
 */
void tls_pool_cleanup_thread(void) {
  if (!g_global_pool) {
    return;
  }

  // Get the thread-local pool
  memory_pool_t *pool =
      (memory_pool_t *)pthread_getspecific(g_global_pool->tls_key);
  if (pool) {
    // Cleanup will be done by the TLS cleanup function
    pthread_setspecific(g_global_pool->tls_key, NULL);
  }
}

/**
 * @brief Allocate memory from the thread-local memory pool
 */
void *tls_pool_alloc(size_t size) {
  memory_pool_t *pool = get_thread_pool();
  if (!pool) {
    return malloc(size); // Fall back to standard malloc
  }

  return memory_pool_alloc(pool, size);
}

/**
 * @brief Free memory allocated from the thread-local memory pool
 */
void tls_pool_free(void *ptr) {
  if (!ptr) {
    return;
  }

  memory_pool_t *pool = get_thread_pool();
  if (!pool) {
    free(ptr); // Fall back to standard free
    return;
  }

  memory_pool_free(pool, ptr);
}

/**
 * @brief Get statistics about the thread-local memory pool
 */
void tls_pool_get_stats(size_t *total_allocated, size_t *max_allocated,
                        size_t *num_allocs, size_t *num_frees,
                        size_t *cache_misses) {
  memory_pool_t *pool = get_thread_pool();
  if (!pool) {
    if (total_allocated)
      *total_allocated = 0;
    if (max_allocated)
      *max_allocated = 0;
    if (num_allocs)
      *num_allocs = 0;
    if (num_frees)
      *num_frees = 0;
    if (cache_misses)
      *cache_misses = 0;
    return;
  }

  if (total_allocated)
    *total_allocated = pool->total_allocated;
  if (max_allocated)
    *max_allocated = pool->max_allocated;
  if (num_allocs)
    *num_allocs = pool->num_allocs;
  if (num_frees)
    *num_frees = pool->num_frees;
  if (cache_misses)
    *cache_misses = pool->cache_misses;
}

/**
 * @brief Create a new memory pool
 */
memory_pool_t *memory_pool_create(size_t block_size, size_t max_blocks) {
  memory_pool_t *pool = (memory_pool_t *)malloc(sizeof(memory_pool_t));
  if (!pool) {
    return NULL;
  }

  if (!memory_pool_init(pool, block_size, max_blocks, DEFAULT_SMALL_CAPACITY)) {
    free(pool);
    return NULL;
  }

  return pool;
}

/**
 * @brief Allocate memory from a memory pool (alias for memory_pool_alloc)
 */
void *memory_pool_malloc(memory_pool_t *pool, size_t size) {
  return memory_pool_alloc(pool, size);
}

/**
 * @brief Get detailed statistics about a memory pool
 */
void memory_pool_get_detailed_stats(memory_pool_t *pool,
                                    size_t *total_allocated,
                                    size_t *max_allocated, size_t *num_allocs,
                                    size_t *num_frees, size_t *cache_misses) {
  if (!pool) {
    if (total_allocated)
      *total_allocated = 0;
    if (max_allocated)
      *max_allocated = 0;
    if (num_allocs)
      *num_allocs = 0;
    if (num_frees)
      *num_frees = 0;
    if (cache_misses)
      *cache_misses = 0;
    return;
  }

  if (total_allocated)
    *total_allocated = pool->total_allocated;
  if (max_allocated)
    *max_allocated = pool->max_allocated;
  if (num_allocs)
    *num_allocs = pool->num_allocs;
  if (num_frees)
    *num_frees = pool->num_frees;
  if (cache_misses)
    *cache_misses = pool->cache_misses;
}

#endif // DISABLE_MEMORY_POOL