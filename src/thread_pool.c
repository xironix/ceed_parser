/**
 * @file thread_pool.c
 * @brief Implementation of high-performance work-stealing thread pool
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>

#ifdef __APPLE__
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#elif defined(__linux__)
#include <sched.h>
#include <pthread.h>
#endif

#include "../include/thread_pool.h"

// Default number of attempts to steal work
#define STEAL_ATTEMPTS 3

// Random number generator for work stealing
static inline unsigned int fast_rand(void) {
    static unsigned int g_seed = 0;
    if (g_seed == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        g_seed = (unsigned int)(ts.tv_nsec);
    }
    g_seed = (214013 * g_seed + 2531011);
    return (g_seed >> 16) & 0x7FFF;
}

// Get the number of CPU cores available on the system
static size_t get_num_cores(void) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores <= 0) {
        return 1; // Default to 1 if we can't determine
    }
    return (size_t)num_cores;
}

// Set CPU affinity for a thread
static bool set_thread_affinity(pthread_t thread, int cpu_id) {
#ifdef __APPLE__
    // macOS doesn't support standard affinity APIs, using thread policy instead
    thread_port_t mach_thread = pthread_mach_thread_np(thread);
    thread_affinity_policy_data_t policy;
    policy.affinity_tag = cpu_id + 1; // Tags start from 1
    kern_return_t ret = thread_policy_set(mach_thread, 
                                         THREAD_AFFINITY_POLICY,
                                         (thread_policy_t)&policy,
                                         THREAD_AFFINITY_POLICY_COUNT);
    return (ret == KERN_SUCCESS);
#elif defined(__linux__)
    // Linux standard CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    return (ret == 0);
#else
    // Unsupported platform
    (void)thread;
    (void)cpu_id;
    return false;
#endif
}

// Push a task to the front of a worker's queue
static bool worker_push_task(thread_worker_t* worker, thread_task_t* task) {
    pthread_mutex_lock(&worker->mutex);
    task->next = worker->queue;
    worker->queue = task;
    worker->queue_size++;
    pthread_mutex_unlock(&worker->mutex);
    pthread_cond_signal(&worker->cond);
    return true;
}

// Pop a task from the front of a worker's queue
static thread_task_t* worker_pop_task(thread_worker_t* worker) {
    pthread_mutex_lock(&worker->mutex);
    thread_task_t* task = worker->queue;
    if (task) {
        worker->queue = task->next;
        worker->queue_size--;
    }
    pthread_mutex_unlock(&worker->mutex);
    return task;
}

// Steal a task from the back of another worker's queue
static thread_task_t* worker_steal_task(thread_worker_t* victim) {
    pthread_mutex_lock(&victim->mutex);
    
    // Empty queue, nothing to steal
    if (!victim->queue) {
        pthread_mutex_unlock(&victim->mutex);
        return NULL;
    }
    
    // Only one task, take it
    if (!victim->queue->next) {
        thread_task_t* task = victim->queue;
        victim->queue = NULL;
        victim->queue_size--;
        pthread_mutex_unlock(&victim->mutex);
        return task;
    }
    
    // Find the second-to-last task
    thread_task_t* prev = NULL;
    thread_task_t* curr = victim->queue;
    while (curr->next) {
        prev = curr;
        curr = curr->next;
    }
    
    // Remove the last task
    prev->next = NULL;
    victim->queue_size--;
    
    pthread_mutex_unlock(&victim->mutex);
    return curr;
}

// Add a task to the shared queue
static bool pool_add_task(thread_pool_t* pool, thread_task_t* task) {
    pthread_mutex_lock(&pool->mutex);
    task->next = pool->shared_queue;
    pool->shared_queue = task;
    pool->shared_queue_size++;
    pool->tasks_queued++;
    pthread_mutex_unlock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond);
    return true;
}

// Get a task from the shared queue
static thread_task_t* pool_get_task(thread_pool_t* pool) {
    pthread_mutex_lock(&pool->mutex);
    thread_task_t* task = pool->shared_queue;
    if (task) {
        pool->shared_queue = task->next;
        pool->shared_queue_size--;
    }
    pthread_mutex_unlock(&pool->mutex);
    return task;
}

// Worker thread function
static void* worker_function(void* arg) {
    thread_worker_t* worker = (thread_worker_t*)arg;
    thread_pool_t* pool = worker->pool;
    
    // Set CPU affinity if enabled
    if (pool->affinity) {
        worker->cpu_id = (int)(worker->id % get_num_cores());
        set_thread_affinity(worker->thread, worker->cpu_id);
    } else {
        worker->cpu_id = -1;
    }
    
    // Block signals in this thread that should be handled by main thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    
    // Worker loop
    while (worker->running) {
        thread_task_t* task = NULL;
        
        // Try to get a task from our own queue
        if ((task = worker_pop_task(worker)) != NULL) {
            // Execute the task
            task->func(task->arg);
            free(task);
            worker->tasks_processed++;
            
            // Update pool stats
            pthread_mutex_lock(&pool->mutex);
            pool->tasks_completed++;
            pthread_mutex_unlock(&pool->mutex);
            continue;
        }
        
        // Try to get a task from the shared queue
        if ((task = pool_get_task(pool)) != NULL) {
            // Execute the task
            task->func(task->arg);
            free(task);
            worker->tasks_processed++;
            
            // Update pool stats
            pthread_mutex_lock(&pool->mutex);
            pool->tasks_completed++;
            pthread_mutex_unlock(&pool->mutex);
            continue;
        }
        
        // Try to steal tasks from other workers
        bool stole = false;
        for (int i = 0; i < STEAL_ATTEMPTS && worker->running; i++) {
            // Pick a random worker to steal from
            size_t victim_id = fast_rand() % pool->num_workers;
            if (victim_id == worker->id) {
                continue; // Don't steal from ourselves
            }
            
            thread_worker_t* victim = &pool->workers[victim_id];
            if ((task = worker_steal_task(victim)) != NULL) {
                // Execute the stolen task
                task->func(task->arg);
                free(task);
                worker->tasks_processed++;
                worker->steals++;
                
                // Update pool stats
                pthread_mutex_lock(&pool->mutex);
                pool->tasks_completed++;
                pthread_mutex_unlock(&pool->mutex);
                
                stole = true;
                break;
            }
        }
        
        // If we couldn't find any work, wait for a signal
        if (!stole) {
            pthread_mutex_lock(&worker->mutex);
            if (worker->queue == NULL && worker->running) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += 10 * 1000 * 1000; // 10ms timeout
                if (ts.tv_nsec >= 1000000000) {
                    ts.tv_sec++;
                    ts.tv_nsec -= 1000000000;
                }
                pthread_cond_timedwait(&worker->cond, &worker->mutex, &ts);
            }
            pthread_mutex_unlock(&worker->mutex);
        }
    }
    
    return NULL;
}

// Create a thread pool
thread_pool_t* thread_pool_create(size_t num_workers, bool adaptive, bool affinity) {
    thread_pool_t* pool = (thread_pool_t*)malloc(sizeof(thread_pool_t));
    if (!pool) {
        return NULL;
    }
    
    // Initialize the pool
    memset(pool, 0, sizeof(thread_pool_t));
    pool->running = true;
    pool->adaptive = adaptive;
    pool->affinity = affinity;
    
    // Determine the number of workers
    if (num_workers == 0) {
        pool->num_workers = get_num_cores();
    } else {
        pool->num_workers = num_workers;
    }
    
    // Initialize the mutex and condition variable
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        free(pool);
        return NULL;
    }
    
    if (pthread_cond_init(&pool->cond, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        free(pool);
        return NULL;
    }
    
    // Allocate memory for workers
    pool->workers = (thread_worker_t*)malloc(pool->num_workers * sizeof(thread_worker_t));
    if (!pool->workers) {
        pthread_mutex_destroy(&pool->mutex);
        pthread_cond_destroy(&pool->cond);
        free(pool);
        return NULL;
    }
    
    // Initialize workers
    for (size_t i = 0; i < pool->num_workers; i++) {
        thread_worker_t* worker = &pool->workers[i];
        memset(worker, 0, sizeof(thread_worker_t));
        worker->pool = pool;
        worker->id = i;
        worker->running = true;
        
        if (pthread_mutex_init(&worker->mutex, NULL) != 0) {
            for (size_t j = 0; j < i; j++) {
                pthread_mutex_destroy(&pool->workers[j].mutex);
                pthread_cond_destroy(&pool->workers[j].cond);
                pthread_join(pool->workers[j].thread, NULL);
            }
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            free(pool->workers);
            free(pool);
            return NULL;
        }
        
        if (pthread_cond_init(&worker->cond, NULL) != 0) {
            pthread_mutex_destroy(&worker->mutex);
            for (size_t j = 0; j < i; j++) {
                pthread_mutex_destroy(&pool->workers[j].mutex);
                pthread_cond_destroy(&pool->workers[j].cond);
                pthread_join(pool->workers[j].thread, NULL);
            }
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            free(pool->workers);
            free(pool);
            return NULL;
        }
        
        if (pthread_create(&worker->thread, NULL, worker_function, worker) != 0) {
            pthread_mutex_destroy(&worker->mutex);
            pthread_cond_destroy(&worker->cond);
            for (size_t j = 0; j < i; j++) {
                pthread_mutex_destroy(&pool->workers[j].mutex);
                pthread_cond_destroy(&pool->workers[j].cond);
                pthread_join(pool->workers[j].thread, NULL);
            }
            pthread_mutex_destroy(&pool->mutex);
            pthread_cond_destroy(&pool->cond);
            free(pool->workers);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Destroy a thread pool
void thread_pool_destroy(thread_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    // Stop all workers
    for (size_t i = 0; i < pool->num_workers; i++) {
        thread_worker_t* worker = &pool->workers[i];
        pthread_mutex_lock(&worker->mutex);
        worker->running = false;
        pthread_mutex_unlock(&worker->mutex);
        pthread_cond_signal(&worker->cond);
    }
    
    // Wait for all workers to finish
    for (size_t i = 0; i < pool->num_workers; i++) {
        pthread_join(pool->workers[i].thread, NULL);
    }
    
    // Clean up remaining tasks in the shared queue
    thread_task_t* task = pool->shared_queue;
    while (task) {
        thread_task_t* next = task->next;
        free(task);
        task = next;
    }
    
    // Clean up remaining tasks in worker queues
    for (size_t i = 0; i < pool->num_workers; i++) {
        thread_worker_t* worker = &pool->workers[i];
        
        // Clean up tasks
        task = worker->queue;
        while (task) {
            thread_task_t* next = task->next;
            free(task);
            task = next;
        }
        
        // Destroy mutex and condition variable
        pthread_mutex_destroy(&worker->mutex);
        pthread_cond_destroy(&worker->cond);
    }
    
    // Destroy pool mutex and condition variable
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    
    // Free memory
    free(pool->workers);
    free(pool);
}

// Submit a task to the thread pool
bool thread_pool_submit(thread_pool_t* pool, thread_task_func_t func, void* arg) {
    if (!pool || !func) {
        return false;
    }
    
    // Create a new task
    thread_task_t* task = (thread_task_t*)malloc(sizeof(thread_task_t));
    if (!task) {
        return false;
    }
    
    task->func = func;
    task->arg = arg;
    task->next = NULL;
    
    // If using adaptive scheduling
    if (pool->adaptive) {
        // Select a random worker
        size_t worker_id = fast_rand() % pool->num_workers;
        thread_worker_t* worker = &pool->workers[worker_id];
        
        // Add the task to the worker's queue
        return worker_push_task(worker, task);
    } else {
        // Add the task to the shared queue
        return pool_add_task(pool, task);
    }
}

// Wait for all tasks in the pool to complete
void thread_pool_wait(thread_pool_t* pool) {
    if (!pool) {
        return;
    }
    
    // Wait for all tasks to complete
    pthread_mutex_lock(&pool->mutex);
    while (pool->tasks_completed < pool->tasks_queued) {
        pthread_cond_wait(&pool->cond, &pool->mutex);
    }
    pthread_mutex_unlock(&pool->mutex);
}

// Get the number of tasks queued
size_t thread_pool_get_tasks_queued(thread_pool_t* pool) {
    if (!pool) {
        return 0;
    }
    
    pthread_mutex_lock(&pool->mutex);
    size_t tasks_queued = pool->tasks_queued;
    pthread_mutex_unlock(&pool->mutex);
    
    return tasks_queued;
}

// Get the number of tasks completed
size_t thread_pool_get_tasks_completed(thread_pool_t* pool) {
    if (!pool) {
        return 0;
    }
    
    pthread_mutex_lock(&pool->mutex);
    size_t tasks_completed = pool->tasks_completed;
    pthread_mutex_unlock(&pool->mutex);
    
    return tasks_completed;
}

// Get the number of workers in the pool
size_t thread_pool_get_num_workers(thread_pool_t* pool) {
    if (!pool) {
        return 0;
    }
    
    return pool->num_workers;
}

// Get the number of active workers
size_t thread_pool_get_num_active_workers(thread_pool_t* pool) {
    if (!pool) {
        return 0;
    }
    
    size_t active_workers = 0;
    for (size_t i = 0; i < pool->num_workers; i++) {
        pthread_mutex_lock(&pool->workers[i].mutex);
        if (pool->workers[i].queue_size > 0) {
            active_workers++;
        }
        pthread_mutex_unlock(&pool->workers[i].mutex);
    }
    
    return active_workers;
}

// Get the number of tasks processed by each worker
bool thread_pool_get_tasks_per_worker(thread_pool_t* pool, size_t* tasks_per_worker, size_t array_size) {
    if (!pool || !tasks_per_worker || array_size < pool->num_workers) {
        return false;
    }
    
    for (size_t i = 0; i < pool->num_workers; i++) {
        tasks_per_worker[i] = pool->workers[i].tasks_processed;
    }
    
    return true;
}

// Get the number of steals performed by each worker
bool thread_pool_get_steals_per_worker(thread_pool_t* pool, size_t* steals_per_worker, size_t array_size) {
    if (!pool || !steals_per_worker || array_size < pool->num_workers) {
        return false;
    }
    
    for (size_t i = 0; i < pool->num_workers; i++) {
        steals_per_worker[i] = pool->workers[i].steals;
    }
    
    return true;
}

// Get the CPU ID each worker is bound to
bool thread_pool_get_cpu_ids(thread_pool_t* pool, int* cpu_ids, size_t array_size) {
    if (!pool || !cpu_ids || array_size < pool->num_workers) {
        return false;
    }
    
    for (size_t i = 0; i < pool->num_workers; i++) {
        cpu_ids[i] = pool->workers[i].cpu_id;
    }
    
    return true;
} 