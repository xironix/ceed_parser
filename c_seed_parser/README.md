# Ceed Parser

A high-performance C implementation for scanning directories to extract cryptocurrency seed phrases and private keys, with specialized support for BIP-39 and Monero mnemonic phrases.

## Features

- **Multi-Cryptocurrency Support**: Bitcoin (BIP-39), Ethereum, and Monero
- **Multi-Language Detection**: English, Spanish, and extensible to more languages
- **High-Performance Architecture**: Threaded scanning for maximum throughput
- **SIMD Optimizations**: (When available) for faster wordlist and text processing
- **Recursive Scanning**: Walk directory trees to find all potential files
- **Database Storage**: Option to store findings in SQLite database

## Prerequisites

Before building Ceed Parser, you'll need the following dependencies:

- **CMake** (version 3.10 or higher)
- **C Compiler** (gcc, clang)
- **OpenSSL** development libraries
- **Threads** library (usually included with the C standard library)
- **SQLite3** development libraries (optional, for database support)

### macOS

```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake openssl sqlite3
```

### Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev libsqlite3-dev
```

### Windows (with MSYS2)

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-openssl mingw-w64-x86_64-sqlite3
```

## Building

```bash
# Create a build directory
mkdir -p build
cd build

# Configure with CMake
cmake ../c_seed_parser

# Build
make -j4  # Replace 4 with the number of CPU cores you want to use
```

If OpenSSL is installed in a non-standard location, you may need to specify its path:

```bash
cmake -DOPENSSL_ROOT_DIR=/path/to/openssl ../c_seed_parser
```

## Usage

```
Usage: ceed_parser [OPTIONS] [PATHS...]

Options:
  -h, --help                   Show this help message
  -t, --threads N              Number of threads to use (default: 4)
  -o, --output FILE            Output file (default: found_seeds.txt)
  -r, --recursive              Recursively scan directories
  -d, --database FILE          SQLite database file for results
  -f, --fast                   Fast mode (sacrifice accuracy for speed)
  -a, --addresses N            Number of addresses to generate (default: 1)
  -l, --languages LANGS        Comma-separated list of languages (en,es,fr,it,ja,ko,zh-s,zh-t)
  -m, --monero                 Enable Monero seed phrase detection (25 words)
  -p, --performance            Show performance information
  -c, --cpu-info               Show CPU information
```

## Examples

```bash
# Scan current directory for seed phrases
./ceed_parser .

# Scan directory recursively with 8 threads
./ceed_parser -r -t 8 /path/to/scan

# Scan with Monero detection enabled
./ceed_parser -m /path/to/scan

# Scan with multiple languages
./ceed_parser -l en,es /path/to/scan 

# Generate 10 addresses for each found seed
./ceed_parser -a 10 /path/to/scan
```

## Performance Optimizations

- Uses thread pools for parallelized file scanning
- Memory mapping for large files
- SIMD instruction sets when available
- Binary search for fast word lookups
- Low memory footprint with buffer reuse

## License

This project is licensed under the MIT License - see the LICENSE file for details.
