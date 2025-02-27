# Porting CEED Parser to Rust

This document outlines the strategy and considerations for porting the CEED Parser from C to Rust.

## Why Rust?

- **Memory Safety**: Rust's ownership model eliminates entire classes of memory errors at compile time
- **Thread Safety**: Fearless concurrency with compile-time guarantees
- **Zero-Cost Abstractions**: High-level abstractions with C-like performance
- **Modern Tooling**: Cargo for dependency management, rustfmt for formatting, clippy for linting
- **Cross-Platform**: Excellent support for cross-compilation

## Migration Strategy

### 1. Project Structure

```
ceed_parser_rs/
├── Cargo.toml              # Package manifest (equivalent to CMakeLists.txt)
├── src/
│   ├── main.rs             # Entry point
│   ├── lib.rs              # Library exports
│   ├── parser.rs           # Seed parser implementation
│   ├── mnemonic.rs         # Mnemonic handling
│   ├── wallet.rs           # Wallet functionality
│   ├── memory.rs           # Memory management (if needed)
│   └── logger.rs           # Logging functionality
├── benches/                # Benchmarks using criterion
├── examples/               # Example usage
└── tests/                  # Integration tests
```

### 2. Equivalent Libraries

| C Library/Feature        | Rust Equivalent                             | Notes                                    |
| ------------------------ | ------------------------------------------- | ---------------------------------------- |
| Custom memory pool       | `std::alloc` or just use Rust's ownership | Likely unnecessary in Rust               |
| OpenSSL                  | `ring`, `rustls`, or `rust-openssl`   | `ring` is pure Rust and recommended    |
| Custom logger            | `log` + `env_logger` or `tracing`     | `tracing` for advanced use cases       |
| Unity test framework     | Built-in `#[test]` or `proptest`        | Rust has testing built into the language |
| Manual error handling    | `Result<T, E>` and `?` operator         | Leverage Rust's error handling           |
| Manual string handling   | `String` and `&str`                     | Safer string handling by default         |
| Manual memory management | Ownership and RAII                          | Let Rust handle memory for you           |

### 3. Implementation Approach

1. **Incremental Migration**:

   - Start with core functionality (parser, mnemonic)
   - Write tests for each module before implementation
   - Implement FFI bindings to call Rust from C during transition
2. **Idiomatic Rust**:

   - Use enums for error handling instead of error codes
   - Leverage traits for abstraction
   - Use iterators instead of manual loops where appropriate
   - Prefer immutable data when possible
3. **Performance Considerations**:

   - Use `#[inline]` for critical path functions
   - Consider `no_std` for embedded contexts if needed
   - Profile with `criterion` benchmarks
   - Use `unsafe` only when necessary and document thoroughly

### 4. Specific Component Migration

#### Parser Module

```rust
// Example of how the parser might look in Rust
pub struct ParserConfig {
    wordlist_path: String,
    max_words: usize,
    // other config options
}

pub struct Parser {
    config: ParserConfig,
    wordlist: Vec<String>,
}

impl Parser {
    pub fn new(config: ParserConfig) -> Result<Self, ParserError> {
        // Initialize parser
        let wordlist = Self::load_wordlist(&config.wordlist_path)?;
        Ok(Self { config, wordlist })
    }
  
    pub fn parse(&self, input: &str) -> Result<Vec<String>, ParserError> {
        // Implementation
    }
  
    fn load_wordlist(path: &str) -> Result<Vec<String>, ParserError> {
        // Implementation
    }
}

#[derive(Debug, thiserror::Error)]
pub enum ParserError {
    #[error("Failed to open wordlist: {0}")]
    WordlistError(#[from] std::io::Error),
    #[error("Invalid input: {0}")]
    InvalidInput(String),
    // Other error variants
}
```

#### Memory Management

- Replace custom memory pool with Rust's ownership model
- Use `Box<T>` for heap allocation
- Use `Arc<T>` for shared ownership
- Consider `Vec<T>` instead of manual dynamic arrays

#### Logging

```rust
// Using the log crate
use log::{debug, error, info, trace, warn};

pub fn initialize_logging(level: log::LevelFilter) {
    env_logger::Builder::new()
        .filter_level(level)
        .format_timestamp_millis()
        .init();
}

// Usage
fn some_function() -> Result<(), Error> {
    debug!("Starting operation");
    if let Err(e) = risky_operation() {
        error!("Operation failed: {}", e);
        return Err(e.into());
    }
    info!("Operation completed successfully");
    Ok(())
}
```

### 5. Testing Strategy

1. **Unit Tests**: Write tests for each function/method

   ```rust
   #[cfg(test)]
   mod tests {
       use super::*;

       #[test]
       fn test_parser_initialization() {
           let config = ParserConfig {
               wordlist_path: "data/wordlist/english.txt".to_string(),
               max_words: 24,
           };
           let parser = Parser::new(config).expect("Failed to create parser");
           assert_eq!(parser.wordlist.len(), 2048);
       }
   }
   ```
2. **Property-Based Testing**: Use `proptest` for generating test cases

   ```rust
   proptest! {
       #[test]
       fn doesnt_crash_on_random_input(s in "\\PC*") {
           let _ = parse_mnemonic(&s);
       }
   }
   ```
3. **Fuzzing**: Use `cargo-fuzz` to find edge cases

### 6. Performance Benchmarking

Use `criterion` for benchmarking:

```rust
use criterion::{black_box, criterion_group, criterion_main, Criterion};

fn bench_parser(c: &Criterion) {
    let config = ParserConfig::default();
    let parser = Parser::new(config).unwrap();
  
    c.bench_function("parse_standard_mnemonic", |b| {
        b.iter(|| parser.parse(black_box("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about")))
    });
}

criterion_group!(benches, bench_parser);
criterion_main!(benches);
```

### 7. FFI Considerations

If you need to maintain C compatibility during transition:

```rust
// In lib.rs
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};

#[no_mangle]
pub extern "C" fn parse_seed(input: *const c_char) -> *mut c_char {
    let c_str = unsafe {
        if input.is_null() {
            return std::ptr::null_mut();
        }
        CStr::from_ptr(input)
    };
  
    let input_str = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
  
    // Call Rust implementation
    let result = match parse_mnemonic(input_str) {
        Ok(r) => r,
        Err(_) => return std::ptr::null_mut(),
    };
  
    // Convert back to C string
    let output = match CString::new(result) {
        Ok(s) => s,
        Err(_) => return std::ptr::null_mut(),
    };
  
    output.into_raw()
}

#[no_mangle]
pub extern "C" fn free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}
```

## Security Considerations

- Use `secrecy` crate for sensitive data
- Consider `zeroize` for secure memory clearing
- Use constant-time comparison for cryptographic operations
- Audit all `unsafe` code blocks thoroughly

## Resources

- [The Rust Book](https://doc.rust-lang.org/book/)
- [Rust by Example](https://doc.rust-lang.org/rust-by-example/)
- [Rustonomicon](https://doc.rust-lang.org/nomicon/) (for unsafe Rust)
- [Rust API Guidelines](https://rust-lang.github.io/api-guidelines/)
- [Rust Design Patterns](https://rust-unofficial.github.io/patterns/)

## Timeline Estimation

1. **Setup and Initial Structure**: 1-2 days
2. **Core Functionality (Parser, Mnemonic)**: 1-2 weeks
3. **Supporting Modules (Logging, etc.)**: 1 week
4. **Testing and Benchmarking**: 1-2 weeks
5. **Documentation and Cleanup**: 1 week

Total estimated time: 4-6 weeks depending on complexity and familiarity with Rust.
