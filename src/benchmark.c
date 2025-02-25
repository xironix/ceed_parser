/**
 * @file benchmark.c
 * @brief Benchmarking system for Ceed Parser
 *
 * This file implements a comprehensive benchmarking system for testing
 * the performance of various components of the Ceed Parser application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "seed_parser.h"
#include "mnemonic.h"
#include "wallet.h"

// Define benchmark types
#define BENCH_WORDLIST         0x01  // Test wordlist loading and lookup
#define BENCH_MNEMONIC         0x02  // Test mnemonic validation
#define BENCH_WALLET           0x04  // Test wallet generation
#define BENCH_FILE_IO          0x08  // Test file I/O operations
#define BENCH_PARALLEL         0x10  // Test parallel processing
#define BENCH_DATABASE         0x20  // Test database operations
#define BENCH_FULL_SCAN        0x40  // Test full directory scan
#define BENCH_ALL              0xFF  // Run all benchmarks

// Configuration
#define BENCH_DEFAULT_THREADS  4
#define BENCH_MAX_THREADS      128
#define BENCH_TEST_PHRASES     10000000
#define BENCH_TEST_FILES       100
#define BENCH_FILE_SIZE        (1 * 1024 * 1024)  // 1MB
#define BENCH_ITERATIONS       5
#define BENCH_WARMUP           2

// Globals
static volatile sig_atomic_t g_running = 1;
static char* g_test_dir = NULL;
static int g_num_threads = BENCH_DEFAULT_THREADS;
static int g_bench_types = BENCH_ALL;
static bool g_verbose = false;
static FILE* g_output_file = NULL;

typedef struct {
    double elapsed_time;
    double cpu_time;
    double throughput;
    double memory_used;
    double memory_peak;
} benchmark_result_t;

// Forward declarations
static void cleanup_test_files(void);
static void create_test_files(void);
static void handle_signal(int sig);
static benchmark_result_t run_benchmark(int bench_type);
static benchmark_result_t bench_wordlist(void);
static benchmark_result_t bench_mnemonic(void);
static benchmark_result_t bench_wallet(void);
static benchmark_result_t bench_file_io(void);
static benchmark_result_t bench_parallel(void);
static benchmark_result_t bench_database(void);
static benchmark_result_t bench_full_scan(void);
static double get_current_memory(void);
static double get_elapsed_time(struct timespec* start, struct timespec* end);
static const char* get_bench_name(int bench_type);
static void print_system_info(void);
static void print_benchmark_result(int bench_type, benchmark_result_t result);
static void print_usage(const char* program_name);
static void generate_random_text(char* buffer, size_t size);
static char** generate_random_phrases(int count);
static void free_phrases(char** phrases, int count);

/**
 * @brief Main entry point for the benchmark
 */
int main(int argc, char* argv[]) {
    int i, opt;
    struct timespec start_time, end_time;
    benchmark_result_t results[8];
    double total_score = 0.0;
    
    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Parse command line arguments
    while ((opt = getopt(argc, argv, "t:o:vhw:m:p:d:a:f")) != -1) {
        switch (opt) {
            case 't':
                g_num_threads = atoi(optarg);
                if (g_num_threads <= 0 || g_num_threads > BENCH_MAX_THREADS) {
                    g_num_threads = BENCH_DEFAULT_THREADS;
                }
                break;
            case 'o':
                g_output_file = fopen(optarg, "w");
                if (!g_output_file) {
                    fprintf(stderr, "Error opening output file %s\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            case 'v':
                g_verbose = true;
                break;
            case 'w':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_WORDLIST;
                }
                break;
            case 'm':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_MNEMONIC;
                }
                break;
            case 'p':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_PARALLEL;
                }
                break;
            case 'd':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_DATABASE;
                }
                break;
            case 'a':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_WALLET;
                }
                break;
            case 'f':
                if (strcmp(optarg, "only") == 0) {
                    g_bench_types = BENCH_FILE_IO;
                }
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_SUCCESS;
        }
    }
    
    // Create temporary directory for test files
    g_test_dir = strdup("/tmp/ceed_benchmark_XXXXXX");
    if (mkdtemp(g_test_dir) == NULL) {
        fprintf(stderr, "Failed to create temporary directory\n");
        return EXIT_FAILURE;
    }
    
    printf("Ceed Parser Benchmark Suite\n");
    printf("===========================\n\n");
    
    // Print system information
    print_system_info();
    
    // Create test files
    printf("Preparing benchmark environment...\n");
    create_test_files();
    
    printf("\nRunning benchmarks with %d threads...\n\n", g_num_threads);
    
    // Run selected benchmarks
    int result_idx = 0;
    
    if (g_bench_types & BENCH_WORDLIST) {
        results[result_idx++] = run_benchmark(BENCH_WORDLIST);
    }
    
    if (g_bench_types & BENCH_MNEMONIC) {
        results[result_idx++] = run_benchmark(BENCH_MNEMONIC);
    }
    
    if (g_bench_types & BENCH_WALLET) {
        results[result_idx++] = run_benchmark(BENCH_WALLET);
    }
    
    if (g_bench_types & BENCH_FILE_IO) {
        results[result_idx++] = run_benchmark(BENCH_FILE_IO);
    }
    
    if (g_bench_types & BENCH_PARALLEL) {
        results[result_idx++] = run_benchmark(BENCH_PARALLEL);
    }
    
    if (g_bench_types & BENCH_DATABASE) {
        results[result_idx++] = run_benchmark(BENCH_DATABASE);
    }
    
    if (g_bench_types & BENCH_FULL_SCAN) {
        results[result_idx++] = run_benchmark(BENCH_FULL_SCAN);
    }
    
    // Calculate and print combined score
    printf("\nBenchmark Summary\n");
    printf("=================\n");
    
    for (i = 0; i < result_idx; i++) {
        total_score += results[i].throughput;
    }
    
    total_score /= result_idx;
    
    printf("Overall Performance Score: %.2f units/s\n", total_score);
    
    // Clean up
    if (g_output_file) {
        fprintf(g_output_file, "Overall Performance Score: %.2f units/s\n", total_score);
        fclose(g_output_file);
    }
    
    cleanup_test_files();
    free(g_test_dir);
    
    return EXIT_SUCCESS;
}

/**
 * @brief Run a specific benchmark with iterations and warmup
 */
static benchmark_result_t run_benchmark(int bench_type) {
    benchmark_result_t result = {0};
    benchmark_result_t best_result = {0};
    benchmark_result_t (*bench_func)(void) = NULL;
    int i;
    
    // Select the benchmark function
    switch (bench_type) {
        case BENCH_WORDLIST:
            bench_func = bench_wordlist;
            break;
        case BENCH_MNEMONIC:
            bench_func = bench_mnemonic;
            break;
        case BENCH_WALLET:
            bench_func = bench_wallet;
            break;
        case BENCH_FILE_IO:
            bench_func = bench_file_io;
            break;
        case BENCH_PARALLEL:
            bench_func = bench_parallel;
            break;
        case BENCH_DATABASE:
            bench_func = bench_database;
            break;
        case BENCH_FULL_SCAN:
            bench_func = bench_full_scan;
            break;
        default:
            return result;
    }
    
    printf("Running %s benchmark... ", get_bench_name(bench_type));
    fflush(stdout);
    
    // Warmup runs
    for (i = 0; i < BENCH_WARMUP; i++) {
        if (g_verbose) {
            printf("\n  Warmup %d/%d... ", i + 1, BENCH_WARMUP);
            fflush(stdout);
        }
        bench_func();
    }
    
    // Measured runs
    best_result.elapsed_time = 999999.0; // Initialize to a high value
    
    for (i = 0; i < BENCH_ITERATIONS; i++) {
        if (g_verbose) {
            printf("\n  Iteration %d/%d... ", i + 1, BENCH_ITERATIONS);
            fflush(stdout);
        }
        
        result = bench_func();
        
        // Keep the best (fastest) result
        if (result.elapsed_time < best_result.elapsed_time) {
            best_result = result;
        }
    }
    
    printf("done.\n");
    
    // Print the results
    print_benchmark_result(bench_type, best_result);
    
    return best_result;
}

/**
 * @brief Benchmark wordlist loading and lookup
 */
static benchmark_result_t bench_wordlist(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    MnemonicContext ctx;
    char* words[] = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb",
        "abstract", "absurd", "abuse", "access", "accident", "account"
    };
    int i, j, found = 0;
    size_t memory_start, memory_peak = 0;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize mnemonic context
    mnemonic_init(&ctx);
    
    // Load all supported wordlists
    for (i = 0; i < LANGUAGE_COUNT; i++) {
        mnemonic_load_wordlist(&ctx, i);
        
        // Check peak memory
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
    }
    
    // Perform lookups
    for (i = 0; i < 10000; i++) {
        for (j = 0; j < 13; j++) {
            if (mnemonic_word_exists(&ctx, LANGUAGE_ENGLISH, words[j])) {
                found++;
            }
        }
    }
    
    // Clean up
    mnemonic_cleanup(&ctx);
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = (10000.0 * 13.0) / result.elapsed_time;
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark mnemonic validation
 */
static benchmark_result_t bench_mnemonic(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    MnemonicContext ctx;
    int i, valid_count = 0;
    char** phrases = generate_random_phrases(10000);
    size_t memory_start, memory_peak = 0;
    MnemonicType type;
    MnemonicLanguage lang;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize mnemonic context
    mnemonic_init(&ctx);
    
    // Load all supported wordlists
    for (i = 0; i < LANGUAGE_COUNT; i++) {
        mnemonic_load_wordlist(&ctx, i);
    }
    
    // Validate mnemonics
    for (i = 0; i < 10000; i++) {
        if (mnemonic_validate(&ctx, phrases[i], &type, &lang)) {
            valid_count++;
        }
        
        // Check peak memory
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
    }
    
    // Clean up
    mnemonic_cleanup(&ctx);
    free_phrases(phrases, 10000);
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = 10000.0 / result.elapsed_time;
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark wallet generation
 */
static benchmark_result_t bench_wallet(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    wallet_context_t ctx;
    char* mnemonic = "abandon ability able about above absent absorb abstract absurd abuse access accident account";
    int i;
    size_t memory_start, memory_peak = 0;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize wallet context
    wallet_init(&ctx);
    
    for (i = 0; i < 1000; i++) {
        wallet_t wallet;
        
        // Generate wallet from mnemonic
        wallet_from_mnemonic(&ctx, &wallet, mnemonic, CRYPTO_BTC);
        
        // Generate addresses
        wallet_generate_addresses(&ctx, &wallet, 5);
        
        // Check peak memory
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
    }
    
    // Clean up
    wallet_cleanup(&ctx);
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = 1000.0 / result.elapsed_time;
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark file I/O operations
 */
static benchmark_result_t bench_file_io(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    char filepath[PATH_MAX];
    char buffer[8192];
    int i, fd;
    size_t total_bytes = 0;
    size_t memory_start, memory_peak = 0;
    DIR* dir;
    struct dirent* entry;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Open test directory
    dir = opendir(g_test_dir);
    if (dir == NULL) {
        perror("opendir");
        return result;
    }
    
    // Read files
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            snprintf(filepath, PATH_MAX, "%s/%s", g_test_dir, entry->d_name);
            
            fd = open(filepath, O_RDONLY);
            if (fd == -1) {
                continue;
            }
            
            ssize_t bytes_read;
            while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                total_bytes += (size_t)bytes_read;
                
                // Check peak memory
                size_t current_memory = (size_t)get_current_memory();
                if (current_memory > memory_peak) {
                    memory_peak = current_memory;
                }
            }
            
            close(fd);
        }
    }
    
    closedir(dir);
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = (double)total_bytes / (result.elapsed_time * 1024.0 * 1024.0); // MB/s
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark parallel processing
 */
static benchmark_result_t bench_parallel(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    SeedParserConfig config;
    SeedParserStats stats;
    size_t memory_start, memory_peak = 0;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Initialize configuration
    memset(&config, 0, sizeof(config));
    config.output_file = "/dev/null";
    config.database_file = NULL;
    config.thread_count = g_num_threads;
    config.detect_monero = true;
    config.recursive = true;
    config.fast_mode = true;
    config.verbose = false;
    
    // Add languages
    config.languages[0] = LANGUAGE_ENGLISH;
    config.languages[1] = LANGUAGE_SPANISH;
    
    // Add supported word chain sizes
    config.word_chain_sizes[0] = 12;
    config.word_chain_sizes[1] = 15;
    config.word_chain_sizes[2] = 18;
    config.word_chain_sizes[3] = 21;
    config.word_chain_sizes[4] = 24;
    config.word_chain_sizes[5] = 25; // Monero
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize the seed parser
    seed_parser_init(&config, &stats);
    
    // Start the parser
    seed_parser_start(g_test_dir);
    
    // Check peak memory during processing
    do {
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
        usleep(10000); // 10ms
    } while (stats.active_threads > 0);
    
    // Stop the parser
    seed_parser_stop();
    
    // Clean up
    seed_parser_cleanup();
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = (double)(stats.bytes_processed) / (result.elapsed_time * 1024.0 * 1024.0); // MB/s
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark database operations
 */
static benchmark_result_t bench_database(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    size_t memory_start, memory_peak = 0;
    char db_path[PATH_MAX];
    
    // Skip if no database support
#ifdef NO_DATABASE_SUPPORT
    result.elapsed_time = 0.0;
    result.throughput = 0.0;
    result.memory_used = 0.0;
    result.memory_peak = 0.0;
    return result;
#endif
    
    // Create database path
    snprintf(db_path, PATH_MAX, "%s/benchmark.db", g_test_dir);
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Initialize configuration
    SeedParserConfig config;
    memset(&config, 0, sizeof(config));
    config.output_file = "/dev/null";
    config.database_file = db_path;
    config.thread_count = 1;
    config.detect_monero = true;
    config.recursive = false;
    config.fast_mode = true;
    config.verbose = false;
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize the seed parser (this creates the database)
    SeedParserStats stats;
    seed_parser_init(&config, &stats);
    
    // Generate and save some test data
    for (int i = 0; i < 1000; i++) {
        char mnemonic[512];
        snprintf(mnemonic, sizeof(mnemonic), "test phrase %d word1 word2 word3 word4 word5 word6 word7 word8 word9 word10 word11 word12", i);
        
        // Save to database directly (internal function call)
        seed_parser_save_phrase(mnemonic, "test_file.txt", MNEMONIC_BIP39, LANGUAGE_ENGLISH);
        
        // Check peak memory
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
    }
    
    // Clean up
    seed_parser_cleanup();
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = 1000.0 / result.elapsed_time;
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Benchmark full directory scan
 */
static benchmark_result_t bench_full_scan(void) {
    benchmark_result_t result = {0};
    struct timespec start, end;
    SeedParserConfig config;
    SeedParserStats stats;
    size_t memory_start, memory_peak = 0;
    
    // Initialize memory tracking
    memory_start = (size_t)get_current_memory();
    
    // Initialize configuration
    memset(&config, 0, sizeof(config));
    config.output_file = "/dev/null";
    config.database_file = NULL;
    config.thread_count = g_num_threads;
    config.detect_monero = true;
    config.recursive = true;
    config.fast_mode = true;
    config.verbose = false;
    config.generate_addresses = true;
    config.address_count = 1;
    
    // Add languages
    config.languages[0] = LANGUAGE_ENGLISH;
    config.languages[1] = LANGUAGE_SPANISH;
    
    // Add supported word chain sizes
    config.word_chain_sizes[0] = 12;
    config.word_chain_sizes[1] = 15;
    config.word_chain_sizes[2] = 18;
    config.word_chain_sizes[3] = 21;
    config.word_chain_sizes[4] = 24;
    config.word_chain_sizes[5] = 25; // Monero
    
    // Start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Initialize the seed parser
    seed_parser_init(&config, &stats);
    
    // Start the parser
    seed_parser_start(g_test_dir);
    
    // Wait for completion
    do {
        // Check peak memory
        size_t current_memory = (size_t)get_current_memory();
        if (current_memory > memory_peak) {
            memory_peak = current_memory;
        }
        
        usleep(10000); // 10ms
    } while (stats.active_threads > 0);
    
    // Clean up
    seed_parser_stop();
    seed_parser_cleanup();
    
    // Stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate results
    result.elapsed_time = get_elapsed_time(&start, &end);
    result.throughput = (double)(stats.bytes_processed) / (result.elapsed_time * 1024.0 * 1024.0); // MB/s
    result.memory_used = (double)(memory_start) / (1024.0 * 1024.0);
    result.memory_peak = (double)(memory_peak) / (1024.0 * 1024.0);
    
    return result;
}

/**
 * @brief Create test files for benchmarking
 */
static void create_test_files(void) {
    int i;
    char filepath[PATH_MAX];
    FILE* file;
    char* buffer;
    
    // Allocate buffer
    buffer = (char*)malloc(BENCH_FILE_SIZE);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    // Create test files
    for (i = 0; i < BENCH_TEST_FILES; i++) {
        snprintf(filepath, PATH_MAX, "%s/test_file_%03d.txt", g_test_dir, i);
        
        file = fopen(filepath, "w");
        if (!file) {
            perror("fopen");
            free(buffer);
            exit(EXIT_FAILURE);
        }
        
        // Generate random text
        generate_random_text(buffer, BENCH_FILE_SIZE);
        
        // Add some potential seed phrases
        if (i % 10 == 0) {
            const char* phrases[] = {
                "abandon ability able about above absent absorb abstract absurd abuse access accident account",
                "above absent absorb abstract absurd abuse access accident account accuse achieve acid acoustic",
                "acoustic acquire across act action actor actress actual adapt add addict address adjust adult"
            };
            
            // Insert phrases randomly in the buffer
            for (int j = 0; j < 3; j++) {
                size_t pos = rand() % (BENCH_FILE_SIZE - strlen(phrases[j]) - 1);
                strcpy(buffer + pos, phrases[j]);
            }
        }
        
        // Write to file
        fwrite(buffer, 1, BENCH_FILE_SIZE, file);
        fclose(file);
    }
    
    // Create subdirectories with files
    for (i = 0; i < 5; i++) {
        char subdir[PATH_MAX];
        snprintf(subdir, PATH_MAX, "%s/subdir_%d", g_test_dir, i);
        
        if (mkdir(subdir, 0755) != 0) {
            perror("mkdir");
            free(buffer);
            exit(EXIT_FAILURE);
        }
        
        // Create files in subdirectory
        for (int j = 0; j < 10; j++) {
            snprintf(filepath, PATH_MAX, "%s/test_file_%03d.txt", subdir, j);
            
            file = fopen(filepath, "w");
            if (!file) {
                perror("fopen");
                free(buffer);
                exit(EXIT_FAILURE);
            }
            
            // Generate random text
            generate_random_text(buffer, BENCH_FILE_SIZE);
            
            // Write to file
            fwrite(buffer, 1, BENCH_FILE_SIZE, file);
            fclose(file);
        }
    }
    
    free(buffer);
}

/**
 * @brief Clean up test files
 */
static void cleanup_test_files(void) {
    char command[PATH_MAX + 20];
    
    snprintf(command, sizeof(command), "rm -rf %s", g_test_dir);
    system(command);
}

/**
 * @brief Generate random text
 */
static void generate_random_text(char* buffer, size_t size) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \t\n.,;:!?-_()[]{}'\"";
    size_t i;
    
    for (i = 0; i < size - 1; i++) {
        buffer[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    
    buffer[size - 1] = '\0';
}

/**
 * @brief Generate random phrases for testing
 */
static char** generate_random_phrases(int count) {
    char** phrases = (char**)malloc(count * sizeof(char*));
    if (!phrases) {
        return NULL;
    }
    
    // Load some seed words
    static const char* bip39_words[] = {
        "abandon", "ability", "able", "about", "above", "absent", "absorb",
        "abstract", "absurd", "abuse", "access", "accident", "account", "accuse",
        "achieve", "acid", "acoustic", "acquire", "across", "act", "action",
        "actor", "actress", "actual", "adapt", "add", "addict", "address",
        "adjust", "admit", "adult", "advance", "advice", "aerobic", "affair",
        "afford", "afraid", "again", "age", "agent", "agree", "ahead", "aim",
        "air", "airport", "aisle", "alarm", "album", "alcohol", "alert", "alien",
        "all", "alley", "allow", "almost", "alone", "alpha", "already", "also",
        "alter", "always", "amateur", "amazing", "among", "amount", "amused",
        "analyst", "anchor", "ancient", "anger", "angle", "angry", "animal",
        "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
        "anxiety", "any", "apart", "apology", "appear", "apple", "approve",
        "april", "arch", "arctic", "area", "arena", "argue", "arm", "armed",
        "armor", "army", "around", "arrange", "arrest", "arrive", "arrow", "art"
    };
    
    for (int i = 0; i < count; i++) {
        phrases[i] = (char*)malloc(512 * sizeof(char));
        if (!phrases[i]) {
            // Clean up
            for (int j = 0; j < i; j++) {
                free(phrases[j]);
            }
            free(phrases);
            return NULL;
        }
        
        // Generate a phrase of random length (12, 15, 18, 21, 24, or 25 words)
        int word_count;
        switch (rand() % 6) {
            case 0: word_count = 12; break;
            case 1: word_count = 15; break;
            case 2: word_count = 18; break;
            case 3: word_count = 21; break;
            case 4: word_count = 24; break;
            case 5: word_count = 25; break;
            default: word_count = 12;
        }
        
        phrases[i][0] = '\0';
        
        for (int j = 0; j < word_count; j++) {
            if (j > 0) {
                strcat(phrases[i], " ");
            }
            strcat(phrases[i], bip39_words[rand() % (sizeof(bip39_words) / sizeof(bip39_words[0]))]);
        }
    }
    
    return phrases;
}

/**
 * @brief Free generated phrases
 */
static void free_phrases(char** phrases, int count) {
    for (int i = 0; i < count; i++) {
        free(phrases[i]);
    }
    free(phrases);
}

/**
 * @brief Get the current memory usage
 */
static double get_current_memory(void) {
    struct rusage usage;
    
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return (double)(usage.ru_maxrss);
    }
    
    return 0.0;
}

/**
 * @brief Calculate elapsed time in seconds
 */
static double get_elapsed_time(struct timespec* start, struct timespec* end) {
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

/**
 * @brief Get benchmark name from type
 */
static const char* get_bench_name(int bench_type) {
    switch (bench_type) {
        case BENCH_WORDLIST:  return "Wordlist";
        case BENCH_MNEMONIC:  return "Mnemonic";
        case BENCH_WALLET:    return "Wallet";
        case BENCH_FILE_IO:   return "File I/O";
        case BENCH_PARALLEL:  return "Parallel";
        case BENCH_DATABASE:  return "Database";
        case BENCH_FULL_SCAN: return "Full Scan";
        default:              return "Unknown";
    }
}

/**
 * @brief Print system information
 */
static void print_system_info(void) {
    FILE* file;
    char buffer[1024];
    int num_processors = 0;
    
    // Get processor info
    file = fopen("/proc/cpuinfo", "r");
    if (file) {
        while (fgets(buffer, sizeof(buffer), file)) {
            if (strncmp(buffer, "model name", 10) == 0) {
                printf("CPU: %s", buffer + 13);
                break;
            }
        }
        fclose(file);
    }
    
    // Get number of processors
#ifdef _SC_NPROCESSORS_ONLN
    num_processors = (int)sysconf(_SC_NPROCESSORS_ONLN);
    printf("Number of CPUs: %d\n", num_processors);
#endif
    
    // Get memory info
    file = fopen("/proc/meminfo", "r");
    if (file) {
        while (fgets(buffer, sizeof(buffer), file)) {
            if (strncmp(buffer, "MemTotal:", 9) == 0) {
                printf("Memory: %s", buffer);
                break;
            }
        }
        fclose(file);
    }
    
    // Get OS info
    file = popen("uname -a", "r");
    if (file) {
        if (fgets(buffer, sizeof(buffer), file)) {
            printf("OS: %s", buffer);
        }
        pclose(file);
    }
    
    printf("Threads for benchmark: %d\n", g_num_threads);
    printf("\n");
}

/**
 * @brief Print benchmark result
 */
static void print_benchmark_result(int bench_type, benchmark_result_t result) {
    const char* name = get_bench_name(bench_type);
    
    printf("  %s:\n", name);
    printf("    Time: %.3f seconds\n", result.elapsed_time);
    
    switch (bench_type) {
        case BENCH_WORDLIST:
            printf("    Throughput: %.2f lookups/second\n", result.throughput);
            break;
        case BENCH_MNEMONIC:
            printf("    Throughput: %.2f validations/second\n", result.throughput);
            break;
        case BENCH_WALLET:
            printf("    Throughput: %.2f wallets/second\n", result.throughput);
            break;
        case BENCH_FILE_IO:
            printf("    Throughput: %.2f MB/second\n", result.throughput);
            break;
        case BENCH_PARALLEL:
            printf("    Throughput: %.2f MB/second\n", result.throughput);
            break;
        case BENCH_DATABASE:
            printf("    Throughput: %.2f records/second\n", result.throughput);
            break;
        case BENCH_FULL_SCAN:
            printf("    Throughput: %.2f MB/second\n", result.throughput);
            break;
    }
    
    printf("    Memory used: %.2f MB\n", result.memory_used);
    printf("    Peak memory: %.2f MB\n", result.memory_peak);
    
    if (g_output_file) {
        fprintf(g_output_file, "%s,%.3f,%.2f,%.2f,%.2f\n",
                name, result.elapsed_time, result.throughput,
                result.memory_used, result.memory_peak);
    }
}

/**
 * @brief Handle signals for graceful termination
 */
static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
    printf("\nReceived termination signal. Cleaning up...\n");
}

/**
 * @brief Print usage information
 */
static void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Options:\n");
    printf("  -t THREADS   Number of threads to use (default: %d)\n", BENCH_DEFAULT_THREADS);
    printf("  -o FILE      Output results to a file\n");
    printf("  -v           Verbose output\n");
    printf("  -w only      Run only wordlist benchmark\n");
    printf("  -m only      Run only mnemonic benchmark\n");
    printf("  -p only      Run only parallel benchmark\n");
    printf("  -d only      Run only database benchmark\n");
    printf("  -a only      Run only address benchmark\n");
    printf("  -f only      Run only file I/O benchmark\n");
    printf("  -h           Display this help message\n");
}
