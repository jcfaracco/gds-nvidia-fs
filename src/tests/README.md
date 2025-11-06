# NVFS Kernel Module Test Suite

This directory contains comprehensive tests for the NVFS (NVIDIA File System) kernel module. The test suite is organized to eliminate duplication and provide clear separation between unit testing and integration testing.

## Directory Structure

```
src/tests/
├── kunit/                    # Unit tests (KUnit framework)
│   ├── nvfs_core_kunit.c     # Core function unit tests
│   ├── nvfs_stress_kunit.c   # Performance and edge case tests
│   ├── Kconfig               # KUnit test configuration
│   └── Makefile              # KUnit build system
├── selftests/                # Integration tests (debugfs-based)
│   ├── nvfs_test_framework.c # Selftest framework
│   ├── nvfs_core_tests.c     # NVFS integration tests
│   ├── nvfs_stress_tests.c   # System stress tests
│   ├── nvfs_stub_tests.c     # Stub functionality tests
│   ├── nvfs_test.h           # Selftest headers
│   └── Makefile              # Selftest build system
├── scripts/                  # Test automation scripts
│   └── test_runner.sh        # Selftest runner script
├── Makefile                  # Main build system
└── README.md                 # This file
```

## Test Framework Purposes

### 🧪 KUnit Tests - Unit Testing
**Purpose**: Test individual functions and components in isolation

**Focus Areas**:
- **Memory allocation parameter validation** - Test calculation correctness
- **Folio/page pointer arithmetic** - Test conversion functions
- **Reference counting logic** - Test increment/decrement behavior  
- **Address alignment checks** - Test memory alignment requirements
- **Performance characteristics** - Test allocation speed and boundaries
- **Edge case handling** - Test boundary conditions and error paths

**Characteristics**:
- Fast, isolated tests with minimal dependencies
- No NVFS module integration required
- Focus on correctness of individual functions
- Mocked or minimal external dependencies

### 🔧 Selftests - Integration Testing  
**Purpose**: Test NVFS functionality with the real module loaded

**Focus Areas**:
- **NVFS module integration** - Test with actual NVFS module loaded
- **GPU memory operations** - Test real GPU page detection and handling
- **File operations integration** - Test NVFS file system hooks
- **System stress scenarios** - Test under concurrent access and memory pressure
- **Real-world performance** - Test sustained load and fragmentation handling
- **End-to-end workflows** - Test complete NVFS usage scenarios

**Characteristics**:
- Requires NVFS module to be loaded
- Tests real system integration
- Handles actual GPU memory and file operations
- Tests system behavior under stress

## Quick Start

### Running KUnit Tests (Unit Tests)

```bash
# Build and run unit tests
cd src/tests
CONFIG_NVFS_KUNIT_TEST_CORE=m CONFIG_NVFS_KUNIT_TEST_STRESS=m make kunit

# Run with KUnit tool (if available)
kunit.py run --kunitconfig=src/tests/kunit/

# Or manually load modules
sudo insmod kunit/nvfs_core_kunit.ko
sudo insmod kunit/nvfs_stress_kunit.ko
```

### Running Selftests (Integration Tests)

```bash
# Build and run integration tests (requires NVFS module loaded)
cd src/tests
make selftests
sudo insmod selftests/nvfs_selftest.ko

# Run all integration tests
echo 'all' > /sys/kernel/debug/nvfs_test/run_tests

# Run specific test suites
echo 'core' > /sys/kernel/debug/nvfs_test/run_tests
echo 'stress' > /sys/kernel/debug/nvfs_test/run_tests
```

### Build All Tests

```bash
# Build both unit and integration tests
make all

# Clean all tests
make clean

# Show help
make help
```

## Test Categories by Framework

### KUnit Unit Tests

**Core Unit Tests** (`nvfs_core_kunit.c`):
- `nvfs_allocation_params_test` - Parameter validation logic
- `nvfs_folio_page_arithmetic_test` - Pointer arithmetic correctness
- `nvfs_refcount_test` - Reference counting behavior
- `nvfs_alignment_test` - Memory alignment validation
- `nvfs_folio_flags_test` - Flag operation correctness

**Performance Unit Tests** (`nvfs_stress_kunit.c`):
- `nvfs_allocation_performance_test` - Allocation speed measurement
- `nvfs_max_order_edge_test` - Maximum allocation boundary testing
- `nvfs_zero_order_edge_test` - Minimum allocation boundary testing
- `nvfs_refcount_stress_test` - Reference counting under load
- `nvfs_page_index_edge_test` - Page indexing boundary testing

### Selftests Integration Tests

**Integration Tests** (`nvfs_core_tests.c`):
- `nvfs_module_integration` - NVFS module loading and basic functionality
- `nvfs_file_operations_integration` - File system operation hooks
- `nvfs_gpu_memory_integration` - GPU memory detection and handling
- `nvfs_memory_pressure_integration` - Behavior under memory pressure
- `nvfs_performance_integration` - Real-world performance characteristics

**System Stress Tests** (`nvfs_stress_tests.c`):
- `nvfs_concurrent_stress` - Multi-threaded access testing
- `nvfs_memory_fragmentation_stress` - Fragmented memory handling
- `nvfs_sustained_load_stress` - Long-duration load testing
- `nvfs_extreme_conditions_stress` - Error handling under extreme conditions

## Configuration

### KUnit Test Configuration

```kconfig
CONFIG_KUNIT=y                      # Enable KUnit framework
CONFIG_NVFS_KUNIT_TEST_CORE=m       # Core unit tests
CONFIG_NVFS_KUNIT_TEST_STRESS=m     # Stress/performance unit tests
```

### Selftests Configuration

Selftests require the NVFS module to be loaded:
```bash
# Load NVFS module first
sudo insmod ../nvidia-fs.ko

# Then load and run selftests  
sudo insmod selftests/nvfs_selftest.ko
echo 'all' > /sys/kernel/debug/nvfs_test/run_tests
```

## Development Guidelines

### When to Use Each Framework

**Use KUnit for**:
- Testing individual functions in isolation
- Validating parameter processing and calculations
- Testing error handling logic
- Performance benchmarking of isolated operations
- Boundary condition testing

**Use Selftests for**:
- Testing NVFS module integration
- Validating real GPU memory operations  
- Testing file system integration
- System-level stress testing
- End-to-end workflow validation

### Adding New Tests

**Adding KUnit Tests**:
```c
static void nvfs_new_function_test(struct kunit *test)
{
    // Test isolated function behavior
    KUNIT_EXPECT_EQ(test, expected, nvfs_function(input));
    KUNIT_ASSERT_NOT_NULL(test, result_pointer);
}
```

**Adding Selftests**:
```c
NVFS_TEST_DECLARE(nvfs_new_integration_test)
{
    // Test with real NVFS module
    if (!nvfs_count_ops) {
        return NVFS_TEST_SKIP("NVFS not loaded");
    }
    
    // Test real integration scenario
    return NVFS_TEST_PASS;
}
```

## Best Practices

### Unit Testing (KUnit)
- Keep tests isolated and independent
- Mock external dependencies when possible
- Test both success and failure paths
- Use descriptive test function names
- Clean up all allocated resources

### Integration Testing (Selftests)
- Verify NVFS module is loaded before testing
- Test realistic usage scenarios
- Include proper error handling
- Test under various system conditions
- Provide informative logging

## Debugging

### KUnit Test Debugging
```bash
# Verbose output
kunit.py run --kunitconfig=kunit/ --raw_output

# Filter specific tests
kunit.py run --filter="nvfs_core.*arithmetic"

# Check kernel logs
dmesg | grep -i kunit
```

### Selftests Debugging
```bash
# Monitor test execution
dmesg -w &
echo 'all' > /sys/kernel/debug/nvfs_test/run_tests

# Check specific test results
cat /sys/kernel/debug/nvfs_test/run_tests
```

## Migration from Old Structure

This restructured test suite eliminates the previous duplication where both frameworks tested the same functionality:

**Old Structure Problems**:
- KUnit and selftests had identical folio allocation tests
- Stress tests duplicated between frameworks
- Maintenance overhead of duplicate test logic

**New Structure Benefits**:
- Clear separation of concerns (unit vs integration)
- No duplicate test logic
- Each framework optimized for its purpose
- Easier maintenance and development

The new structure provides comprehensive testing coverage while eliminating redundancy and clearly defining the role of each test framework.