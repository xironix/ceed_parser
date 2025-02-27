#!/bin/bash
set -e
set -x

echo "Setting up test environment..."

# Make sure wordlist files are accessible
cd "$(dirname "$0")"

# Print current directory for debugging
PWD=$(pwd)
echo "Current directory: $PWD"

# Check for wordlist directory 
if [ ! -d "data" ]; then
  echo "ERROR: 'data' directory not found!"
  exit 1
fi

echo "Checking for wordlist files..."
if [ ! -f "data/english.txt" ]; then
  echo "ERROR: 'data/english.txt' not found!"
  exit 1
fi

# Run the tests with proper environment setup
cd build
echo "Running tests from: $(pwd)"

# Use absolute path for wordlist dir
WORDLIST_DIR="$PWD/../data"
echo "Using wordlist directory: $WORDLIST_DIR"

# Create a customized test runner that forces the right paths
cat > test_runner.c << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    // Set environment variables if needed
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "./bin/ceed_parser_tests");
    
    printf("Running tests with command: %s\n", cmd);
    return system(cmd);
}
EOF

# Compile the test runner
gcc -o test_runner test_runner.c

echo "Running tests..."
./test_runner

echo "Tests completed." 