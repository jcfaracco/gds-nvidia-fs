# NVFS Kernel Module Test Suite

This directory contains comprehensive tests for the NVFS (NVIDIA File System) kernel module, organized into modern KUnit tests and kernel selftests.

## Directory Structure

```
src/tests/
├── kunit/                    # Modern KUnit-based tests
│   ├── nvfs_core_kunit.c     # Core functionality KUnit tests  
│   ├── nvfs_stress_kunit.c   # Stress testing KUnit tests
│   ├── Kconfig               # KUnit test configuration
│   ├── Makefile              # KUnit build system
│   └── .kunitconfig          # KUnit configuration file
├── selftests/                # Kernel selftests (debugfs-based)
│   ├── nvfs_test_framework.c # Selftest framework
│   ├── nvfs_core_tests.c     # Core selftests
│   ├── nvfs_stress_tests.c   # Stress selftests
│   ├── nvfs_stub_tests.c     # Stub selftests
│   ├── nvfs_test.h           # Selftest headers
│   └── Makefile              # Selftest build system
├── scripts/                  # Test automation scripts
│   └── test_runner.sh        # Selftest runner script
├── Makefile                  # Main build system
└── README.md                 # This file
```

## Test Types

### 🧪 KUnit Tests (Recommended)

Modern kernel unit testing framework providing:

- **Isolated unit tests** that run in kernel space
- **Structured test organization** with test suites and cases
- **Rich assertion framework** (KUNIT_EXPECT_*, KUNIT_ASSERT_*)
- **Integration with kernel testing tools**
- **Better error reporting and debugging**

**Test Coverage:**
- Core functionality (folio allocation, page mapping, reference counting)
- Stress testing (memory pressure, rapid cycles, edge cases)
- Memory management validation
- Error handling scenarios

### 🔧 Kernel Selftests

Debugfs-based testing framework:

- **Runtime testing** via debugfs interface
- **Live system validation** 
- **Manual test execution** control
- **Integrated with kernel selftest infrastructure**

## Quick Start

### Running KUnit Tests

```bash
# Build KUnit tests
make kunit

# Run with KUnit tool (recommended)
kunit.py run --kunitconfig=src/tests/kunit/

# Or build into kernel and run
CONFIG_NVFS_KUNIT_TEST=y make && boot
```

### Running Selftests

```bash
# Build and run selftests
make test-selftests

# Or manually
make selftests
echo 'all' > /sys/kernel/debug/nvfs_test/run_tests
```

### Build All Tests

```bash
# Build both KUnit and selftests
make all

# Clean all tests
make clean

# Show help
make help
```

## KUnit Test Configuration

Configure KUnit tests via kernel config:

```kconfig
CONFIG_KUNIT=y                      # Enable KUnit framework
CONFIG_NVFS_KUNIT_TEST=y            # Enable NVFS KUnit tests
CONFIG_NVFS_KUNIT_TEST_CORE=y       # Core functionality tests
CONFIG_NVFS_KUNIT_TEST_STRESS=y     # Stress and edge case tests
```

## Test Categories

### Core Functionality Tests
- **Folio allocation/deallocation** (single and multi-page)
- **Page-to-folio conversion** validation
- **kmap_local_page/kunmap_local** functionality
- **Reference counting** correctness
- **Memory allocation failure** handling

### Stress Tests
- **Memory pressure simulation** with high-order allocations
- **Rapid allocation/deallocation cycles**
- **Concurrent kmap operations** testing
- **Edge cases** (maximum order allocations, boundary conditions)
- **Reference counting under stress**

### Integration Tests
- **NVFS-specific operations** validation
- **GPU memory interaction** testing (when applicable)
- **File system integration** scenarios

## Usage Examples

### KUnit Testing Workflow

```bash
# Development workflow
cd src/tests
make kunit
kunit.py run --kunitconfig=kunit/

# CI/CD integration
kunit.py run --kunitconfig=kunit/ --json > test_results.json

# Running specific test suites
kunit.py run --kunitconfig=kunit/ nvfs_core
kunit.py run --kunitconfig=kunit/ nvfs_stress
```

### Selftest Workflow

```bash
# Quick validation
make test-selftests

# Manual control
make selftests
sudo insmod selftests/nvfs_selftest.ko
echo 'core' > /sys/kernel/debug/nvfs_test/run_tests
dmesg | tail -20
```

## Development Guidelines

### Adding New KUnit Tests

1. **Create test function:**
   ```c
   static void nvfs_new_feature_test(struct kunit *test)
   {
       KUNIT_ASSERT_NOT_NULL(test, some_function());
       KUNIT_EXPECT_EQ(test, expected, actual);
   }
   ```

2. **Add to test suite:**
   ```c
   static struct kunit_case nvfs_core_test_cases[] = {
       KUNIT_CASE(nvfs_new_feature_test),
       // ... other tests
       {},
   };
   ```

3. **Update configuration** in `kunit/Kconfig` if needed

### KUnit Best Practices

- **Use KUNIT_ASSERT_*** for critical conditions that should stop the test
- **Use KUNIT_EXPECT_*** for conditions you want to check but continue testing
- **Keep tests focused** - one concept per test function
- **Use descriptive test names** that explain what is being tested
- **Clean up resources** in test functions (folios, mappings, etc.)

## Debugging Tests

### KUnit Test Debugging

```bash
# Run with verbose output
kunit.py run --kunitconfig=kunit/ --raw_output

# Debug specific test
kunit.py run --kunitconfig=kunit/ --filter="nvfs_core.*allocation"

# Build for debugging
echo "CONFIG_DEBUG_KERNEL=y" >> kunit/.kunitconfig
```

### Selftest Debugging

```bash
# Monitor test execution
dmesg -w &
echo 'all' > /sys/kernel/debug/nvfs_test/run_tests

# Check test status
cat /sys/kernel/debug/nvfs_test/run_tests
```

## Contributing

When adding new tests:

1. **Prefer KUnit tests** for new functionality
2. **Maintain selftests** for runtime validation scenarios
3. **Update documentation** when adding new test categories
4. **Follow kernel coding style** (use checkpatch.pl)
5. **Test both success and failure paths**

## KUnit vs Selftests

The test suite supports both frameworks for different use cases:

- **KUnit tests**: Ideal for unit testing, CI/CD integration, and development workflows
- **Selftests**: Best for runtime validation, live system testing, and manual verification

**KUnit advantages:**
- Better integration with kernel testing infrastructure
- Improved debugging and error reporting  
- Structured test organization
- Automated test discovery and execution