#!/bin/bash
# Build script for Ceed Parser

set -e # Exit on any error
set -x # Print commands as they are executed

# Ensure proper directory structure
echo "Setting up build environment..."
rm -rf build
mkdir -p build
mkdir -p build/bin
mkdir -p build/bin/data
cd build

# Create symlinks instead of copying the wordlist files
echo "Creating symlinks for wordlist data..."
for wordlist in ../data/*.txt; do
    filename=$(basename "$wordlist")
    ln -sf "$(pwd)/../data/$filename" "$(pwd)/bin/data/$filename"
done

# Create test directories
mkdir -p test/data

# Configure with CMake
echo "Configuring build..."
cmake ..

# Build
echo "Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo "Build complete." 