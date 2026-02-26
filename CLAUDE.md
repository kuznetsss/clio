# Clio Development Guide

This document contains important information for developers working on the Clio project.

## Building the Project

### Build System

Clio uses CMake with Ninja as the build generator.

### Building Tests

To build the test suite:

```bash
ninja -C build -j8 clio_tests
```

Options:
- `-C build` - Specify the build directory
- `-j8` - Use 8 parallel jobs (adjust based on your CPU cores)
- `clio_tests` - The main test executable target

## Running Tests

### Run All Tests

```bash
build/clio_tests
```

### Run Specific Test Suites

Use Google Test filters to run specific tests:

```bash
# Run all RPC counter tests
build/clio_tests --gtest_filter="RPCCounters*"

# Run a specific test
build/clio_tests --gtest_filter="RPCCountersTest.CheckThatCountersAddUp"

# Run tests matching a pattern
build/clio_tests --gtest_filter="*LedgerRequest*"
```

### Useful GTest Flags

- `--gtest_filter=<pattern>` - Run tests matching the pattern (supports wildcards)
- `--gtest_list_tests` - List all available tests without running them
- `--gtest_repeat=<count>` - Repeat tests multiple times
- `--gtest_shuffle` - Run tests in random order
- `--gtest_brief=1` - Use brief output format

## Project Structure

### Key Directories

- `src/` - Main source code
  - `src/rpc/` - RPC handlers and related code
  - `src/rpc/handlers/` - Individual RPC method handlers
- `tests/` - Test suite
  - `tests/unit/` - Unit tests
  - `tests/unit/rpc/` - RPC-related unit tests
- `build/` - Build output directory (not in version control)

## Development Workflow

### Making Changes

1. Make your code changes
2. Build the affected components: `ninja -C build -j8 clio_tests`
3. Run relevant tests: `build/clio_tests --gtest_filter="<pattern>"`
4. Ensure all tests pass before committing

### Adding New Tests

When adding new functionality:
1. Write tests in the appropriate `tests/unit/` subdirectory
2. Follow existing test patterns (use Google Test framework)
3. For Prometheus metrics, use `WithMockPrometheus` and mock expectations
4. Run the new tests to verify they pass

## Common Tasks

### Testing RPC Changes

When modifying RPC handlers or counters:
```bash
# Build
ninja -C build -j8 clio_tests

# Run RPC tests
build/clio_tests --gtest_filter="RPC*"
```

### Testing Specific Components

```bash
# Build only what's needed
ninja -C build -j8 clio_tests

# Run component-specific tests
build/clio_tests --gtest_filter="ComponentName*"
```
