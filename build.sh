#!/bin/bash

# Stop on error
set -e

# Define variables
BUILD_DIR="build"
INSTALL_PREFIX="/usr/local"
BUILD_TYPE="Release"
RUN_TESTS=0

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

# Configure and build
echo "Configuring build..."
cd $BUILD_DIR
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..

echo "Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)

# Run tests if requested
if [ $RUN_TESTS -eq 1 ]; then
    echo "Running tests..."
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