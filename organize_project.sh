#!/bin/bash
# Script to organize the project structure

set -e

echo "Organizing project structure..."

# Create directory for ad-hoc tests if it doesn't exist
mkdir -p adhoc_tests

# Move ad-hoc test files to the dedicated directory
for file in test_*.c debug_*.c; do
    if [ -f "$file" ]; then
        echo "Moving $file to adhoc_tests/"
        mv "$file" adhoc_tests/
    fi
done

# Move compiled test binaries
for file in test_*; do
    if [ -f "$file" ] && [ ! -f "$file.c" ]; then
        echo "Moving binary $file to adhoc_tests/"
        mv "$file" adhoc_tests/
    fi
done

# Clean up object files
echo "Cleaning up object files..."
rm -f *.o

echo "Project organization complete."
echo "Ad-hoc tests have been moved to the 'adhoc_tests' directory."
echo "Run 'make clean' to remove all temporary files." 