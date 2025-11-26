#!/bin/bash
# GDS (GPUDirect Storage) System Check Script
# Based on NVIDIA gdscheck.py but simplified for nouveau-based GDS driver
#
# Copyright (c) 2024
# License: GPL-2.0

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_header() {
    echo ""
    echo "========================================"
    echo "$1"
    echo "========================================"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_info() {
    echo "       $1"
}

check_kernel_version() {
    print_header "Kernel Version Check"

    KERNEL_VERSION=$(uname -r)
    KERNEL_MAJOR=$(echo $KERNEL_VERSION | cut -d. -f1)
    KERNEL_MINOR=$(echo $KERNEL_VERSION | cut -d. -f2)

    print_info "Running kernel: $KERNEL_VERSION"

    # GDS requires kernel 4.15+
    if [ $KERNEL_MAJOR -gt 4 ] || ([ $KERNEL_MAJOR -eq 4 ] && [ $KERNEL_MINOR -ge 15 ]); then
        print_pass "Kernel version is compatible (>= 4.15.0)"
    else
        print_fail "Kernel version too old (require >= 4.15.0)"
        return 1
    fi
}

check_nvidia_fs_module() {
    print_header "NVIDIA-FS Module Check"

    if lsmod | grep -q nvidia_fs; then
        print_pass "nvidia-fs module is loaded"

        # Check module version
        if [ -f /sys/module/nvidia_fs/version ]; then
            VERSION=$(cat /sys/module/nvidia_fs/version)
            print_info "Module version: $VERSION"
        fi

        # Check module parameters
        if [ -d /sys/module/nvidia_fs/parameters ]; then
            print_info "Module parameters:"
            for param in /sys/module/nvidia_fs/parameters/*; do
                if [ -r "$param" ]; then
                    param_name=$(basename $param)
                    param_value=$(cat $param)
                    print_info "  $param_name = $param_value"
                fi
            done
        fi

        return 0
    else
        print_fail "nvidia-fs module is NOT loaded"
        print_info "GDS will operate in compatible/P2P mode (NVMe only)"
        print_info "To load: sudo insmod src/nvidia-fs.ko"
        return 1
    fi
}

check_proc_interface() {
    print_header "Proc Interface Check"

    if [ -d /proc/driver/nvidia-fs ]; then
        print_pass "/proc/driver/nvidia-fs exists"

        # Check various proc files
        for file in stats version peer_distance; do
            if [ -f /proc/driver/nvidia-fs/$file ]; then
                print_pass "  $file is available"
                if [ "$VERBOSE" = "1" ]; then
                    print_info "  Content preview:"
                    head -n 5 /proc/driver/nvidia-fs/$file | while read line; do
                        print_info "    $line"
                    done
                fi
            else
                print_warn "  $file is not available"
            fi
        done
    else
        print_fail "/proc/driver/nvidia-fs does not exist"
        return 1
    fi
}

check_gpu_presence() {
    print_header "GPU Detection"

    # Check for NVIDIA GPUs
    if command -v nvidia-smi &> /dev/null; then
        GPU_COUNT=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | wc -l)
        if [ $GPU_COUNT -gt 0 ]; then
            print_pass "Found $GPU_COUNT NVIDIA GPU(s)"
            nvidia-smi --query-gpu=index,name,pci.bus_id,memory.total --format=csv,noheader 2>/dev/null | while read line; do
                print_info "  GPU: $line"
            done
        else
            print_warn "No NVIDIA GPUs detected via nvidia-smi"
        fi
    else
        print_warn "nvidia-smi not found"
    fi

    # Check for Nouveau driver (open-source)
    if lsmod | grep -q nouveau; then
        print_pass "Nouveau driver is loaded (open-source NVIDIA driver)"
    fi

    # Check for GPUs via lspci
    GPU_PCI_COUNT=$(lspci | grep -i 'vga.*nvidia\|3d.*nvidia' | wc -l)
    if [ $GPU_PCI_COUNT -gt 0 ]; then
        print_pass "Found $GPU_PCI_COUNT GPU(s) via PCI enumeration"
        lspci | grep -i 'vga.*nvidia\|3d.*nvidia' | while read line; do
            print_info "  $line"
        done
    else
        print_warn "No NVIDIA GPUs found via PCI"
    fi
}

check_nvme_devices() {
    print_header "NVMe Device Check"

    NVME_COUNT=$(lsblk -o NAME,KNAME,TRAN | grep nvme | wc -l)

    if [ $NVME_COUNT -gt 0 ]; then
        print_pass "Found $NVME_COUNT NVMe device(s)"
        lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,TRAN | grep -E 'NAME|nvme' | while read line; do
            print_info "  $line"
        done

        # Check for NVMe driver
        if lsmod | grep -q nvme; then
            print_pass "NVMe driver is loaded"

            # Check if NVMe driver is patched for GDS
            if grep -q "nvme.*register_nvfs_dma_ops" /proc/kallsyms 2>/dev/null; then
                print_pass "NVMe driver appears to be GDS-patched"
            else
                print_warn "NVMe driver may not be GDS-patched"
                print_info "  GDS may still work in P2P mode"
            fi
        else
            print_fail "NVMe driver is NOT loaded"
        fi
    else
        print_warn "No NVMe devices detected"
        print_info "GDS requires NVMe or supported storage"
    fi
}

check_filesystem_support() {
    print_header "Filesystem Support Check"

    print_info "Checking mounted filesystems..."

    # Check for supported filesystems
    for fs in xfs ext4 nfs; do
        if mount | grep -q "type $fs"; then
            print_pass "$fs filesystem detected"
            mount | grep "type $fs" | while read line; do
                print_info "  $line"
            done
        fi
    done

    # Check for O_DIRECT support (critical for GDS)
    print_info "GDS requires O_DIRECT support on the filesystem"
    print_info "Supported: XFS, EXT4 (ordered mode), NFS over RDMA"
}

check_memory_settings() {
    print_header "Memory Settings Check"

    # Check IOMMU status
    if [ -d /sys/class/iommu ]; then
        IOMMU_COUNT=$(ls /sys/class/iommu 2>/dev/null | wc -l)
        if [ $IOMMU_COUNT -gt 0 ]; then
            print_warn "IOMMU appears to be enabled ($IOMMU_COUNT devices)"
            print_info "  GDS performs better with IOMMU disabled"
            print_info "  Add 'intel_iommu=off' or 'amd_iommu=off' to kernel cmdline"
        else
            print_pass "IOMMU appears to be disabled (recommended for GDS)"
        fi
    fi

    # Check hugepages
    HUGEPAGES=$(cat /proc/meminfo | grep HugePages_Total | awk '{print $2}')
    if [ "$HUGEPAGES" -gt 0 ]; then
        print_info "Hugepages configured: $HUGEPAGES"
    fi
}

check_dependencies() {
    print_header "Dependency Check"

    TOOLS=("lsblk" "lspci" "lsmod" "mount")

    for tool in "${TOOLS[@]}"; do
        if command -v $tool &> /dev/null; then
            print_pass "$tool is available"
        else
            print_fail "$tool is NOT available"
        fi
    done
}

run_functional_test() {
    print_header "Functional Test"

    if [ -z "$TEST_FILE" ]; then
        print_warn "No test file specified, skipping functional test"
        print_info "Use: $0 -f /path/to/test/file"
        return 0
    fi

    print_info "Testing file: $TEST_FILE"

    # Check if file exists and is on a suitable filesystem
    if [ ! -e "$TEST_FILE" ]; then
        # Try to create test directory
        TEST_DIR=$(dirname "$TEST_FILE")
        if [ ! -d "$TEST_DIR" ]; then
            print_warn "Test directory doesn't exist: $TEST_DIR"
            return 1
        fi
    fi

    # Check filesystem type
    FS_TYPE=$(df -T "$TEST_FILE" 2>/dev/null | tail -1 | awk '{print $2}')
    print_info "Filesystem type: $FS_TYPE"

    case $FS_TYPE in
        xfs|ext4|nfs|nfs4)
            print_pass "Filesystem type is supported"
            ;;
        *)
            print_warn "Filesystem type may not be optimal for GDS"
            ;;
    esac

    # Check if we can create O_DIRECT files
    print_info "Testing O_DIRECT support..."

    # Note: Actual O_DIRECT testing would require a C program
    # This is a placeholder for now
    print_info "O_DIRECT test requires compiled test program"
}

show_topology() {
    print_header "GPU/Storage Topology"

    if [ ! -f /proc/driver/nvidia-fs/peer_distance ]; then
        print_warn "Cannot read topology: /proc/driver/nvidia-fs/peer_distance not found"
        return 1
    fi

    print_info "GPU to Storage Device Distances:"
    cat /proc/driver/nvidia-fs/peer_distance
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

GDS (GPUDirect Storage) System Check Script

OPTIONS:
    -h, --help          Show this help message
    -p, --platform      Platform check (kernel, modules, GPU, storage)
    -f, --file FILE     Test specific file for GDS compatibility
    -t, --topology      Show GPU/storage topology
    -v, --verbose       Verbose output
    -a, --all           Run all checks

Examples:
    $0 -p                       # Platform check only
    $0 -f /mnt/nvme/testfile   # Test specific file
    $0 -t                       # Show topology
    $0 -a                       # Run all checks

EOF
}

# Main script
main() {
    VERBOSE=0
    PLATFORM=0
    TOPOLOGY=0
    ALL=0
    TEST_FILE=""

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -p|--platform)
                PLATFORM=1
                shift
                ;;
            -f|--file)
                TEST_FILE="$2"
                shift 2
                ;;
            -t|--topology)
                TOPOLOGY=1
                shift
                ;;
            -v|--verbose)
                VERBOSE=1
                shift
                ;;
            -a|--all)
                ALL=1
                shift
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # If no options specified, show help
    if [ $PLATFORM -eq 0 ] && [ $TOPOLOGY -eq 0 ] && [ $ALL -eq 0 ] && [ -z "$TEST_FILE" ]; then
        usage
        exit 0
    fi

    echo "GDS System Check - Nouveau-based GPUDirect Storage"
    echo "=================================================="

    if [ $ALL -eq 1 ] || [ $PLATFORM -eq 1 ]; then
        check_dependencies
        check_kernel_version
        check_nvidia_fs_module
        check_proc_interface
        check_gpu_presence
        check_nvme_devices
        check_filesystem_support
        check_memory_settings
    fi

    if [ ! -z "$TEST_FILE" ]; then
        run_functional_test
    fi

    if [ $ALL -eq 1 ] || [ $TOPOLOGY -eq 1 ]; then
        show_topology
    fi

    echo ""
    print_header "Check Complete"
}

main "$@"
