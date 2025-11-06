# NVFS (NVIDIA File System) Kernel Module - Developer Guide

## Overview

NVFS is a kernel module that provides high-performance file system operations optimized for NVIDIA GPUs. It enables direct memory mapping between GPU memory and storage devices, bypassing traditional CPU-based copy operations for improved performance in GPU-intensive workloads.

## Architecture Overview

### Core Components

The NVFS module is structured around several key subsystems:

```
nvfs-core.c/h         - Core module infrastructure and main API
nvfs-mmap.c/h         - Memory mapping and GPU memory management  
nvfs-dma.c/h          - DMA operations and memory transfers
nvfs-pci.c/h          - PCI device management and peer-to-peer
nvfs-batch.c/h        - Batch operations and I/O aggregation
nvfs-rdma.c/h         - RDMA integration for network storage
nvfs-fault.c/h        - Page fault handling for GPU memory
nvfs-stat.c/h         - Statistics and performance monitoring
nvfs-proc.c           - /proc interface for debugging
nvfs-mod.c            - Module dependency management
```

### Key Data Structures

#### Block States (`enum nvfs_block_state`)
- `NVFS_IO_FREE` - Initial state, block available for allocation
- `NVFS_IO_ALLOC` - Block allocated but not initialized
- `NVFS_IO_INIT` - Block initialized and ready for I/O
- `NVFS_IO_QUEUED` - Block queued for DMA operation
- `NVFS_IO_DMA_START` - DMA operation in progress
- `NVFS_IO_DONE` - I/O operation completed successfully
- `NVFS_IO_DMA_ERROR` - DMA operation failed

#### I/O States (`enum nvfs_io_state`)
- `IO_FREE` - I/O request not active
- `IO_INIT` - I/O request initialized
- `IO_READY` - Ready for processing
- `IO_IN_PROGRESS` - Currently being processed
- `IO_TERMINATE_REQ` - Termination requested
- `IO_TERMINATED` - I/O terminated
- `IO_CALLBACK_REQ` - Callback requested
- `IO_CALLBACK_END` - Callback completed

## Module Components

### 1. Core Module (`nvfs-core.c/h`)

**Purpose**: Central module management, device registration, and primary API

**Key Functions**:
- Module initialization and cleanup
- Character device registration (`/dev/nvidia-fs`)
- Core API for file system operations
- GPU memory region management

**Important Constants**:
```c
#define DEVICE_NAME "nvidia-fs"
#define CLASS_NAME "nvidia-fs-class"
```

### 2. Memory Mapping (`nvfs-mmap.c/h`)

**Purpose**: GPU memory mapping, page management, and sparse file handling

**Key Functions**:
- `nvfs_mgroup_mmap()` - Character device mmap implementation
- `nvfs_mgroup_check_and_set()` - Block state validation and transitions
- `nvfs_handle_sparse_read_region()` - Sparse region processing for reads
- `nvfs_handle_done_block_validation()` - I/O completion validation

**Memory Constants**:
```c
#define NVFS_BLOCK_SIZE    (4096)    // 4KB block size
#define NVFS_BLOCK_SHIFT   (12)      // log2(4096)
#define MAX_PCI_BUCKETS    32        // PCI device organization
```

**State Management**:
The module uses a sophisticated state machine to track memory blocks through their lifecycle. The `nvfs_mgroup_check_and_set()` function validates state transitions using a switch statement for optimal performance.

### 3. DMA Operations (`nvfs-dma.c/h`)

**Purpose**: Direct Memory Access operations between GPU and storage

**Key Features**:
- High-performance memory transfers
- Asynchronous I/O operations
- Error handling and recovery
- Memory pinning and unpinning

### 4. PCI Management (`nvfs-pci.c/h`)

**Purpose**: PCI device discovery and peer-to-peer memory access

**Key Features**:
- GPU device enumeration
- PCI BAR mapping
- Peer-to-peer capability detection
- Device compatibility validation

### 5. Batch Operations (`nvfs-batch.c/h`)

**Purpose**: I/O operation batching for improved performance

**Key Features**:
- Multiple I/O request aggregation
- Reduced system call overhead
- Optimized memory access patterns

### 6. Statistics (`nvfs-stat.c/h`)

**Purpose**: Performance monitoring and debugging

**Key Metrics**:
- I/O operation counts
- Memory allocation statistics
- Error rates and types
- Performance timings

## Development Guidelines

### Code Style

The module follows Linux kernel coding standards:
- **Indentation**: Use tabs (8 spaces)
- **Line Length**: 80 characters preferred, 100 maximum
- **Naming**: Snake_case for functions, UPPER_CASE for macros
- **Comments**: Use `/* */` for multi-line, `//` acceptable for single line

### Function Organization

Functions are organized by complexity and purpose:

1. **Static Helper Functions**: Small, single-purpose utilities
2. **State Management**: Functions handling state transitions
3. **Public APIs**: Module interface functions
4. **Error Handling**: Cleanup and error recovery

### Memory Management

**Critical Rules**:
- Always check return values from memory allocation
- Use proper cleanup paths with `goto` labels
- Pin GPU memory before DMA operations
- Unpin memory in reverse allocation order

**Example Pattern**:
```c
static int nvfs_operation(struct nvfs_data *data)
{
	int ret = 0;
	void *buffer = NULL;
	
	buffer = kmalloc(size, GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto cleanup;
	}
	
	ret = nvfs_process_data(buffer);
	if (ret)
		goto cleanup;
		
cleanup:
	kfree(buffer);
	return ret;
}
```

### Error Handling

Use kernel-standard error codes:
- `-ENOMEM` - Memory allocation failure
- `-EINVAL` - Invalid argument
- `-EIO` - I/O error
- `-EBUSY` - Resource busy
- `-EFAULT` - Memory access fault

### State Machine Management

The module uses state machines extensively. When adding new states:

1. **Define the state** in the appropriate enum
2. **Add case handling** in switch statements
3. **Update validation logic** in check functions
4. **Add state name** to debugging arrays

## API Reference

### Core Functions

#### `nvfs_mgroup_mmap(struct file *filp, struct vm_area_struct *vma)`
Maps NVFS memory regions to user space.

**Parameters**:
- `filp` - File pointer
- `vma` - Virtual memory area

**Returns**: 0 on success, negative error code on failure

#### `nvfs_mgroup_check_and_set(nvfs_mgroup_ptr_t nvfs_mgroup, enum nvfs_block_state state, bool validate, bool update_nvfsio)`
Validates and transitions block states.

**Parameters**:
- `nvfs_mgroup` - Memory group pointer
- `state` - Target state
- `validate` - Enable validation checks
- `update_nvfsio` - Update I/O metadata

### Helper Functions

#### `nvfs_handle_sparse_read_region(...)`
Processes sparse regions during read operations.

**Key Features**:
- Hole detection and mapping
- Sparse region consolidation
- Maximum region limit handling

#### `nvfs_handle_done_block_validation(...)`
Validates completed I/O operations.

**Key Features**:
- State validation
- Error detection
- Sparse/write operation handling

## Building and Testing

### Build Requirements

- Linux kernel headers (version 5.4+)
- GCC compiler
- Make utility
- Root privileges for testing

### Build Commands

```bash
# Build the module
make

# Install module (requires root)
sudo make install

# Load module
sudo modprobe nvfs

# Check module status
lsmod | grep nvfs
```

### Testing

The module includes comprehensive self-tests:

```bash
# Run all tests
cd tests/
sudo ./test_runner.sh

# Run specific test suite
sudo ./test_runner.sh core
sudo ./test_runner.sh stress

# View test results
dmesg | grep "NVFS_TEST"
```

### Debugging

#### Enable Debug Output

```bash
# Load with debug enabled
sudo modprobe nvfs debug=1

# View debug messages
dmesg -w | grep nvfs
```

#### Performance Monitoring

```bash
# Check statistics
cat /proc/nvfs/stats

# Monitor I/O operations
cat /proc/nvfs/io_stats
```

## Common Development Tasks

### Adding New Functionality

1. **Design the API**: Define function signatures and data structures
2. **Implement core logic**: Add the main functionality
3. **Add state management**: Update state machines if needed
4. **Include error handling**: Add proper cleanup and error paths
5. **Write tests**: Create unit tests for new functionality
6. **Update documentation**: Add to this guide and inline comments

### Debugging Issues

1. **Check kernel logs**: `dmesg | grep nvfs`
2. **Verify state transitions**: Use debug output to trace states
3. **Monitor memory usage**: Check for leaks with `kmemleak`
4. **Use kernel debugging**: GDB, KGDB, or ftrace
5. **Run self-tests**: Verify basic functionality

### Performance Optimization

1. **Profile hotpaths**: Use `perf` or `ftrace`
2. **Optimize memory access**: Minimize allocations in fast paths
3. **Batch operations**: Group related I/O operations
4. **Reduce lock contention**: Use lock-free algorithms where possible
5. **Cache frequently accessed data**: Implement intelligent caching

## Advanced Topics

### GPU Memory Management

NVFS manages GPU memory through several mechanisms:
- **Direct mapping**: GPU memory mapped directly to user space
- **Pinning**: GPU pages pinned during DMA operations  
- **Coherency**: Cache coherency management between CPU and GPU

### Sparse File Handling

The module supports sparse files with:
- **Hole detection**: Automatic identification of sparse regions
- **Efficient I/O**: Skip unnecessary transfers for holes
- **Metadata tracking**: Maintain sparse region information

### Error Recovery

Robust error handling includes:
- **State rollback**: Revert to known good states on errors
- **Resource cleanup**: Automatic cleanup of partial operations
- **Retry mechanisms**: Intelligent retry for transient failures

## Contributing Guidelines

### Code Review Process

1. **Self-review**: Check your code against coding standards
2. **Run tests**: Ensure all tests pass
3. **Check style**: Use `scripts/checkpatch.pl` to validate style
4. **Document changes**: Update relevant documentation
5. **Submit patch**: Follow kernel patch submission guidelines

### Coding Standards Checklist

- [ ] Code follows Linux kernel style (verified with checkpatch)
- [ ] Functions are properly documented
- [ ] Error paths are tested
- [ ] Memory leaks are prevented
- [ ] State transitions are validated
- [ ] Tests are included for new functionality

### Performance Considerations

- **Hot paths**: Minimize work in performance-critical sections
- **Memory allocation**: Prefer stack allocation for small objects
- **Lock granularity**: Use fine-grained locking where possible
- **Cache efficiency**: Structure data for optimal cache usage

## Troubleshooting Guide

### Common Issues

1. **Module Load Failures**
   - **Cause**: Missing dependencies or version mismatch
   - **Solution**: Check kernel version and install proper headers

2. **Memory Allocation Failures**
   - **Cause**: Insufficient memory or fragmentation
   - **Solution**: Implement proper fallback mechanisms

3. **DMA Errors**
   - **Cause**: Invalid memory regions or hardware issues
   - **Solution**: Validate memory before DMA operations

4. **State Machine Violations**
   - **Cause**: Race conditions or incorrect state transitions
   - **Solution**: Add proper locking and validation

### Debug Techniques

1. **Kernel Debugging**:
   ```bash
   echo 'module nvfs +p' > /sys/kernel/debug/dynamic_debug/control
   ```

2. **Memory Debugging**:
   ```bash
   echo 1 > /sys/kernel/debug/kmemleak
   cat /sys/kernel/debug/kmemleak
   ```

3. **Performance Analysis**:
   ```bash
   perf record -g sudo modprobe nvfs
   perf report
   ```

## Future Development

### Planned Enhancements

- **Extended GPU support**: Additional GPU architectures
- **Network storage**: Enhanced RDMA capabilities  
- **Performance optimization**: Further reduction in latency
- **Debugging tools**: Enhanced debugging and profiling support

### Research Areas

- **Zero-copy I/O**: Eliminate all memory copies
- **Asynchronous operations**: Full async I/O pipeline
- **Machine learning integration**: Optimizations for ML workloads
- **Container support**: Enhanced containerization features

---

This developer guide provides the foundation for understanding, building, and extending the NVFS kernel module. For specific implementation details, refer to the inline code documentation and test suites.

**Happy coding! =€**