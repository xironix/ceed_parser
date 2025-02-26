/**
 * @file cache.h
 * @brief High-performance cache for optimizing memory access
 *
 * This file provides a lock-free cache implementation
 * with low-latency access and automatic pruning.
 */

#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/**
 * Cache entry structure
 */
typedef struct cache_entry {
    uint64_t key;                  // Hash of the key
    void* value;                   // Cached value
    size_t value_size;             // Size of the value
    time_t timestamp;              // Last access time
    struct cache_entry* next;      // Next entry in the bucket
    uint32_t access_count;         // Number of times this entry was accessed
    bool is_dirty;                 // Whether this entry has been modified
} cache_entry_t;

/**
 * Cache statistics structure
 */
typedef struct cache_stats {
    size_t size;                   // Current cache size in bytes
    size_t capacity;               // Maximum cache size in bytes
    size_t num_entries;            // Number of entries in the cache
    size_t hits;                   // Number of cache hits
    size_t misses;                 // Number of cache misses
    size_t evictions;              // Number of entries evicted
    size_t collisions;             // Number of hash collisions
    size_t overwrites;             // Number of entries overwritten
    double hit_rate;               // Cache hit rate
    double avg_lookup_time;        // Average lookup time in microseconds
    double avg_insert_time;        // Average insert time in microseconds
} cache_stats_t;

/**
 * Cache pruning policy
 */
typedef enum cache_policy {
    CACHE_POLICY_LRU,              // Least Recently Used
    CACHE_POLICY_LFU,              // Least Frequently Used
    CACHE_POLICY_MRU,              // Most Recently Used
    CACHE_POLICY_FIFO,             // First In First Out
    CACHE_POLICY_RANDOM            // Random Eviction
} cache_policy_t;

/**
 * Cache structure
 */
typedef struct cache {
    cache_entry_t** buckets;       // Hash table buckets
    size_t num_buckets;            // Number of buckets
    size_t size;                   // Current cache size in bytes
    size_t capacity;               // Maximum cache size in bytes
    size_t num_entries;            // Number of entries in the cache
    size_t hits;                   // Number of cache hits
    size_t misses;                 // Number of cache misses
    size_t evictions;              // Number of entries evicted
    size_t collisions;             // Number of hash collisions
    size_t overwrites;             // Number of entries overwritten
    double total_lookup_time;      // Total lookup time in microseconds
    double total_insert_time;      // Total insert time in microseconds
    size_t num_lookups;            // Number of lookups
    size_t num_inserts;            // Number of inserts
    cache_policy_t policy;         // Pruning policy
    time_t prune_interval;         // Time between automatic pruning
    time_t last_prune;             // Last time the cache was pruned
    void (*cleanup_fn)(void*);     // Function to clean up values
} cache_t;

/**
 * @brief Create a new cache
 * 
 * @param capacity Maximum capacity of the cache in bytes
 * @param num_buckets Number of buckets in the hash table
 * @param policy Pruning policy
 * @param prune_interval Time between automatic pruning in seconds
 * @param cleanup_fn Function to clean up values, or NULL
 * @return Pointer to the created cache, or NULL on failure
 */
cache_t* cache_create(size_t capacity, size_t num_buckets, cache_policy_t policy, 
                      time_t prune_interval, void (*cleanup_fn)(void*));

/**
 * @brief Destroy a cache
 * 
 * @param cache Cache to destroy
 */
void cache_destroy(cache_t* cache);

/**
 * @brief Get a value from the cache
 * 
 * @param cache Cache to get the value from
 * @param key Key to look up
 * @param key_len Length of the key
 * @param value_size Pointer to store the size of the value, or NULL
 * @return Pointer to the value, or NULL if not found
 */
void* cache_get(cache_t* cache, const void* key, size_t key_len, size_t* value_size);

/**
 * @brief Put a value in the cache
 * 
 * @param cache Cache to put the value in
 * @param key Key to store the value under
 * @param key_len Length of the key
 * @param value Value to store
 * @param value_size Size of the value
 * @return true if the value was stored successfully
 */
bool cache_put(cache_t* cache, const void* key, size_t key_len, const void* value, size_t value_size);

/**
 * @brief Remove a value from the cache
 * 
 * @param cache Cache to remove the value from
 * @param key Key to remove
 * @param key_len Length of the key
 * @return true if the value was removed successfully
 */
bool cache_remove(cache_t* cache, const void* key, size_t key_len);

/**
 * @brief Clear the cache
 * 
 * @param cache Cache to clear
 */
void cache_clear(cache_t* cache);

/**
 * @brief Prune the cache to free up space
 * 
 * @param cache Cache to prune
 * @param target_size Target size in bytes after pruning, or 0 for default
 * @return Number of entries pruned
 */
size_t cache_prune(cache_t* cache, size_t target_size);

/**
 * @brief Get cache statistics
 * 
 * @param cache Cache to get statistics for
 * @param stats Pointer to a cache_stats_t structure to fill
 */
void cache_get_stats(cache_t* cache, cache_stats_t* stats);

/**
 * @brief Resize the cache
 * 
 * @param cache Cache to resize
 * @param new_capacity New capacity in bytes
 * @param new_num_buckets New number of buckets, or 0 to keep the current number
 * @return true if the cache was resized successfully
 */
bool cache_resize(cache_t* cache, size_t new_capacity, size_t new_num_buckets);

/**
 * @brief Iterate over all entries in the cache
 * 
 * @param cache Cache to iterate over
 * @param callback Function to call for each entry
 * @param user_data User data to pass to the callback
 */
void cache_for_each(cache_t* cache, void (*callback)(const void* key, size_t key_len, 
                    void* value, size_t value_size, void* user_data), void* user_data);

/**
 * @brief Hash a key
 * 
 * @param key Key to hash
 * @param key_len Length of the key
 * @return Hash of the key
 */
uint64_t cache_hash(const void* key, size_t key_len);

#endif // CACHE_H 
