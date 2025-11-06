# NVIDIA GPUDirect Storage Kernel Module

[![Build](https://github.com/jcfaracco/gds-nouveau-fs/actions/workflows/build.yml/badge.svg)](https://github.com/jcfaracco/gds-nouveau-fs/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/License-MIT%2BGPL--2.0-blue.svg)](LICENSE)

A Linux kernel module that enables direct I/O between GPU memory and storage devices, bypassing CPU memory copies for maximum performance. This is the upstream-aligned version designed for integration with nouveau and open-source GPU drivers.

## 🚀 Features

- **Zero-copy I/O**: Direct data transfer between storage and GPU memory
- **High Performance**: Eliminates CPU bottlenecks for GPU workloads
- **Broad Storage Support**: Works with NVMe, NVMeOF, distributed filesystems
- **RDMA Ready**: Supports RDMA-capable storage solutions

### Supported Storage Solutions

- ✅ XFS and EXT4 filesystems (ordered mode) on NVMe/NVMeOF/ScaleFlux CSD
- ✅ NFS over RDMA with MOFED 5.1+
- ✅ RDMA-capable distributed filesystems (DDN Exascaler, WekaFS, VAST)
- ✅ ScaleFlux Computational Storage

## 📋 Requirements

### Hardware
- NVIDIA Tesla or Quadro GPUs (Pascal, Volta, Turing, or Ampere architecture)
- NVMe/NVMeOF storage devices or supported distributed filesystem

### Software
- Linux kernel 4.15.0+ 
- CUDA Toolkit 10.0+
- GPU display driver ≥ 418.40
- MOFED 5.1+ (for RDMA functionality)

## 🛠️ Installation

### Quick Start

```bash
cd src
export CONFIG_MOFED_VERSION=$(ofed_info -s | cut -d '-' -f 2)
sudo make
sudo insmod nvidia-fs.ko
```

### Development Build

```bash
# Clone the repository
git clone <repository-url>
cd kmt/src

# Configure environment
export CONFIG_MOFED_VERSION=5.4-1.0.3.0  # or your MOFED version
export GDS_VERSION=$(cat GDS_VERSION)

# Build
make clean
make module

# Install
sudo insmod nvidia-fs.ko
```

### Verification

```bash
# Check if module is loaded
lsmod | grep nvidia_fs

# Check module info
modinfo nvidia-fs.ko
```

## 📚 Documentation

For comprehensive usage documentation, visit the [NVIDIA GPUDirect Storage Documentation](https://docs.nvidia.com/gpudirect-storage/index.html).

## 🔧 Development

### Building

The module uses a standard Linux kernel module build system:

```bash
make clean    # Clean build artifacts
make module   # Build the kernel module
make install  # Install the module (requires root)
```

### Testing

```bash
# Load the module with debug enabled
sudo insmod nvidia-fs.ko debug=1

# Check kernel logs
dmesg | tail -n 20
```

The project includes a comprehensive self-test suite with automated test execution, stress testing, and detailed validation of core functionality. For complete testing documentation, available test suites, and usage instructions, see [tests README.md](src/tests/README.md).

## 🤝 Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

If you need a comprehensive definition of this module, read the [module README.md](src/README.md).

### Quick Contribution Guide

1. 🐛 **File an issue** for bugs or feature requests
2. 🔀 **Fork and create** a feature branch
3. ✨ **Follow** conventional commit messages
4. 📝 **Sign your commits** with `git commit -s`
5. 🔍 **Submit** a pull request

## 📄 License

This project is licensed under GPL v2.0 for kernel module components

See [LICENSE](LICENSE) for full details.

## 🔗 Links

- [NVIDIA GPUDirect Storage Documentation](https://docs.nvidia.com/gpudirect-storage/index.html)
- [CUDA Toolkit Downloads](https://developer.nvidia.com/cuda-downloads)
- [Mellanox OFED Downloads](https://www.mellanox.com/products/infiniband-drivers/linux/mlnx_ofed)

## ⚠️ Disclaimer

This is an upstream-aligned development version intended for integration with open-source GPU drivers. For production deployments, consider the official NVIDIA GPUDirect Storage release.

---

**Note**: This project follows [Semantic Versioning](https://semver.org/) and uses [Conventional Commits](https://www.conventionalcommits.org/) for commit messages.
