/**
 * @file thread_pool.h
 * @brief High-performance work-stealing thread pool
 *
 * This file provides a work-stealing thread pool implementation
 * with adaptive scheduling for maximum performance.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declaration for task
struct thread_task;

/**
 * Thread task type definition
 */
typedef void (*thread_task_func_t)(void* arg);

/**
 * Thread task structure
 */
typedef struct thread_task {
    thread_task_func_t func;       // Task function to execute
    void* arg;                      // Argument to pass to the function
    struct thread_task* next;       // Next task in the queue
} thread_task_t;

/**
 * Thread worker structure
 */
typedef struct thread_worker {
    pthread_t thread;               // Worker thread
    struct thread_pool* pool;       // Pool this worker belongs to
    struct thread_task* queue;      // Worker's private task queue
    size_t queue_size;              // Size of worker's queue
    pthread_mutex_t mutex;          // Mutex for worker's queue
    pthread_cond_t cond;            // Condition variable for worker
    size_t tasks_processed;         // Number of tasks processed
    size_t steals;                  // Number of tasks stolen
    size_t id;                      // Worker ID
    bool running;                   // Whether the worker is running
    int cpu_id;                     // CPU ID this worker is bound to
} thread_worker_t;

/**
 * Thread pool structure
 */
typedef struct thread_pool {
    thread_worker_t* workers;       // Array of workers
    size_t num_workers;             // Number of workers
    pthread_mutex_t mutex;          // Mutex for shared queue
    pthread_cond_t cond;            // Condition variable for pool
    struct thread_task* shared_queue; // Shared task queue
    size_t shared_queue_size;       // Size of shared queue
    size_t tasks_queued;            // Total number of tasks queued
    size_t tasks_completed;         // Total number of tasks completed
    bool running;                   // Whether the pool is running
    bool adaptive;                  // Whether to use adaptive scheduling
    bool affinity;                  // Whether to set CPU affinity
} thread_pool_t;

/**
 * @brief Create a thread pool with the specified number of workers
 * 
 * @param num_workers Number of worker threads to create
 * @param adaptive Whether to use adaptive scheduling
 * @param affinity Whether to set CPU affinity
 * @return Pointer to the created thread pool, or NULL on failure
 */
thread_pool_t* thread_pool_create(size_t num_workers, bool adaptive, bool affinity);

/**
 * @brief Destroy a thread pool
 * 
 * @param pool Thread pool to destroy
 */
void thread_pool_destroy(thread_pool_t* pool);

/**
 * @brief Submit a task to a thread pool
 * 
 * @param pool Thread pool to submit the task to
 * @param func Task function to execute
 * @param arg Argument to pass to the function
 * @return true if the task was submitted successfully
 */
bool thread_pool_submit(thread_pool_t* pool, thread_task_func_t func, void* arg);

/**
 * @brief Wait for all tasks in the pool to complete
 * 
 * @param pool Thread pool to wait for
 */
void thread_pool_wait(thread_pool_t* pool);

/**
 * @brief Get the number of tasks queued
 * 
 * @param pool Thread pool to query
 * @return Number of tasks queued
 */
size_t thread_pool_get_tasks_queued(thread_pool_t* pool);

/**
 * @brief Get the number of tasks completed
 * 
 * @param pool Thread pool to query
 * @return Number of tasks completed
 */
size_t thread_pool_get_tasks_completed(thread_pool_t* pool);

/**
 * @brief Get the number of workers in the pool
 * 
 * @param pool Thread pool to query
 * @return Number of workers
 */
size_t thread_pool_get_num_workers(thread_pool_t* pool);

/**
 * @brief Get the number of active workers
 * 
 * @param pool Thread pool to query
 * @return Number of active workers
 */
size_t thread_pool_get_num_active_workers(thread_pool_t* pool);

/**
 * @brief Get the number of tasks processed by each worker
 * 
 * @param pool Thread pool to query
 * @param tasks_per_worker Array to store the number of tasks per worker
 * @param array_size Size of the array
 * @return true if successful
 */
bool thread_pool_get_tasks_per_worker(thread_pool_t* pool, size_t* tasks_per_worker, size_t array_size);

/**
 * @brief Get the number of steals performed by each worker
 * 
 * @param pool Thread pool to query
 * @param steals_per_worker Array to store the number of steals per worker
 * @param array_size Size of the array
 * @return true if successful
 */
bool thread_pool_get_steals_per_worker(thread_pool_t* pool, size_t* steals_per_worker, size_t array_size);

/**
 * @brief Get the CPU ID each worker is bound to
 * 
 * @param pool Thread pool to query
 * @param cpu_ids Array to store the CPU IDs
 * @param array_size Size of the array
 * @return true if successful
 */
bool thread_pool_get_cpu_ids(thread_pool_t* pool, int* cpu_ids, size_t array_size);

#endif // THREAD_POOL_H 