#!/bin/bash
# GDS Benchmark Runner
# Automated benchmark execution with configuration file support
#
# Copyright (c) 2024
# License: GPL-2.0

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="${SCRIPT_DIR}/configs"
RESULTS_BASE_DIR="${SCRIPT_DIR}/results"

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo "========================================"
    echo "$1"
    echo "========================================"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

GDS Benchmark Runner - Automated benchmark execution

OPTIONS:
    -c, --config FILE       Configuration file (required)
    -o, --output DIR        Output directory (default: ./results/TIMESTAMP)
    -p, --pre-check         Run system check before benchmarks
    -s, --summary           Generate summary report after completion
    -h, --help              Show this help message

CONFIGURATION:
    Configuration files should be placed in: $CONFIG_DIR
    Use .conf extension

EXAMPLES:
    # Run quick test
    $0 -c configs/quick-test.conf -p

    # Run sequential I/O benchmark with summary
    $0 -c configs/seq-read-write.conf -p -s

    # Run stress test with custom output directory
    $0 -c configs/stress-test.conf -o /mnt/results -p -s

    # Run multi-GPU benchmark
    $0 -c configs/multi-gpu.conf -p -s

AVAILABLE CONFIGURATIONS:
EOF

    # List available configs
    if [ -d "$CONFIG_DIR" ]; then
        echo ""
        for conf in "$CONFIG_DIR"/*.conf; do
            if [ -f "$conf" ]; then
                conf_name=$(basename "$conf")
                conf_desc=$(grep "^# " "$conf" | head -1 | sed 's/^# //')
                echo "    $conf_name"
                if [ ! -z "$conf_desc" ]; then
                    echo "        $conf_desc"
                fi
            fi
        done
    fi
}

parse_config_file() {
    local config_file=$1

    if [ ! -f "$config_file" ]; then
        print_error "Config file not found: $config_file"
        exit 1
    fi

    print_info "Parsing configuration: $config_file"

    # Extract global settings
    GLOBAL_DURATION=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "duration") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_FILESIZE=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "filesize") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_WORKERS=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "workers") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_BLOCK_SIZES=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "block_sizes") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_TEST_TYPES=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "test_types") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_MODE=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "mode") print $2}' "$config_file" | tr -d ' ')
    GLOBAL_VERIFY=$(awk -F= '/^\[global\]/,/^\[/ {if ($1 == "verify") print $2}' "$config_file" | tr -d ' ')

    print_info "Global settings:"
    print_info "  Duration: ${GLOBAL_DURATION}s"
    print_info "  File size: ${GLOBAL_FILESIZE}MB"
    print_info "  Workers: $GLOBAL_WORKERS"
    print_info "  Block sizes: $GLOBAL_BLOCK_SIZES"
    print_info "  Test types: $GLOBAL_TEST_TYPES"
    print_info "  Mode: $GLOBAL_MODE"
    print_info "  Verify: $GLOBAL_VERIFY"
}

extract_job_configs() {
    local config_file=$1
    local job_section=$2

    local gpu=$(awk -F= "/^\[$job_section\]/,/^\[/ {if (\$1 == \"gpu\") print \$2}" "$config_file" | tr -d ' ')
    local device=$(awk -F= "/^\[$job_section\]/,/^\[/ {if (\$1 == \"device\") print \$2}" "$config_file" | tr -d ' ')
    local numa=$(awk -F= "/^\[$job_section\]/,/^\[/ {if (\$1 == \"numa\") print \$2}" "$config_file" | tr -d ' ')
    local workers=$(awk -F= "/^\[$job_section\]/,/^\[/ {if (\$1 == \"workers\") print \$2}" "$config_file" | tr -d ' ')

    # Use global values as defaults
    [ -z "$numa" ] && numa=-1
    [ -z "$workers" ] && workers=$GLOBAL_WORKERS

    echo "$gpu|$device|$numa|$workers"
}

get_job_list() {
    local config_file=$1

    # Extract all job sections
    grep "^\[job" "$config_file" | sed 's/\[\(.*\)\]/\1/'
}

run_pre_check() {
    print_header "Running System Pre-Check"

    if [ ! -f "${SCRIPT_DIR}/gds_check.sh" ]; then
        print_warn "gds_check.sh not found, skipping pre-check"
        return
    fi

    bash "${SCRIPT_DIR}/gds_check.sh" -p

    print_info "Pre-check complete"
}

run_job_benchmark() {
    local job_name=$1
    local gpu=$2
    local device=$3
    local numa=$4
    local workers=$5

    print_header "Running Job: $job_name"
    print_info "GPU: $gpu, Device: $device, NUMA: $numa, Workers: $workers"

    # Build command
    local cmd="${SCRIPT_DIR}/gds_bench.sh"
    cmd="$cmd -d $device"
    cmd="$cmd -g $gpu"
    cmd="$cmd -n $numa"
    cmd="$cmd -w $workers"
    cmd="$cmd -s $GLOBAL_FILESIZE"
    cmd="$cmd -t $GLOBAL_DURATION"
    cmd="$cmd -o $OUTPUT_DIR"

    # Add block sizes
    if [ ! -z "$GLOBAL_BLOCK_SIZES" ]; then
        cmd="$cmd -B $GLOBAL_BLOCK_SIZES"
    fi

    # Add test types
    if [ ! -z "$GLOBAL_TEST_TYPES" ]; then
        # Convert comma-separated to individual tests
        IFS=',' read -ra TYPES <<< "$GLOBAL_TEST_TYPES"
        for test_type in "${TYPES[@]}"; do
            test_type=$(echo "$test_type" | xargs)  # trim whitespace

            local job_cmd="$cmd -T $test_type"

            # Add mode
            if [ ! -z "$GLOBAL_MODE" ]; then
                job_cmd="$job_cmd -m $GLOBAL_MODE"
            fi

            # Add verification
            if [ "$GLOBAL_VERIFY" = "1" ]; then
                job_cmd="$job_cmd -V"
            fi

            print_info "Executing: $job_cmd"

            # Run benchmark
            if bash -c "$job_cmd"; then
                print_success "Completed test: $test_type for $job_name"
            else
                print_error "Failed test: $test_type for $job_name"
                FAILED_TESTS+=("$job_name:$test_type")
            fi

            # Brief pause between tests
            sleep 2
        done
    fi
}

generate_summary_report() {
    print_header "Generating Summary Report"

    local summary_file="${OUTPUT_DIR}/SUMMARY.md"

    cat > "$summary_file" << EOF
# GDS Benchmark Summary Report

**Generated:** $(date)
**Configuration:** $CONFIG_FILE

## Test Configuration

- **Duration:** ${GLOBAL_DURATION}s per test
- **File Size:** ${GLOBAL_FILESIZE}MB
- **Workers:** $GLOBAL_WORKERS
- **Block Sizes:** $GLOBAL_BLOCK_SIZES
- **Test Types:** $GLOBAL_TEST_TYPES
- **Mode:** $GLOBAL_MODE
- **Verification:** $([ "$GLOBAL_VERIFY" = "1" ] && echo "Enabled" || echo "Disabled")

## System Information

\`\`\`
$(cat "${OUTPUT_DIR}/system_info.txt" 2>/dev/null || echo "System info not available")
\`\`\`

## Test Results

EOF

    # Find all result files
    for result_file in "${OUTPUT_DIR}"/benchmark_results_*.log; do
        if [ -f "$result_file" ]; then
            echo "### $(basename "$result_file")" >> "$summary_file"
            echo "" >> "$summary_file"
            echo "\`\`\`" >> "$summary_file"
            tail -n 50 "$result_file" >> "$summary_file"
            echo "\`\`\`" >> "$summary_file"
            echo "" >> "$summary_file"
        fi
    done

    # Add FIO results if available
    if command -v jq &> /dev/null; then
        echo "## FIO Performance Summary" >> "$summary_file"
        echo "" >> "$summary_file"
        echo "| Test | Block Size | Bandwidth (MB/s) | IOPS | Latency (us) |" >> "$summary_file"
        echo "|------|------------|------------------|------|--------------|" >> "$summary_file"

        for fio_file in "${OUTPUT_DIR}"/fio_*.json; do
            if [ -f "$fio_file" ]; then
                test_name=$(basename "$fio_file" .json)
                bw=$(jq -r '.jobs[0].read.bw // .jobs[0].write.bw // 0' "$fio_file")
                bw_mb=$(echo "scale=2; $bw / 1024" | bc)
                iops=$(jq -r '.jobs[0].read.iops // .jobs[0].write.iops // 0' "$fio_file")
                lat=$(jq -r '.jobs[0].read.lat_ns.mean // .jobs[0].write.lat_ns.mean // 0' "$fio_file")
                lat_us=$(echo "scale=2; $lat / 1000" | bc)

                # Parse test name
                IFS='_' read -ra NAME_PARTS <<< "$test_name"
                test_type="${NAME_PARTS[1]:-unknown}"
                block_size="${NAME_PARTS[2]:-unknown}"

                echo "| $test_type | $block_size | $bw_mb | $iops | $lat_us |" >> "$summary_file"
            fi
        done
    fi

    # Add failed tests section
    if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
        echo "" >> "$summary_file"
        echo "## Failed Tests" >> "$summary_file"
        echo "" >> "$summary_file"
        for failed in "${FAILED_TESTS[@]}"; do
            echo "- $failed" >> "$summary_file"
        done
    fi

    print_success "Summary report generated: $summary_file"
}

# Main script
main() {
    CONFIG_FILE=""
    OUTPUT_DIR=""
    RUN_PRE_CHECK=0
    GENERATE_SUMMARY=0
    FAILED_TESTS=()

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--config)
                CONFIG_FILE="$2"
                shift 2
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -p|--pre-check)
                RUN_PRE_CHECK=1
                shift
                ;;
            -s|--summary)
                GENERATE_SUMMARY=1
                shift
                ;;
            *)
                echo "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Validate
    if [ -z "$CONFIG_FILE" ]; then
        echo "Error: Configuration file required (-c option)"
        usage
        exit 1
    fi

    # Set output directory
    if [ -z "$OUTPUT_DIR" ]; then
        TIMESTAMP=$(date +%Y%m%d_%H%M%S)
        CONFIG_NAME=$(basename "$CONFIG_FILE" .conf)
        OUTPUT_DIR="${RESULTS_BASE_DIR}/${CONFIG_NAME}_${TIMESTAMP}"
    fi

    mkdir -p "$OUTPUT_DIR"

    print_header "GDS Benchmark Runner"
    print_info "Configuration: $CONFIG_FILE"
    print_info "Output directory: $OUTPUT_DIR"

    # Run pre-check
    if [ $RUN_PRE_CHECK -eq 1 ]; then
        run_pre_check
    fi

    # Parse configuration
    parse_config_file "$CONFIG_FILE"

    # Get job list
    jobs=$(get_job_list "$CONFIG_FILE")

    if [ -z "$jobs" ]; then
        print_error "No jobs found in configuration file"
        exit 1
    fi

    print_info "Found jobs: $(echo $jobs | tr '\n' ' ')"

    # Run benchmarks for each job
    for job in $jobs; do
        job_config=$(extract_job_configs "$CONFIG_FILE" "$job")
        IFS='|' read -r gpu device numa workers <<< "$job_config"

        if [ -z "$device" ]; then
            print_warn "No device specified for $job, skipping"
            continue
        fi

        run_job_benchmark "$job" "$gpu" "$device" "$numa" "$workers"
    done

    # Generate summary
    if [ $GENERATE_SUMMARY -eq 1 ]; then
        generate_summary_report
    fi

    print_header "Benchmark Complete"
    print_success "Results saved to: $OUTPUT_DIR"

    if [ ${#FAILED_TESTS[@]} -gt 0 ]; then
        print_warn "Some tests failed:"
        for failed in "${FAILED_TESTS[@]}"; do
            print_warn "  - $failed"
        done
        exit 1
    else
        print_success "All tests passed!"
    fi
}

main "$@"
