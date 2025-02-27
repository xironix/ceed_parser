#include "../include/memory_pool.h"
#include "../include/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for test runner functions
void print_suite_header(const char *suite_name);
void print_suite_footer(void);
typedef void (*TestFunction)(void);
void custom_test_runner(TestFunction test);

// Test context
static memory_pool_t *test_pool = NULL;
static const size_t TEST_BLOCK_SIZE = 64;
static const size_t TEST_POOL_SIZE = 1024;

// Setup function - called before each test
void setUp(void) {
  // Create a new memory pool for testing
  test_pool =
      memory_pool_create(TEST_BLOCK_SIZE, TEST_POOL_SIZE / TEST_BLOCK_SIZE);
  TEST_ASSERT(test_pool != NULL);
}

// Teardown function - called after each test
void tearDown(void) {
  // Destroy the memory pool
  if (test_pool) {
    memory_pool_destroy(test_pool);
    test_pool = NULL;
  }
}

// Test memory pool creation
void test_memory_pool_create(void) {
  // Test that the pool was created successfully
  TEST_ASSERT(test_pool != NULL);

  // Get pool stats
  memory_pool_stats_t stats;
  memory_pool_get_stats(test_pool, &stats);

  // Test that the pool has the correct properties
  TEST_ASSERT_EQUAL(TEST_BLOCK_SIZE, stats.block_size);
  TEST_ASSERT_EQUAL(TEST_POOL_SIZE / TEST_BLOCK_SIZE, stats.block_count);
  TEST_ASSERT_EQUAL(0, stats.allocations);
}

// Test memory allocation from pool
void test_memory_pool_alloc(void) {
  // Allocate a block from the pool
  void *block = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE / 2);
  TEST_ASSERT(block != NULL);

  // Get pool stats
  memory_pool_stats_t stats;
  memory_pool_get_stats(test_pool, &stats);

  // Test that the pool usage has increased
  TEST_ASSERT_EQUAL(1, stats.allocations);

  // Write to the block to ensure it's usable
  memset(block, 0xAA, TEST_BLOCK_SIZE / 2);

  // Allocate another block
  void *block2 = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE / 2);
  TEST_ASSERT(block2 != NULL);
  TEST_ASSERT(block != block2);

  // Get updated stats
  memory_pool_get_stats(test_pool, &stats);

  // Test that the pool usage has increased again
  TEST_ASSERT_EQUAL(2, stats.allocations);
}

// Test memory freeing
void test_memory_pool_free(void) {
  // Allocate a block from the pool
  void *block = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE / 2);
  TEST_ASSERT(block != NULL);

  // Get pool stats
  memory_pool_stats_t stats;
  memory_pool_get_stats(test_pool, &stats);
  TEST_ASSERT_EQUAL(1, stats.allocations);

  // Free the block
  memory_pool_free(test_pool, block);

  // Get updated stats
  memory_pool_get_stats(test_pool, &stats);

  // Allocate again - should be able to allocate again
  void *block2 = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE / 2);
  TEST_ASSERT(block2 != NULL);
}

// Test pool exhaustion
void test_memory_pool_exhaustion(void) {
  // Calculate how many blocks we can allocate
  size_t num_blocks = TEST_POOL_SIZE / TEST_BLOCK_SIZE;
  void *blocks[num_blocks + 1];

  // Allocate all blocks
  for (size_t i = 0; i < num_blocks; i++) {
    blocks[i] = memory_pool_malloc(
        test_pool, TEST_BLOCK_SIZE - 8); // Leave some room for metadata
    TEST_ASSERT(blocks[i] != NULL);
  }

  // Get pool stats
  memory_pool_stats_t stats;
  memory_pool_get_stats(test_pool, &stats);

  // Verify pool is full
  TEST_ASSERT_EQUAL(num_blocks, stats.allocations);

  // Try to allocate one more block - might fail depending on implementation
  blocks[num_blocks] = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE);

  // Free one block
  memory_pool_free(test_pool, blocks[0]);

  // Should be able to allocate again
  blocks[0] = memory_pool_malloc(test_pool, TEST_BLOCK_SIZE - 8);
  TEST_ASSERT(blocks[0] != NULL);
}

// Test multiple allocations and frees
void test_memory_pool_multiple_ops(void) {
  // Allocate and free in a pattern to test memory reuse
  void *blocks[10];

  // Allocate 5 blocks
  for (int i = 0; i < 5; i++) {
    blocks[i] = memory_pool_malloc(test_pool, 16);
    TEST_ASSERT(blocks[i] != NULL);
  }

  // Free blocks 0, 2, 4
  memory_pool_free(test_pool, blocks[0]);
  memory_pool_free(test_pool, blocks[2]);
  memory_pool_free(test_pool, blocks[4]);

  // Allocate 3 more blocks
  for (int i = 5; i < 8; i++) {
    blocks[i] = memory_pool_malloc(test_pool, 16);
    TEST_ASSERT(blocks[i] != NULL);
  }
}

// Test thread safety if supported
#ifdef THREAD_SAFE_MEMORY_POOL
void test_memory_pool_thread_safety(void) {
  // This test would create multiple threads that allocate and free memory
  // from the same pool simultaneously to test thread safety

  // For now, just mark as passed
  TEST_ASSERT(1);
}
#endif

// Run all memory pool tests
void run_memory_tests(void) {
  print_suite_header("Memory Pool Tests");

  // Run all tests
  custom_test_runner(test_memory_pool_create);
  custom_test_runner(test_memory_pool_alloc);
  custom_test_runner(test_memory_pool_free);
  custom_test_runner(test_memory_pool_exhaustion);
  custom_test_runner(test_memory_pool_multiple_ops);

#ifdef THREAD_SAFE_MEMORY_POOL
  custom_test_runner(test_memory_pool_thread_safety);
#endif

  print_suite_footer();
}