/**
 * @file cache.c
 * @brief Implementation of high-performance cache
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../include/cache.h"

// FNV-1a hash constants
#define FNV_PRIME 1099511628211ULL
#define FNV_OFFSET_BASIS 14695981039346656037ULL

// Default target size after pruning (75% of capacity)
#define DEFAULT_PRUNE_TARGET_RATIO 0.75

// Get current time in microseconds
static uint64_t get_time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// FNV-1a hash function
uint64_t cache_hash(const void *key, size_t key_len) {
  const unsigned char *data = (const unsigned char *)key;
  uint64_t hash = FNV_OFFSET_BASIS;

  for (size_t i = 0; i < key_len; i++) {
    hash ^= data[i];
    hash *= FNV_PRIME;
  }

  return hash;
}

// Create a new cache
cache_t *cache_create(size_t capacity, size_t num_buckets,
                      cache_policy_t policy, time_t prune_interval,
                      void (*cleanup_fn)(void *)) {
  // Validate parameters
  if (capacity == 0 || num_buckets == 0) {
    return NULL;
  }

  // Allocate memory for the cache
  cache_t *cache = (cache_t *)malloc(sizeof(cache_t));
  if (!cache) {
    return NULL;
  }

  // Initialize the cache
  memset(cache, 0, sizeof(cache_t));
  cache->capacity = capacity;
  cache->num_buckets = num_buckets;
  cache->policy = policy;
  cache->prune_interval = prune_interval;
  cache->last_prune = time(NULL);
  cache->cleanup_fn = cleanup_fn;

  // Allocate memory for the buckets
  cache->buckets =
      (cache_entry_t **)calloc(num_buckets, sizeof(cache_entry_t *));
  if (!cache->buckets) {
    free(cache);
    return NULL;
  }

  return cache;
}

// Destroy a cache
void cache_destroy(cache_t *cache) {
  if (!cache) {
    return;
  }

  // Clear the cache first
  cache_clear(cache);

  // Free the buckets array
  free(cache->buckets);

  // Free the cache
  free(cache);
}

// Get a value from the cache
void *cache_get(cache_t *cache, const void *key, size_t key_len,
                size_t *value_size) {
  if (!cache || !key || key_len == 0) {
    return NULL;
  }

  // Start timing
  uint64_t start_time = get_time_us();

  // Hash the key
  uint64_t hash = cache_hash(key, key_len);

  // Find the bucket
  size_t bucket_index = hash % cache->num_buckets;
  cache_entry_t *entry = cache->buckets[bucket_index];

  // Look for the entry
  while (entry) {
    if (entry->key == hash) {
      // Found the entry
      entry->timestamp = time(NULL);
      entry->access_count++;

      // End timing
      uint64_t end_time = get_time_us();
      cache->total_lookup_time += (end_time - start_time);
      cache->num_lookups++;

      // Update statistics
      cache->hits++;

      // Return the value
      if (value_size) {
        *value_size = entry->value_size;
      }
      return entry->value;
    }
    entry = entry->next;
  }

  // End timing
  uint64_t end_time = get_time_us();
  cache->total_lookup_time += (end_time - start_time);
  cache->num_lookups++;

  // Entry not found
  cache->misses++;
  return NULL;
}

// Remove the least valuable entry according to the policy
static size_t cache_prune_one(cache_t *cache) {
  if (!cache || cache->num_entries == 0) {
    return 0;
  }

  cache_entry_t *victim = NULL;
  cache_entry_t **victim_prev = NULL;
  size_t victim_bucket __attribute__((unused)) = 0;

  // Variables for policy decisions
  time_t oldest_time = time(NULL);
  time_t newest_time = 0;
  uint32_t lowest_count = UINT32_MAX;

  // Find the victim based on the policy
  for (size_t i = 0; i < cache->num_buckets; i++) {
    cache_entry_t **prev = &cache->buckets[i];
    cache_entry_t *entry = *prev;

    while (entry) {
      bool is_victim = false;

      switch (cache->policy) {
      case CACHE_POLICY_LRU:
        // Least Recently Used
        if (entry->timestamp < oldest_time) {
          oldest_time = entry->timestamp;
          is_victim = true;
        }
        break;

      case CACHE_POLICY_LFU:
        // Least Frequently Used
        if (entry->access_count < lowest_count) {
          lowest_count = entry->access_count;
          is_victim = true;
        }
        break;

      case CACHE_POLICY_MRU:
        // Most Recently Used
        if (entry->timestamp > newest_time) {
          newest_time = entry->timestamp;
          is_victim = true;
        }
        break;

      case CACHE_POLICY_FIFO:
        // First In First Out - we just compare timestamps
        if (entry->timestamp < oldest_time) {
          oldest_time = entry->timestamp;
          is_victim = true;
        }
        break;

      case CACHE_POLICY_RANDOM:
        // Random - we replace the victim with 1/n probability
        if (rand() % cache->num_entries == 0) {
          is_victim = true;
        }
        break;
      }

      if (is_victim) {
        victim = entry;
        victim_prev = prev;
        victim_bucket = i;
      }

      prev = &entry->next;
      entry = entry->next;
    }
  }

  // If we found a victim, remove it
  if (victim) {
    // Remove it from the bucket
    *victim_prev = victim->next;

    // Update cache statistics
    cache->size -= victim->value_size;
    cache->num_entries--;
    cache->evictions++;

    // Clean up the value if needed
    if (cache->cleanup_fn) {
      cache->cleanup_fn(victim->value);
    } else {
      free(victim->value);
    }

    // Free the entry
    free(victim);

    return 1;
  }

  return 0;
}

// Put a value in the cache
bool cache_put(cache_t *cache, const void *key, size_t key_len,
               const void *value, size_t value_size) {
  if (!cache || !key || key_len == 0 || !value || value_size == 0) {
    return false;
  }

  // Start timing
  uint64_t start_time = get_time_us();

  // Check if we need to prune
  time_t now = time(NULL);
  if (cache->prune_interval > 0 &&
      now - cache->last_prune >= cache->prune_interval) {
    cache_prune(cache, 0);
    cache->last_prune = now;
  }

  // Check if we need to make space
  if (cache->size + value_size > cache->capacity) {
    // Try to make space by pruning
    size_t target_size = cache->capacity - value_size;
    cache_prune(cache, target_size); // Ignore return value

    // If we couldn't make enough space, fail
    if (cache->size + value_size > cache->capacity) {
      uint64_t end_time = get_time_us();
      cache->total_insert_time += (end_time - start_time);
      cache->num_inserts++;
      return false;
    }
  }

  // Hash the key
  uint64_t hash = cache_hash(key, key_len);

  // Find the bucket
  size_t bucket_index = hash % cache->num_buckets;
  cache_entry_t *entry = cache->buckets[bucket_index];
  cache_entry_t *prev = NULL;

  // Check if the key already exists
  while (entry) {
    if (entry->key == hash) {
      // Key exists, update the value
      void *new_value = malloc(value_size);
      if (!new_value) {
        uint64_t end_time = get_time_us();
        cache->total_insert_time += (end_time - start_time);
        cache->num_inserts++;
        return false;
      }

      memcpy(new_value, value, value_size);

      // Clean up the old value if needed
      if (cache->cleanup_fn) {
        cache->cleanup_fn(entry->value);
      } else {
        free(entry->value);
      }

      // Update the entry
      entry->value = new_value;
      entry->value_size = value_size;
      entry->timestamp = time(NULL);
      entry->access_count++;
      entry->is_dirty = true;

      // Update statistics
      cache->size = cache->size - entry->value_size + value_size;
      cache->overwrites++;

      // End timing
      uint64_t end_time = get_time_us();
      cache->total_insert_time += (end_time - start_time);
      cache->num_inserts++;

      return true;
    }

    prev = entry;
    entry = entry->next;
  }

  // Key doesn't exist, create a new entry
  cache_entry_t *new_entry = (cache_entry_t *)malloc(sizeof(cache_entry_t));
  if (!new_entry) {
    uint64_t end_time = get_time_us();
    cache->total_insert_time += (end_time - start_time);
    cache->num_inserts++;
    return false;
  }

  // Allocate memory for the value
  void *new_value = malloc(value_size);
  if (!new_value) {
    free(new_entry);
    uint64_t end_time = get_time_us();
    cache->total_insert_time += (end_time - start_time);
    cache->num_inserts++;
    return false;
  }

  memcpy(new_value, value, value_size);

  // Initialize the entry
  new_entry->key = hash;
  new_entry->value = new_value;
  new_entry->value_size = value_size;
  new_entry->timestamp = time(NULL);
  new_entry->next = NULL;
  new_entry->access_count = 1;
  new_entry->is_dirty = true;

  // Add the entry to the bucket
  if (prev) {
    prev->next = new_entry;
    cache->collisions++;
  } else {
    cache->buckets[bucket_index] = new_entry;
  }

  // Update statistics
  cache->size += value_size;
  cache->num_entries++;

  // End timing
  uint64_t end_time = get_time_us();
  cache->total_insert_time += (end_time - start_time);
  cache->num_inserts++;

  return true;
}

// Remove a value from the cache
bool cache_remove(cache_t *cache, const void *key, size_t key_len) {
  if (!cache || !key || key_len == 0) {
    return false;
  }

  // Hash the key
  uint64_t hash = cache_hash(key, key_len);

  // Find the bucket
  size_t bucket_index = hash % cache->num_buckets;
  cache_entry_t *entry = cache->buckets[bucket_index];
  cache_entry_t *prev = NULL;

  // Look for the entry
  while (entry) {
    if (entry->key == hash) {
      // Found the entry
      if (prev) {
        prev->next = entry->next;
      } else {
        cache->buckets[bucket_index] = entry->next;
      }

      // Update statistics
      cache->size -= entry->value_size;
      cache->num_entries--;

      // Clean up the value if needed
      if (cache->cleanup_fn) {
        cache->cleanup_fn(entry->value);
      } else {
        free(entry->value);
      }

      // Free the entry
      free(entry);

      return true;
    }

    prev = entry;
    entry = entry->next;
  }

  return false;
}

// Clear the cache
void cache_clear(cache_t *cache) {
  if (!cache) {
    return;
  }

  // Free all entries
  for (size_t i = 0; i < cache->num_buckets; i++) {
    cache_entry_t *entry = cache->buckets[i];

    while (entry) {
      cache_entry_t *next = entry->next;

      // Clean up the value if needed
      if (cache->cleanup_fn) {
        cache->cleanup_fn(entry->value);
      } else {
        free(entry->value);
      }

      // Free the entry
      free(entry);

      entry = next;
    }

    cache->buckets[i] = NULL;
  }

  // Reset statistics
  cache->size = 0;
  cache->num_entries = 0;
}

// Prune the cache to free up space
size_t cache_prune(cache_t *cache, size_t target_size) {
  if (!cache) {
    return 0;
  }

  // If target_size is 0, use the default
  if (target_size == 0) {
    target_size = (size_t)(cache->capacity * DEFAULT_PRUNE_TARGET_RATIO);
  }

  // Don't prune if we're already below the target
  if (cache->size <= target_size) {
    return 0;
  }

  // Prune until we're below the target
  size_t pruned = 0;
  while (cache->size > target_size && cache->num_entries > 0) {
    pruned += cache_prune_one(cache);
  }

  return pruned;
}

// Get cache statistics
void cache_get_stats(cache_t *cache, cache_stats_t *stats) {
  if (!cache || !stats) {
    return;
  }

  // Copy statistics
  stats->size = cache->size;
  stats->capacity = cache->capacity;
  stats->num_entries = cache->num_entries;
  stats->hits = cache->hits;
  stats->misses = cache->misses;
  stats->evictions = cache->evictions;
  stats->collisions = cache->collisions;
  stats->overwrites = cache->overwrites;

  // Calculate hit rate
  size_t total_lookups = cache->hits + cache->misses;
  stats->hit_rate =
      total_lookups > 0 ? (double)cache->hits / total_lookups : 0.0;

  // Calculate average lookup time
  stats->avg_lookup_time = cache->num_lookups > 0
                               ? cache->total_lookup_time / cache->num_lookups
                               : 0.0;

  // Calculate average insert time
  stats->avg_insert_time = cache->num_inserts > 0
                               ? cache->total_insert_time / cache->num_inserts
                               : 0.0;
}

// Resize the cache
bool cache_resize(cache_t *cache, size_t new_capacity, size_t new_num_buckets) {
  if (!cache || new_capacity == 0) {
    return false;
  }

  // If we're not changing the number of buckets, just update the capacity
  if (new_num_buckets == 0 || new_num_buckets == cache->num_buckets) {
    cache->capacity = new_capacity;

    // Prune if needed
    if (cache->size > new_capacity) {
      cache_prune(cache, new_capacity);
    }

    return true;
  }

  // Create a new array of buckets
  cache_entry_t **new_buckets =
      (cache_entry_t **)calloc(new_num_buckets, sizeof(cache_entry_t *));
  if (!new_buckets) {
    return false;
  }

  // Rehash all entries
  for (size_t i = 0; i < cache->num_buckets; i++) {
    cache_entry_t *entry = cache->buckets[i];

    while (entry) {
      cache_entry_t *next = entry->next;

      // Calculate the new bucket index
      size_t new_bucket_index = entry->key % new_num_buckets;

      // Insert the entry into the new bucket
      entry->next = new_buckets[new_bucket_index];
      new_buckets[new_bucket_index] = entry;

      entry = next;
    }
  }

  // Free the old buckets array
  free(cache->buckets);

  // Update the cache
  cache->buckets = new_buckets;
  cache->num_buckets = new_num_buckets;
  cache->capacity = new_capacity;

  // Prune if needed
  if (cache->size > new_capacity) {
    cache_prune(cache, new_capacity);
  }

  return true;
}

// Iterate over all entries in the cache
void cache_for_each(cache_t *cache,
                    void (*callback)(const void *key, size_t key_len,
                                     void *value, size_t value_size,
                                     void *user_data),
                    void *user_data) {
  if (!cache || !callback) {
    return;
  }

  for (size_t i = 0; i < cache->num_buckets; i++) {
    cache_entry_t *entry = cache->buckets[i];

    while (entry) {
      // Call the callback with a dummy key since we only have the hash
      uint64_t key_hash = entry->key;
      callback(&key_hash, sizeof(key_hash), entry->value, entry->value_size,
               user_data);

      entry = entry->next;
    }
  }
}
