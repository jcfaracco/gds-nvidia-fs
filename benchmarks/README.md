# GDS Benchmark Suite

Comprehensive benchmarking tools for testing GPUDirect Storage (GDS) performance with the nouveau-based GDS driver.

## Overview

This benchmark suite is based on NVIDIA's cuFile tools (gdscheck, gdsio, gds_perf) but adapted for testing the open-source GDS driver. It provides:

- **System verification** - Check if your system is properly configured for GDS
- **Performance benchmarking** - Measure I/O throughput, IOPS, and latency
- **Configuration-based testing** - Pre-configured test scenarios
- **Multi-GPU support** - Test multiple GPUs concurrently
- **Automated reporting** - Generate detailed performance reports

## Directory Structure

```
benchmarks/
├── gds_check.sh           # System verification script
├── gds_bench.sh           # Core benchmark script
├── run_benchmarks.sh      # Automated test runner
├── configs/               # Test configurations
│   ├── quick-test.conf
│   ├── seq-read-write.conf
│   ├── random-io.conf
│   ├── multi-gpu.conf
│   └── stress-test.conf
├── results/               # Benchmark results (created at runtime)
└── README.md             # This file
```

## Prerequisites

### Required

- Linux kernel 4.15 or later
- NVIDIA GPU (Pascal, Volta, Turing, or Ampere architecture)
- NVMe storage or supported distributed filesystem
- Built and loaded `nvidia-fs.ko` kernel module

### Optional (for enhanced functionality)

- `fio` - Flexible I/O tester (recommended for comprehensive benchmarks)
- `iostat` - I/O statistics monitoring
- `nvidia-smi` - GPU monitoring
- `jq` - JSON parsing for result analysis
- `bc` - Calculations in summary reports

Install optional tools on Ubuntu/Debian:
```bash
sudo apt-get install fio sysstat jq bc
```

Install optional tools on RHEL/CentOS/Rocky:
```bash
sudo yum install fio sysstat jq bc
```

## Quick Start

### 1. System Check

Before running benchmarks, verify your system is properly configured:

```bash
cd benchmarks
chmod +x *.sh
./gds_check.sh -p
```

This will check:
- Kernel version compatibility
- nvidia-fs module status
- GPU detection
- NVMe device availability
- Filesystem support
- Memory settings (IOMMU)

### 2. Run a Quick Test

Test basic GDS functionality:

```bash
./run_benchmarks.sh -c configs/quick-test.conf -p -s
```

This runs a 10-second sanity test with verification enabled.

### 3. Run Comprehensive Benchmarks

For sequential I/O testing:

```bash
./run_benchmarks.sh -c configs/seq-read-write.conf -p -s
```

For random I/O testing:

```bash
./run_benchmarks.sh -c configs/random-io.conf -p -s
```

## Available Scripts

### gds_check.sh - System Verification

Verifies GDS configuration and prerequisites.

**Usage:**
```bash
./gds_check.sh [OPTIONS]

Options:
  -p, --platform      Platform check (kernel, modules, GPU, storage)
  -f, --file FILE     Test specific file for GDS compatibility
  -t, --topology      Show GPU/storage topology
  -v, --verbose       Verbose output
  -a, --all           Run all checks
```

**Examples:**
```bash
# Basic platform check
./gds_check.sh -p

# Check specific file
./gds_check.sh -f /mnt/nvme/testfile

# Show GPU/storage topology
./gds_check.sh -t

# Run all checks with verbose output
./gds_check.sh -a -v
```

### gds_bench.sh - Core Benchmark Script

Low-level benchmark script for manual testing.

**Usage:**
```bash
./gds_bench.sh -d DEVICE [OPTIONS]

Required:
  -d, --device FILE/DIR   Device or directory to test

Optional:
  -g, --gpu ID            GPU device index (default: 0)
  -n, --numa NODE         NUMA node (default: 0)
  -w, --workers NUM       Number of worker threads (default: 32)
  -s, --size SIZE         File size in MB (default: 4096)
  -t, --duration TIME     Test duration in seconds (default: 30)
  -T, --test TYPE         Test type: seq-read, seq-write, rand-read, rand-write, all
  -B, --blocksize SIZE    I/O block size (e.g., 4K, 1M) or range (4K:4M:4K)
  -m, --mode MODE         Transfer mode: gds, p2p, staged, all
  -V, --verify            Enable data verification
  -q, --quick             Quick test mode
```

**Examples:**
```bash
# Basic sequential test
./gds_bench.sh -d /mnt/nvme/test -g 0

# Random read with 4K-1M block sizes
./gds_bench.sh -d /mnt/nvme/test -T rand-read -B 4K:1M:4K

# Quick verification test
./gds_bench.sh -d /mnt/nvme/test -q -V

# 60-second sequential write with 32 workers
./gds_bench.sh -d /mnt/nvme/test -T seq-write -t 60 -w 32
```

### run_benchmarks.sh - Automated Test Runner

Configuration-based automated testing.

**Usage:**
```bash
./run_benchmarks.sh -c CONFIG [OPTIONS]

Required:
  -c, --config FILE       Configuration file

Optional:
  -o, --output DIR        Output directory
  -p, --pre-check         Run system check before benchmarks
  -s, --summary           Generate summary report after completion
```

**Examples:**
```bash
# Run quick test with pre-check
./run_benchmarks.sh -c configs/quick-test.conf -p

# Run full sequential benchmark with summary
./run_benchmarks.sh -c configs/seq-read-write.conf -p -s

# Multi-GPU benchmark with custom output
./run_benchmarks.sh -c configs/multi-gpu.conf -o /mnt/results -p -s
```

## Configuration Files

Configuration files use INI-style format with `[global]` and `[job*]` sections.

### Example Configuration

```ini
[global]
name=my-test
duration=30
filesize=4096
workers=32
block_sizes=4K,64K,1M,4M
test_types=seq-read,seq-write
mode=gds
verify=0
numa=-1

[job1]
gpu=0
device=/mnt/nvme0/gds_test

[job2]
gpu=1
device=/mnt/nvme1/gds_test
```

### Configuration Parameters

**Global Section:**
- `name` - Test name
- `duration` - Test duration in seconds
- `filesize` - File size in MB
- `workers` - Number of worker threads
- `block_sizes` - Block sizes to test (comma-separated or range)
- `test_types` - Test types: seq-read, seq-write, rand-read, rand-write
- `mode` - Transfer mode: gds, p2p, staged, all
- `verify` - Enable verification (0=disabled, 1=enabled)
- `numa` - NUMA node (-1=auto-detect)

**Job Section:**
- `gpu` - GPU device ID
- `device` - Test device or directory path
- `numa` - Per-job NUMA node (optional)
- `workers` - Per-job worker count (optional)

### Pre-configured Tests

#### quick-test.conf
- **Purpose:** Fast sanity check
- **Duration:** 10 seconds
- **File size:** 1GB
- **Tests:** Sequential read/write with verification
- **Use case:** Quick validation after driver installation

#### seq-read-write.conf
- **Purpose:** Sequential I/O performance
- **Duration:** 30 seconds
- **File size:** 4GB
- **Block sizes:** 4K to 4M
- **Use case:** Baseline performance measurement

#### random-io.conf
- **Purpose:** Random I/O patterns (database workloads)
- **Duration:** 60 seconds
- **File size:** 8GB
- **Block sizes:** 4K to 128K
- **Use case:** Database and random access workloads

#### multi-gpu.conf
- **Purpose:** Multi-GPU concurrent testing
- **GPUs:** 4 GPUs
- **Use case:** Validating multi-GPU scaling

#### stress-test.conf
- **Purpose:** Extended stress testing
- **Duration:** 300 seconds (5 minutes)
- **File size:** 16GB
- **Workers:** 128
- **Verification:** Enabled
- **Use case:** Stability and reliability testing

## Understanding Results

### Benchmark Output

Each benchmark produces:

1. **Console output** - Real-time progress and results
2. **Result logs** - Detailed timing and performance data
3. **System info** - Hardware configuration snapshot
4. **FIO JSON** - Structured performance data (if fio is used)
5. **iostat logs** - System I/O statistics

### Result Directory Structure

```
results/
└── seq-read-write_20240115_143022/
    ├── benchmark_results_20240115_143022.log
    ├── system_info.txt
    ├── fio_seq-read_4K_gds.json
    ├── fio_seq-read_1M_gds.json
    ├── fio_seq-write_4K_gds.json
    ├── iostat_seq-read.log
    └── SUMMARY.md
```

### Key Metrics

- **Bandwidth (MB/s)** - Throughput for sequential operations
- **IOPS** - I/O operations per second (important for random I/O)
- **Latency (μs)** - Average I/O latency
- **CPU usage** - System, user, and IRQ time

### Performance Expectations

Typical GDS performance (varies by hardware):

| Workload | Block Size | GDS (GB/s) | Traditional (GB/s) | Speedup |
|----------|------------|------------|-------------------|---------|
| Seq Read | 1MB | 10-20 | 3-5 | 2-4x |
| Seq Write | 1MB | 8-15 | 3-5 | 2-3x |
| Rand Read | 4KB | 100K IOPS | 50K IOPS | 2x |

*Note: Results vary significantly based on GPU model, NVMe device, PCIe configuration, and CPU.*

## Troubleshooting

### Module Not Loaded

```bash
Error: nvidia-fs module is NOT loaded
```

**Solution:**
```bash
cd ../src
sudo make
sudo insmod nvidia-fs.ko
```

### Permission Denied

```bash
Error: Cannot access /mnt/nvme/test
```

**Solution:**
```bash
# Ensure directory exists and has proper permissions
sudo mkdir -p /mnt/nvme/test
sudo chmod 777 /mnt/nvme/test
```

### Low Performance

If performance is lower than expected:

1. **Check IOMMU status:**
   ```bash
   ./gds_check.sh -p | grep IOMMU
   ```
   Disable IOMMU for better performance: add `intel_iommu=off` or `amd_iommu=off` to kernel parameters

2. **Verify filesystem:** XFS or EXT4 in ordered mode recommended

3. **Check PCIe topology:**
   ```bash
   ./gds_check.sh -t
   ```
   Ensure GPU and NVMe are on same NUMA node when possible

4. **Monitor CPU usage:** High CPU usage may indicate fallback to non-GDS path

### Verification Failures

```bash
Error: Data verification failed
```

**Possible causes:**
- Filesystem doesn't support O_DIRECT properly
- Hardware issue (rare)
- Driver bug

**Debug steps:**
```bash
# Run with shorter test and verbose output
./gds_bench.sh -d /mnt/nvme/test -V -s 1024 -t 10 -v
```

## Common Usage Patterns

### Development Workflow

```bash
# 1. Build and load module
cd ../src
make clean && make
sudo insmod nvidia-fs.ko

# 2. Verify system
cd ../benchmarks
./gds_check.sh -a

# 3. Quick functional test
./run_benchmarks.sh -c configs/quick-test.conf -p

# 4. Full performance test
./run_benchmarks.sh -c configs/seq-read-write.conf -p -s
```

### Performance Comparison

Compare GDS vs traditional I/O:

```bash
# Test with GDS
./gds_bench.sh -d /mnt/nvme/test -m gds -T seq-read -o results/gds

# Test without GDS (staged through CPU)
./gds_bench.sh -d /mnt/nvme/test -m staged -T seq-read -o results/staged

# Compare results
diff results/gds/benchmark_results_*.log results/staged/benchmark_results_*.log
```

### Multi-GPU Scaling

Test scaling across multiple GPUs:

```bash
# Edit configs/multi-gpu.conf to match your GPU count
# Then run:
./run_benchmarks.sh -c configs/multi-gpu.conf -p -s

# Results will show per-GPU and aggregate performance
```

### Continuous Integration

For automated testing in CI/CD:

```bash
#!/bin/bash
set -e

# Run quick validation
./run_benchmarks.sh -c configs/quick-test.conf -p || exit 1

# Check for performance regression
# (compare against baseline results)

exit 0
```

## Advanced Topics

### Custom Configuration

Create custom configurations for specific workloads:

```bash
cp configs/seq-read-write.conf configs/my-workload.conf
# Edit my-workload.conf with your parameters
./run_benchmarks.sh -c configs/my-workload.conf -p -s
```

### Profiling with perf

Combine with Linux perf for detailed profiling:

```bash
sudo perf record -g ./gds_bench.sh -d /mnt/nvme/test -T seq-read
sudo perf report
```

### Integration with Monitoring

Monitor GDS stats during benchmarks:

```bash
# In one terminal
watch -n 1 cat /proc/driver/nvidia-fs/stats

# In another terminal
./run_benchmarks.sh -c configs/stress-test.conf
```

## References

- [NVIDIA GPUDirect Storage Documentation](https://docs.nvidia.com/gpudirect-storage/)
- [NVIDIA cuFile Tools](https://developer.nvidia.com/cufile)
- [FIO Documentation](https://fio.readthedocs.io/)
- [Main Project README](../README.md)

## Known Issues

Based on analysis of [NVIDIA GDS issues](https://github.com/NVIDIA/gds-nvidia-fs/issues):

1. **Kernel Compatibility:** Some kernel versions (6.1, 6.2, 6.6+) may require patches
2. **NVMe Driver:** Requires GDS-patched NVMe driver for optimal performance
3. **RAID Compatibility:** P2P DMA may not work with all RAID configurations
4. **VM Flags:** Kernel 6.3+ requires VM flag modification handling

## Contributing

See the main [CONTRIBUTING.md](../CONTRIBUTING.md) for contribution guidelines.

For benchmark-specific improvements:
- Add new test configurations in `configs/`
- Extend benchmark scripts with new test patterns
- Improve result parsing and reporting
- Add support for additional storage types

## License

GPL-2.0 - See [LICENSE](../LICENSE) for details.

---

**Last Updated:** 2024
**Maintainer:** GDS Nouveau Project
