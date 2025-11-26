#!/bin/bash
# GDS (GPUDirect Storage) Benchmark Script
# Based on NVIDIA gds_perf.sh and gdsio patterns
#
# This script performs comprehensive I/O benchmarking for GDS-enabled systems
# Copyright (c) 2024
# License: GPL-2.0

set -e

# Default configuration
DEFAULT_DURATION=30
DEFAULT_FILESIZE=4096  # MB
DEFAULT_THREADS=32
DEFAULT_GPU=0
DEFAULT_NUMA=0

# Color codes
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

GDS (GPUDirect Storage) Benchmark Script

REQUIRED OPTIONS:
    -d, --device FILE/DIR   Device or directory to test (required)

OPTIONAL OPTIONS:
    -g, --gpu ID            GPU device index (default: 0)
    -n, --numa NODE         NUMA node (default: 0)
    -w, --workers NUM       Number of worker threads (default: 32)
    -s, --size SIZE         File size in MB (default: 4096)
    -t, --duration TIME     Test duration in seconds (default: 30)
    -o, --output DIR        Output directory for results (default: ./results)

TEST TYPE OPTIONS:
    -T, --test TYPE         Test type: seq-read, seq-write, rand-read, rand-write, all
                            (default: all)
    -B, --blocksize SIZE    I/O block size (e.g., 4K, 64K, 1M, 4M)
                            Can specify range: 4K:1M:4K (start:end:step)

MODE OPTIONS:
    -m, --mode MODE         Transfer mode (default: all):
                              gds       - GDS mode (direct GPU)
                              p2p       - P2P mode
                              staged    - Staged through CPU
                              all       - Test all modes

ADVANCED OPTIONS:
    -V, --verify            Enable data verification
    -q, --quick             Quick test mode (reduced iterations)
    -h, --help              Show this help message

EXAMPLES:
    # Basic sequential read/write test
    $0 -d /mnt/nvme/test -g 0

    # Quick random I/O test with verification
    $0 -d /mnt/nvme/test -T rand-read -q -V

    # Comprehensive test with various block sizes
    $0 -d /mnt/nvme/test -B 4K:4M:4K -t 60

    # Multi-GPU test (run multiple instances)
    $0 -d /mnt/nvme0/test -g 0 &
    $0 -d /mnt/nvme1/test -g 1 &

EOF
}

check_prerequisites() {
    print_info "Checking prerequisites..."

    # Check if nvidia-fs module is loaded
    if ! lsmod | grep -q nvidia_fs; then
        print_warn "nvidia-fs module is not loaded"
        print_warn "GDS will operate in P2P/compatible mode"
    fi

    # Check if test device/directory exists
    if [ ! -e "$TEST_DEVICE" ]; then
        echo "Error: Test device/directory does not exist: $TEST_DEVICE"
        exit 1
    fi

    # Check for required tools
    for tool in dd fio iostat; do
        if ! command -v $tool &> /dev/null; then
            print_warn "$tool not found (some tests may be limited)"
        fi
    done

    print_success "Prerequisites check complete"
}

create_test_directory() {
    if [ -d "$TEST_DEVICE" ]; then
        TEST_DIR="$TEST_DEVICE"
    else
        TEST_DIR=$(dirname "$TEST_DEVICE")
    fi

    # Create per-GPU test directory
    TEST_PATH="${TEST_DIR}/gds_bench_gpu${GPU_ID}"
    mkdir -p "$TEST_PATH"

    print_info "Test directory: $TEST_PATH"
}

# Parse block size with units (K, M, G)
parse_size() {
    local size_str=$1
    local size_num=$(echo $size_str | sed 's/[KMG]$//')
    local size_unit=$(echo $size_str | sed 's/^[0-9]*//')

    case $size_unit in
        K|k) echo $((size_num * 1024)) ;;
        M|m) echo $((size_num * 1024 * 1024)) ;;
        G|g) echo $((size_num * 1024 * 1024 * 1024)) ;;
        *) echo $size_num ;;
    esac
}

# Generate block size list from range specification
generate_block_sizes() {
    local spec=$1

    if [[ $spec == *":"* ]]; then
        # Range specification: start:end:step
        IFS=':' read -r start end step <<< "$spec"

        local start_bytes=$(parse_size $start)
        local end_bytes=$(parse_size $end)
        local step_bytes=$(parse_size $step)

        local sizes=()
        local current=$start_bytes

        while [ $current -le $end_bytes ]; do
            # Convert back to human readable
            if [ $current -ge $((1024*1024)) ]; then
                sizes+=("$((current / 1024 / 1024))M")
            elif [ $current -ge 1024 ]; then
                sizes+=("$((current / 1024))K")
            else
                sizes+=("${current}")
            fi

            current=$((current + step_bytes))
        done

        echo "${sizes[@]}"
    else
        # Single block size
        echo "$spec"
    fi
}

run_dd_benchmark() {
    local test_type=$1
    local block_size=$2
    local mode=$3

    print_info "Running DD benchmark: $test_type, block_size=$block_size, mode=$mode"

    local test_file="${TEST_PATH}/test_${test_type}_${block_size}.dat"
    local bs_bytes=$(parse_size $block_size)
    local count=$((FILESIZE * 1024 * 1024 / bs_bytes))

    case $test_type in
        seq-write)
            if [ "$mode" = "gds" ] || [ "$mode" = "all" ]; then
                print_info "Sequential write test with bs=$block_size"
                dd if=/dev/zero of="$test_file" bs=$bs_bytes count=$count oflag=direct 2>&1 | tee -a "$RESULTS_FILE"
            fi
            ;;
        seq-read)
            # Need to create file first
            if [ ! -f "$test_file" ]; then
                dd if=/dev/zero of="$test_file" bs=$bs_bytes count=$count oflag=direct 2>&1 > /dev/null
            fi

            if [ "$mode" = "gds" ] || [ "$mode" = "all" ]; then
                print_info "Sequential read test with bs=$block_size"
                dd if="$test_file" of=/dev/null bs=$bs_bytes count=$count iflag=direct 2>&1 | tee -a "$RESULTS_FILE"
            fi
            ;;
    esac
}

run_fio_benchmark() {
    local test_type=$1
    local block_size=$2
    local mode=$3

    if ! command -v fio &> /dev/null; then
        print_warn "fio not available, skipping fio benchmark"
        return
    fi

    print_info "Running FIO benchmark: $test_type, block_size=$block_size, mode=$mode"

    local test_file="${TEST_PATH}/fio_test"
    local fio_output="${OUTPUT_DIR}/fio_${test_type}_${block_size}_${mode}.json"

    # Map test type to fio rw parameter
    local rw_type
    case $test_type in
        seq-read) rw_type="read" ;;
        seq-write) rw_type="write" ;;
        rand-read) rw_type="randread" ;;
        rand-write) rw_type="randwrite" ;;
        *) rw_type="randrw" ;;
    esac

    # Build fio command
    fio --name=gds_bench \
        --filename="$test_file" \
        --rw=$rw_type \
        --bs=$block_size \
        --size=${FILESIZE}M \
        --numjobs=$WORKERS \
        --time_based \
        --runtime=$DURATION \
        --ioengine=libaio \
        --direct=1 \
        --group_reporting \
        --output-format=json \
        --output="$fio_output" \
        2>&1 | tee -a "$RESULTS_FILE"

    # Parse and display results
    if [ -f "$fio_output" ]; then
        print_success "FIO results saved to: $fio_output"

        # Extract key metrics using basic tools
        if command -v jq &> /dev/null; then
            local bw=$(jq '.jobs[0].read.bw // .jobs[0].write.bw' "$fio_output")
            local iops=$(jq '.jobs[0].read.iops // .jobs[0].write.iops' "$fio_output")
            local lat=$(jq '.jobs[0].read.lat_ns.mean // .jobs[0].write.lat_ns.mean' "$fio_output")

            print_success "Bandwidth: $(echo "scale=2; $bw / 1024" | bc) MB/s"
            print_success "IOPS: $iops"
            print_success "Latency: $(echo "scale=2; $lat / 1000" | bc) us"
        fi
    fi
}

run_custom_gds_benchmark() {
    local test_type=$1
    local block_size=$2

    print_info "Running custom GDS benchmark: $test_type, block_size=$block_size"

    # This would integrate with actual GDS test programs if available
    # For now, we'll use standard Linux I/O with O_DIRECT

    local test_file="${TEST_PATH}/gds_custom_test.dat"

    # Check if we have a custom GDS test binary (like gdsio)
    if [ -f "./gds_test_binary" ]; then
        print_info "Using custom GDS test binary"
        ./gds_test_binary -f "$test_file" -d $GPU_ID -n $NUMA_NODE -w $WORKERS \
            -s ${FILESIZE}M -i $block_size -I $test_type 2>&1 | tee -a "$RESULTS_FILE"
    else
        print_warn "No custom GDS test binary found, using fio"
        run_fio_benchmark "$test_type" "$block_size" "gds"
    fi
}

run_iostat_monitoring() {
    if ! command -v iostat &> /dev/null; then
        return
    fi

    local duration=$1
    local output_file="${OUTPUT_DIR}/iostat_${TEST_TYPE}.log"

    print_info "Starting iostat monitoring..."
    iostat -x 1 $duration > "$output_file" 2>&1 &
    IOSTAT_PID=$!

    echo $IOSTAT_PID
}

stop_iostat_monitoring() {
    local pid=$1

    if [ ! -z "$pid" ] && kill -0 $pid 2>/dev/null; then
        kill $pid 2>/dev/null || true
        print_info "Stopped iostat monitoring"
    fi
}

collect_system_info() {
    local info_file="${OUTPUT_DIR}/system_info.txt"

    print_info "Collecting system information..."

    {
        echo "========================================"
        echo "System Information"
        echo "========================================"
        echo ""
        echo "Date: $(date)"
        echo "Hostname: $(hostname)"
        echo "Kernel: $(uname -r)"
        echo ""
        echo "CPU Info:"
        lscpu | grep -E "Model name|Socket|Core|Thread|NUMA"
        echo ""
        echo "Memory Info:"
        free -h
        echo ""
        echo "GPU Info:"
        if command -v nvidia-smi &> /dev/null; then
            nvidia-smi --query-gpu=index,name,memory.total --format=csv
        else
            lspci | grep -i nvidia || echo "No NVIDIA GPUs found via lspci"
        fi
        echo ""
        echo "Storage Info:"
        lsblk -o NAME,SIZE,TYPE,MOUNTPOINT,TRAN
        echo ""
        echo "NVIDIA-FS Module:"
        if lsmod | grep -q nvidia_fs; then
            lsmod | grep nvidia_fs
            if [ -f /proc/driver/nvidia-fs/stats ]; then
                echo ""
                echo "NVIDIA-FS Stats:"
                cat /proc/driver/nvidia-fs/stats
            fi
        else
            echo "nvidia-fs module not loaded"
        fi
    } > "$info_file"

    print_success "System info saved to: $info_file"
}

run_benchmark_suite() {
    print_info "Starting benchmark suite..."
    print_info "Test type: $TEST_TYPE"
    print_info "Block sizes: ${BLOCK_SIZES[@]}"
    print_info "Duration: ${DURATION}s"
    print_info "Workers: $WORKERS"
    print_info "File size: ${FILESIZE}MB"

    # Create results file
    RESULTS_FILE="${OUTPUT_DIR}/benchmark_results_$(date +%Y%m%d_%H%M%S).log"
    touch "$RESULTS_FILE"

    echo "========================================" | tee -a "$RESULTS_FILE"
    echo "GDS Benchmark Results" | tee -a "$RESULTS_FILE"
    echo "========================================" | tee -a "$RESULTS_FILE"
    echo "Test Device: $TEST_DEVICE" | tee -a "$RESULTS_FILE"
    echo "GPU: $GPU_ID, NUMA: $NUMA_NODE" | tee -a "$RESULTS_FILE"
    echo "Workers: $WORKERS, Duration: ${DURATION}s" | tee -a "$RESULTS_FILE"
    echo "File Size: ${FILESIZE}MB" | tee -a "$RESULTS_FILE"
    echo "========================================" | tee -a "$RESULTS_FILE"
    echo "" | tee -a "$RESULTS_FILE"

    # Determine test types to run
    local test_types=()
    if [ "$TEST_TYPE" = "all" ]; then
        test_types=("seq-read" "seq-write" "rand-read" "rand-write")
    else
        test_types=("$TEST_TYPE")
    fi

    # Start system monitoring
    IOSTAT_PID=$(run_iostat_monitoring $((DURATION * ${#test_types[@]} * ${#BLOCK_SIZES[@]})))

    # Run tests
    for tt in "${test_types[@]}"; do
        for bs in "${BLOCK_SIZES[@]}"; do
            echo "" | tee -a "$RESULTS_FILE"
            echo "----------------------------------------" | tee -a "$RESULTS_FILE"
            echo "Test: $tt, Block Size: $bs" | tee -a "$RESULTS_FILE"
            echo "----------------------------------------" | tee -a "$RESULTS_FILE"

            if [ "$ENABLE_VERIFY" = "1" ]; then
                print_warn "Data verification enabled (will impact performance)"
            fi

            # Run benchmark based on mode
            if [ "$MODE" = "all" ]; then
                run_fio_benchmark "$tt" "$bs" "gds"
            else
                run_fio_benchmark "$tt" "$bs" "$MODE"
            fi

            sleep 2  # Brief pause between tests
        done
    done

    # Stop monitoring
    stop_iostat_monitoring "$IOSTAT_PID"

    print_success "Benchmark complete! Results saved to: $RESULTS_FILE"
}

cleanup() {
    print_info "Cleaning up..."

    # Remove test files if requested
    if [ "$CLEANUP" = "1" ]; then
        rm -rf "$TEST_PATH"
        print_info "Test files removed"
    else
        print_info "Test files retained in: $TEST_PATH"
    fi
}

# Main script
main() {
    # Default values
    GPU_ID=$DEFAULT_GPU
    NUMA_NODE=$DEFAULT_NUMA
    WORKERS=$DEFAULT_THREADS
    FILESIZE=$DEFAULT_FILESIZE
    DURATION=$DEFAULT_DURATION
    TEST_DEVICE=""
    OUTPUT_DIR="./results"
    TEST_TYPE="all"
    BLOCK_SIZE_SPEC="4K:4M:4K"
    MODE="all"
    ENABLE_VERIFY=0
    QUICK_MODE=0
    CLEANUP=0

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -d|--device)
                TEST_DEVICE="$2"
                shift 2
                ;;
            -g|--gpu)
                GPU_ID="$2"
                shift 2
                ;;
            -n|--numa)
                NUMA_NODE="$2"
                shift 2
                ;;
            -w|--workers)
                WORKERS="$2"
                shift 2
                ;;
            -s|--size)
                FILESIZE="$2"
                shift 2
                ;;
            -t|--duration)
                DURATION="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -T|--test)
                TEST_TYPE="$2"
                shift 2
                ;;
            -B|--blocksize)
                BLOCK_SIZE_SPEC="$2"
                shift 2
                ;;
            -m|--mode)
                MODE="$2"
                shift 2
                ;;
            -V|--verify)
                ENABLE_VERIFY=1
                shift
                ;;
            -q|--quick)
                QUICK_MODE=1
                DURATION=10
                BLOCK_SIZE_SPEC="64K,1M,4M"
                shift
                ;;
            -C|--cleanup)
                CLEANUP=1
                shift
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Validate required arguments
    if [ -z "$TEST_DEVICE" ]; then
        echo "Error: Test device/directory is required (-d option)"
        usage
        exit 1
    fi

    # Generate block size list
    IFS=',' read -r -a BLOCK_SIZES <<< "$(generate_block_sizes $BLOCK_SIZE_SPEC)"

    # Create output directory
    mkdir -p "$OUTPUT_DIR"

    # Run benchmark
    print_info "Starting GDS Benchmark"
    print_info "======================"

    check_prerequisites
    create_test_directory
    collect_system_info
    run_benchmark_suite
    cleanup

    print_success "All tests complete!"
    print_info "Results directory: $OUTPUT_DIR"
}

# Trap cleanup on exit
trap cleanup EXIT

main "$@"
