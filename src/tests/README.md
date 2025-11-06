# NVFS Kernel Module Self-Tests

This directory contains comprehensive self-tests for the NVFS (NVIDIA File System) kernel module. The test suite validates core functionality, memory management, and stress testing scenarios.

## Test Architecture

### Test Framework Components

1. **Test Framework** (`nvfs_test_framework.c`)
   - Core test execution engine
   - Debugfs interface for test control
   - Result reporting and statistics
   - Test suite management

2. **Test Suites**
   - **Core Tests** (`nvfs_core_tests.c`) - Basic functionality validation
   - **Stress Tests** (`nvfs_stress_tests.c`) - Intensive testing and edge cases
   - **Stub Tests** (`nvfs_stub_tests.c`) - Placeholder tests for future features

3. **Test Infrastructure**
   - **Makefile** - Build system for test modules
   - **test_runner.sh** - Automated test execution script
   - **nvfs_test.h** - Test framework headers and macros

## Features

### Core Functionality Tests
- Folio allocation and deallocation (single and multi-page)
- Page-to-folio conversion validation
- kmap_local_page/kunmap_local functionality
- Reference counting verification
- Memory allocation failure handling

### Stress and Edge Case Tests
- High-volume folio allocation/deallocation cycles
- Concurrent memory mapping operations
- Maximum order allocation attempts
- Rapid allocation/deallocation cycles
- Memory pressure simulation
- Reference counting stress testing

### Test Framework Features
- **Debugfs Interface**: `/sys/kernel/debug/nvfs_test/run_tests`
- **Multiple Execution Modes**: Individual test suites or comprehensive testing
- **Real-time Results**: Statistics and pass/fail reporting
- **Kernel Log Integration**: Detailed test output in dmesg
- **Automated Cleanup**: Module loading/unloading management

## Usage

### Quick Start

```bash
# Run all tests (requires root privileges)
sudo ./test_runner.sh

# Run specific test suite
sudo ./test_runner.sh core
sudo ./test_runner.sh stress

# Build tests only
sudo ./test_runner.sh --build

# Clean build artifacts
sudo ./test_runner.sh --clean
```

### Manual Test Execution

```bash
# Build test module
cd tests/
make

# Load test module
sudo insmod nvfs_selftest.ko

# Run tests via debugfs
echo 'all' | sudo tee /sys/kernel/debug/nvfs_test/run_tests
echo 'core' | sudo tee /sys/kernel/debug/nvfs_test/run_tests
echo 'stress' | sudo tee /sys/kernel/debug/nvfs_test/run_tests

# View results
cat /sys/kernel/debug/nvfs_test/run_tests

# Check detailed output
dmesg | grep "NVFS_TEST:"

# Unload module
sudo rmmod nvfs_selftest
```

### Available Test Suites

- `all` - Execute all test suites
- `core` - Core functionality tests
- `mmap` - Memory mapping tests (stub)
- `dma` - DMA functionality tests (stub)
- `memory` - Memory management tests (stub)
- `stress` - Stress and edge case tests

## Test Results

### Output Format
```
NVFS Self-Test Results
======================
Total tests: 15
Passed: 14
Failed: 0
Skipped: 1
Success rate: 93%
Elapsed time: 234 jiffies (58 ms)
```

### Result Interpretation
- **PASS**: Test completed successfully
- **FAIL**: Test failed with detailed error information
- **SKIP**: Test was skipped (e.g., due to missing dependencies)

## Development

### Adding New Tests

1. **Create Test Function**:
```c
NVFS_TEST_DECLARE(my_new_test)
{
    // Test implementation
    NVFS_TEST_ASSERT(condition, "Error message");
    return NVFS_TEST_PASS;
}
```

2. **Add to Test Suite**:
```c
static struct nvfs_test_case my_tests[] = {
    NVFS_TEST_CASE(my_new_test, "Description of test"),
    // ... other tests
};
```

3. **Register Test Suite**:
```c
struct nvfs_test_suite my_test_suite = {
    .name = "My Test Suite",
    .tests = my_tests,
    .num_tests = ARRAY_SIZE(my_tests),
    .setup = my_setup_function,
    .teardown = my_teardown_function,
};
```

### Test Assertions

Available assertion macros:
- `NVFS_TEST_ASSERT(condition, msg)` - General condition check
- `NVFS_TEST_ASSERT_EQ(expected, actual, msg)` - Equality check
- `NVFS_TEST_ASSERT_NULL(ptr, msg)` - Null pointer check
- `NVFS_TEST_ASSERT_NOT_NULL(ptr, msg)` - Non-null pointer check

## Integration with CI/CD

### GitHub Actions Integration

The test suite integrates with the existing checkpatch workflow and can be extended for automated testing:

```yaml
- name: Run NVFS Self-Tests
  run: |
    cd tests/
    sudo ./test_runner.sh all
    # Check for test failures
    if dmesg | grep -q "NVFS_TEST.*FAIL"; then
      echo "Tests failed"
      exit 1
    fi
```

### Requirements

- Linux kernel development headers
- Root privileges for module operations
- debugfs support
- Modern kernel with folio API support (5.16+)

## Troubleshooting

### Common Issues

1. **Module Load Failures**
   - Ensure kernel headers are installed
   - Check for symbol conflicts with main NVFS module
   - Verify kernel version compatibility

2. **Test Execution Failures**
   - Confirm debugfs is mounted: `mount -t debugfs none /sys/kernel/debug`
   - Check dmesg for detailed error messages
   - Ensure sufficient memory for stress tests

3. **Build Issues**
   - Install kernel development packages: `dnf install kernel-devel`
   - Verify KDIR path in Makefile
   - Check for missing include files

### Debug Information

Enable verbose output:
```bash
# View detailed kernel messages
dmesg -w | grep NVFS_TEST

# Check module status
lsmod | grep nvfs

# Verify debugfs entries
ls -la /sys/kernel/debug/nvfs_test/
```

## Future Enhancements

- Integration with actual NVFS module testing
- Performance benchmarking capabilities
- Automated regression testing
- Extended DMA and MMAP test coverage
- GPU memory interaction testing
- Continuous integration pipeline integration

For questions or contributions, please refer to the main NVFS project documentation.