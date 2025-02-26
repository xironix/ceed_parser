cmake_minimum_required(VERSION 3.10)
project(ceed_parser VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Determine the architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(IS_ARM64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
    set(IS_X86_64 TRUE)
endif()

# Common compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Debug build flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0 -DDEBUG")

# Release build flags with architecture-specific optimizations
if(IS_ARM64)
    # ARM64-specific optimizations
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native -flto -fno-omit-frame-pointer -DNDEBUG")
    # Enable ARM NEON SIMD instructions
    add_definitions(-DUSE_NEON)
elseif(IS_X86_64)
    # x86_64-specific optimizations
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -march=native -flto -fno-omit-frame-pointer -DNDEBUG")
    # Check for AVX2 support
    include(CheckCCompilerFlag)
    check_c_compiler_flag(-mavx2 AVX2_SUPPORTED)
    if(AVX2_SUPPORTED)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -mavx2")
        add_definitions(-DUSE_AVX2)
    endif()
else()
    # Generic optimizations
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -flto -fno-omit-frame-pointer -DNDEBUG")
endif()

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Handle SQLite3
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SQLite3 QUIET sqlite3)
endif()

if(NOT SQLite3_FOUND)
    # If not found via pkg-config, try to find it manually
    find_path(SQLite3_INCLUDE_DIRS sqlite3.h)
    find_library(SQLite3_LIBRARIES sqlite3)
    if(SQLite3_INCLUDE_DIRS AND SQLite3_LIBRARIES)
        set(SQLite3_FOUND TRUE)
    endif()
endif()

if(NOT SQLite3_FOUND)
    message(WARNING "SQLite3 not found, building without database support")
    add_definitions(-DNO_DATABASE_SUPPORT)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    /Library/Frameworks/GStreamer.framework/Headers
)

# Source files
set(SOURCES
    src/main.c
    src/seed_parser.c
    src/mnemonic.c
    src/wallet.c
    src/sha3.c
    src/simd_utils.c
    src/memory_pool.c
    src/thread_pool.c
    src/cache.c
    src/seed_parser_optimized.c
)

# Option to use optimized seed parser
option(USE_OPTIMIZED_PARSER "Use the optimized version of the seed parser" ON)

if(USE_OPTIMIZED_PARSER)
    add_definitions(-DUSE_OPTIMIZED_PARSER)
endif()

# Add Apple Silicon specific optimizations
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Configuring for Apple Silicon (ARM64)")
    
    # Enable ARM NEON SIMD instructions explicitly
    add_compile_options(-march=armv8.5-a+simd+crypto)
    
    # Use Apple's accelerate framework for optimized math
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(ACCELERATE_FRAMEWORK)
        message(STATUS "Found Accelerate framework: ${ACCELERATE_FRAMEWORK}")
        list(APPEND EXTRA_LIBS ${ACCELERATE_FRAMEWORK})
        add_definitions(-DUSE_ACCELERATE_FRAMEWORK)
    endif()
    
    # Enable link time optimization for maximum performance
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    
    # Use Apple-specific memory alignment for best performance
    add_compile_definitions(APPLE_SILICON=1)
    add_compile_definitions(PREFERRED_ALIGNMENT=16)
    
    # Set optimal thread count based on M-series chip core architecture
    add_compile_definitions(DEFAULT_PERFORMANCE_CORES=8)
    add_compile_definitions(DEFAULT_EFFICIENCY_CORES=2)
    
    # Add fast math optimizations (be careful if exact IEEE compliance is needed)
    add_compile_options(-ffast-math)
    
    # Tune specifically for M2 architecture
    add_compile_options(-mcpu=apple-m2)
    
    # Use Apple's Grand Central Dispatch integration
    add_compile_definitions(USE_GCD=1)
endif()

# Add executable
add_executable(ceed_parser ${SOURCES})

# Link libraries
target_link_libraries(ceed_parser
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    m  # Add math library
    ${EXTRA_LIBS}  # Added Apple Accelerate framework if available
)

if(SQLite3_FOUND)
    target_link_libraries(ceed_parser ${SQLite3_LIBRARIES})
endif()

# Enable Link Time Optimization if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED OUTPUT LTO_ERROR)
if(LTO_SUPPORTED)
    set_property(TARGET ceed_parser PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    message(STATUS "LTO/IPO enabled for better performance")
else()
    message(STATUS "LTO/IPO not supported: ${LTO_ERROR}")
endif()

# Install targets
install(TARGETS ceed_parser DESTINATION bin)

# Install data files
install(DIRECTORY data/ DESTINATION share/ceed_parser/data)

# Create data directory in build directory
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data)

# Copy wordlists to build directory
file(GLOB WORDLISTS "data/*.txt")
foreach(WORDLIST ${WORDLISTS})
    get_filename_component(WORDLIST_NAME ${WORDLIST} NAME)
    configure_file(${WORDLIST} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data/${WORDLIST_NAME} COPYONLY)
endforeach()

# Add uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)

# Configure benchmark build option
option(BUILD_BENCHMARKS "Build benchmarking tools" ON)
if(BUILD_BENCHMARKS)
    # Create a copy of SOURCES without main.c for the benchmark
    set(BENCHMARK_SOURCES ${SOURCES})
    list(REMOVE_ITEM BENCHMARK_SOURCES src/main.c)
    
    add_executable(bench_ceed_parser src/benchmark.c ${BENCHMARK_SOURCES})
    target_compile_definitions(bench_ceed_parser PRIVATE -DBENCHMARK_MODE)
    target_link_libraries(bench_ceed_parser
        ${OPENSSL_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        m
    )
    if(SQLite3_FOUND)
        target_link_libraries(bench_ceed_parser ${SQLite3_LIBRARIES})
    endif()
    if(LTO_SUPPORTED)
        set_property(TARGET bench_ceed_parser PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()

# Add README
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
"# Ceed Parser

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

## Usage

```bash
./seed_parser [options] <seed_phrase>
```

See `./seed_parser --help` for more options.

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
")

# Add license file
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
"MIT License

Copyright (c) 2023 Ceed Parser Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the \"Software\"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
")

message(STATUS "Configured ceed_parser with the following performance settings:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler flags: ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "  Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  LTO/IPO: ${LTO_SUPPORTED}")
if(IS_X86_64 AND AVX2_SUPPORTED)
    message(STATUS "  SIMD: AVX2 enabled")
elseif(IS_ARM64)
    message(STATUS "  SIMD: NEON enabled")
endif()
message(STATUS "  OpenSSL: ${OPENSSL_FOUND}")
message(STATUS "  SQLite3: ${SQLite3_FOUND}")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")

# Enable testing
enable_testing()

# Define test sources
set(TEST_SOURCES
    test/test_main.c
    test/test_mnemonic.c
    test/test_wallet.c
    test/test_parser.c
    src/mnemonic.c
    src/wallet.c
    src/seed_parser.c
    src/sha3.c
)

# Add test executable
add_executable(ceed_parser_tests ${TEST_SOURCES})

# Link test libraries
target_link_libraries(ceed_parser_tests
    ${OPENSSL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    m  # Add math library
)

if(SQLite3_FOUND)
    target_link_libraries(ceed_parser_tests ${SQLite3_LIBRARIES})
endif()

# Copy data directory for tests
add_custom_command(
    TARGET ceed_parser_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/data ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data
)

# Register tests
add_test(NAME all_tests COMMAND ceed_parser_tests)
add_test(NAME mnemonic_tests COMMAND ceed_parser_tests mnemonic)
add_test(NAME wallet_tests COMMAND ceed_parser_tests wallet)
add_test(NAME parser_tests COMMAND ceed_parser_tests parser) 