# Box Compiler

Box is an educational compiler experimental statically-typed compiled programming language with LLVM-based code generation. This project is currently in development and is not ready for production use.

## Project Status

**This compiler is a work in progress and has significant limitations:**
- this is compiler for educational
- no standard library
- Basic optimization support
- Active development with breaking changes

## Overview

Box compiler (`boxc`) is an educational compiler project featuring:

- LLVM-based code generation
- Static memory analysis
- Basic optimization pipeline

## System Requirements

### Supported Platforms

- Linux (Ubuntu 20.04+, Debian 11+, Fedora 35+, Arch Linux)
- macOS (10.15+)

### Build Dependencies

- CMake 3.10+
- C++17 compatible compiler (GCC 9+ or Clang 10+)
- LLVM 14+ (recommended: LLVM 17)
- GNU Make
- ncurses development libraries
- zlib development libraries

## Installation

### Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/mohammadamin382/master/BoxcLang/setup.sh | bash
```

```bash
boxc --version
```

## Quick Start

### Hello World

```box
print "Hello, World!";
```

Compile and run:
```bash
boxc hello.box
./hello
```

### With Optimization

```bash
boxc -O3 -Oasm3 hello.box -o hello_optimized
./hello_optimized
```

## Command Line Interface

### Synopsis

```
boxc [options] <input-file>
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output <file>` | Specify output executable name |
| `--emit-llvm` | Generate LLVM IR (.ll file) |
| `-S` | Generate assembly (.s file) |
| `-r, --run` | Execute compiled program immediately |
| `--no-optimize` | Disable all optimizations |
| `-O<0-3>` | IR optimization level (default: 3) |
| `-Oasm<0-3>` | LLVM backend optimization level (default: 3) |
| `--no-warnings` | Suppress memory safety warnings |
| `-v, --verbose` | Enable verbose compilation output |
| `--version` | Display compiler version |
| `-h, --help` | Show help message |

### Optimization Levels

#### IR Optimization (`-O`)

- `-O0`: No optimization
- `-O1`: Basic optimizations
- `-O2`: Standard optimizations
- `-O3`: Maximum optimization

#### LLVM Optimization (`-Oasm`)

- `-Oasm0`: No LLVM optimization
- `-Oasm1`: LLVM basic optimization
- `-Oasm2`: LLVM default optimization
- `-Oasm3`: LLVM aggressive optimization

### Examples

```bash
# Basic compilation
boxc program.box

# Custom output name
boxc -o myapp program.box

# Generate LLVM IR
boxc --emit-llvm program.box

# Generate assembly
boxc -S program.box

# Compile and run
boxc -r program.box

# Maximum optimization
boxc -O3 -Oasm3 program.box

# Debug build
boxc -O0 -Oasm0 program.box
```

## Project Structure

```
Box/
├── src/
│   ├── compiler/       # Compiler driver
│   ├── lexer/          # Lexical analysis
│   ├── parser/         # Syntax analysis
│   ├── memory_analyzer/ # Memory safety analysis
│   ├── optimizer/      # IR optimization passes
│   └── codegen/        # LLVM code generation
├── tests/              # Test suites
├── CMakeLists.txt      # Build configuration
└── setup.sh            # Installation script
```


## Language Features

- Static typing with type inference
- Arrays and dictionaries(limited)
- Memory management primitives (malloc, free, calloc, realloc)
- Pointer operations (addr_of, deref)
- LLVM inline
- Switch statements

## Known Limitations

- No garbage collection
- No standard library
- No module system beyond basic imports
- No generics or templates
- No multithreading support
- Basic error messages
- Limited optimization compared to established compilers

## Build From Source

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install cmake g++ llvm-17 llvm-17-dev make libncurses-dev zlib1g-dev
```

## License

This project is released under GPL-3.0 License. See LICENSE file for details.

## Support

For issues or questions, please use the GitHub issue tracker.

## Acknowledgments

Box compiler uses LLVM infrastructure and CMake for build system management.
