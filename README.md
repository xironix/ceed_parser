# Ceed Parser

A high-performance tool for parsing and validating cryptocurrency seed phrases, mnemonics, and wallets.

## Features

- Fast validation of BIP39 seed phrases
- Support for multiple languages (English, Japanese, Korean, Spanish, etc.)
- Generation of wallet addresses from seed phrases
- Comprehensive error reporting for invalid phrases
- Fast binary search for word lookups
- Multi-threaded processing for high throughput
- SIMD-accelerated word matching and validation
- Work-stealing thread pool for optimal CPU utilization
- Custom memory pool for reduced allocation overhead
- High-performance caching for frequently accessed data
- Thread-safe logging system with multiple output options
- Comprehensive test suite with Unity framework

## Requirements

- C compiler with C11 support
- CMake 3.10 or higher
- OpenSSL development libraries
- SQLite3 development libraries (optional, for database support)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

For optimal performance, use:

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NATIVE=ON ..
make
```

## Testing

The project uses the Unity testing framework for unit tests. To run the tests:

```bash
cd build
ctest
```

To run specific test suites:

```bash
cd build
./bin/ceed_parser_tests mnemonic  # Run only mnemonic tests
./bin/ceed_parser_tests wallet    # Run only wallet tests
./bin/ceed_parser_tests parser    # Run only parser tests
./bin/ceed_parser_tests memory    # Run only memory tests
```

## Project Structure

```
.
├── include/            # Header files
├── src/                # Source files
├── test/               # Unit tests
├── adhoc_tests/        # Ad-hoc test files (not part of the main build)
├── data/               # Data files (wordlists, etc.)
├── .github/workflows/  # CI configuration
├── CMakeLists.txt      # CMake build configuration
├── build.sh            # Build script
└── run_tests.sh        # Test runner script
```

## Continuous Integration

The project uses GitHub Actions for continuous integration, with the following workflows:

- **Build and Test**: Builds the project and runs tests on multiple platforms
- **Static Analysis**: Runs static analysis tools (cppcheck, clang-tidy)
- **Code Coverage**: Generates code coverage reports
- **Sanitizers**: Runs tests with various sanitizers (address, undefined behavior, thread)

## Logging System

The project includes a thread-safe logging system with the following features:

- Multiple log levels (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
- Multiple output destinations (console, file, syslog, callback)
- Customizable formatting
- Thread safety
- Colored output
- Conditional logging
- Assertion macros

Example usage:

```c
#include "logger.h"

int main() {
    // Initialize logger
    logger_init(LOG_INFO, LOG_OUTPUT_CONSOLE | LOG_OUTPUT_FILE, "app.log");
    
    // Log messages
    LOG_INFO("Application started");
    LOG_DEBUG("Debug message");
    LOG_ERROR("Error: %s", strerror(errno));
    
    // Conditional logging
    LOG_INFO_IF(condition, "This will only be logged if condition is true");
    
    // Assertions
    LOG_ASSERT(ptr != NULL, "Pointer should not be NULL");
    
    // Shutdown logger
    logger_shutdown();
    
    return 0;
}
```

## Memory Management

The project includes a high-performance memory pool implementation with the following features:

- Thread-local memory pools
- Fast allocation and deallocation
- Reduced fragmentation
- Automatic cleanup
- Memory statistics

Example usage:

```c
#include "memory_pool.h"

int main() {
    // Create memory pool
    memory_pool_t *pool = memory_pool_create(1024, 10);
    
    // Allocate memory
    void *ptr = memory_pool_malloc(pool, 512);
    
    // Use memory
    memset(ptr, 0, 512);
    
    // Free memory
    memory_pool_free(pool, ptr);
    
    // Destroy memory pool
    memory_pool_destroy(pool);
    
    return 0;
}
```

## Performance Optimizations

The Ceed Parser employs multiple cutting-edge optimization techniques:

1. **SIMD Acceleration**: Vectorized operations for fast word matching and validation using AVX2/SSE/NEON
2. **Custom Memory Management**: Thread-local memory pools to minimize allocation overhead
3. **Work-stealing Thread Pool**: Adaptive load balancing across CPU cores
4. **High-performance Caching**: Lock-free cache with optimized memory access patterns
5. **Branch Prediction Hints**: Strategic use of __builtin_expect for hot paths
6. **Cache Line Alignment**: Data structures aligned to avoid false sharing
7. **Link-Time Optimization**: Whole-program optimization for maximum performance
8. **CPU-specific Tuning**: Automatic detection and use of optimal instructions for the host CPU
9. **Bloom Filters**: Ultra-fast approximate membership testing for wordlists
10. **Optimized Memory Layout**: Minimizing cache misses through careful data organization

## License

This project is licensed under the MIT License - see the LICENSE file for details.
