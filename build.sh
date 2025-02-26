#!/bin/bash

# Stop on error
set -e

# Define variables
BUILD_DIR="build"
INSTALL_PREFIX="/usr/local"
BUILD_TYPE="Release"
RUN_TESTS=1

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --install)
            DO_INSTALL=1
            shift
            ;;
        --prefix=*)
            INSTALL_PREFIX="${1#*=}"
            shift
            ;;
        --test)
            RUN_TESTS=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "  --debug          Build in debug mode"
            echo "  --install        Install after building"
            echo "  --prefix=<path>  Installation prefix (default: /usr/local)"
            echo "  --test           Build and run tests"
            echo "  --help, -h       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR

# Make sure the data directory exists
if [ ! -d "data" ]; then
    echo "Error: data directory not found. Please ensure the wordlist files are available."
    exit 1
fi

# Ensure the wordlist data directories exist for tests
echo "Setting up wordlist data for tests..."
mkdir -p $BUILD_DIR/data
# Use symlinks instead of copying for read-only files
if [ ! -L "$BUILD_DIR/data/english.txt" ]; then 
    # Remove any existing file
    rm -f "$BUILD_DIR/data/english.txt"
    # Create symlink to wordlist data
    ln -sf "$(pwd)/data/english.txt" "$BUILD_DIR/data/english.txt"
    echo "Created symlink for english.txt"
fi

# Link other wordlist files - these are optional but will be linked if they exist
for wordlist in spanish.txt chinese_simplified.txt chinese_traditional.txt czech.txt french.txt italian.txt japanese.txt korean.txt portuguese.txt monero_english.txt monero_chinese_simplified.txt monero_dutch.txt monero_esperanto.txt monero_french.txt monero_german.txt monero_italian.txt monero_japanese.txt monero_lojban.txt monero_portuguese.txt monero_russian.txt monero_spanish.txt; do
    if [ -f "data/$wordlist" ] && [ ! -L "$BUILD_DIR/data/$wordlist" ]; then
        # Remove any existing file 
        rm -f "$BUILD_DIR/data/$wordlist"
        # Create symlink
        ln -sf "$(pwd)/data/$wordlist" "$BUILD_DIR/data/$wordlist"
        echo "Created symlink for $wordlist"
    fi
done

# Configure and build
echo "Configuring build..."
cd $BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..

echo "Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

# Run tests if requested
if [ $RUN_TESTS -eq 1 ]; then
    echo "Running tests..."
    # Make sure wordlist data is accessible in the current directory too
    mkdir -p ./data
    # Create symlinks in the current directory for tests run from build dir
    for wordlist in english.txt spanish.txt monero_english.txt; do
        if [ -f "../data/$wordlist" ] && [ ! -L "./data/$wordlist" ]; then
            # Remove any existing file 
            rm -f "./data/$wordlist"
            # Create symlink
            ln -sf "$(pwd)/../data/$wordlist" "./data/$wordlist"
            echo "Created symlink for $wordlist in current directory"
        fi
    done
    ctest -V
    echo "Test execution complete"
fi

# Install if requested
if [ -n "$DO_INSTALL" ]; then
    echo "Installing to $INSTALL_PREFIX..."
    make install
fi

echo "Build completed successfully!"
if [ $RUN_TESTS -eq 1 ]; then
    echo "Tests executed successfully!"
fi 