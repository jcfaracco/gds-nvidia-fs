#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# NVFS Self-Test Runner Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_MODULE="nvfs_selftest"
DEBUGFS_PATH="/sys/kernel/debug/nvfs_test/run_tests"
RESULTS_PATH="/sys/kernel/debug/nvfs_test/run_tests"

declare -a KUNIT_TESTS=("core" "stress")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

# Check if debugfs is mounted
check_debugfs() {
    if ! mount | grep -q debugfs; then
        log_warning "debugfs not mounted, attempting to mount..."
        mount -t debugfs none /sys/kernel/debug || {
            log_error "Failed to mount debugfs"
            exit 1
        }
        log_success "debugfs mounted successfully"
    fi
}

# Build test module
build_tests() {
    log_info "Building NVFS self-tests..."
    cd "$SCRIPT_DIR"/..
    make clean >/dev/null 2>&1 || true
    if CONFIG_KUNIT=y CONFIG_NVFS_KUNIT_TEST_CORE=m CONFIG_NVFS_KUNIT_TEST_STRESS=m make all; then
        log_success "Test module built successfully"
    else
        log_error "Failed to build test module"
        exit 1
    fi
    cd -
}

# Load test module
load_selftests_module() {
    log_info "Loading test module..."
    
    # Unload if already loaded
    if lsmod | grep -q "$TEST_MODULE"; then
        log_info "Test module already loaded, unloading first..."
        rmmod "$TEST_MODULE" || true
    fi
    
    if insmod selftests/"${TEST_MODULE}.ko"; then
        log_success "Test module loaded successfully"
    else
        log_error "Failed to load test module"
        exit 1
    fi
    
    # Wait for debugfs entries to appear
    sleep 1
    
    if [[ ! -f "$DEBUGFS_PATH" ]]; then
        log_error "Test interface not available at $DEBUGFS_PATH"
        exit 1
    fi
}

# Load kunit test modules
load_kunit_modules() {
    for TEST in $KUNIT_TESTS; do
        log_info "Loading ${TEST} test module..."

        # Unload if already loaded
        if lsmod | grep -q "nvfs_${TEST}_kunit"; then
            log_info "Test module nvfs_${TEST}_kunit already loaded, unloading first..."
            rmmod "nvfs_${TEST}_kunit" || true
        fi

        if insmod kunit/"nvfs_${TEST}_kunit.ko"; then
            log_success "Test module loaded successfully"
        else
            log_error "Failed to load test module"
            exit 1
        fi
    done
}

# Load all test modules
load_modules() {
    load_selftests_module
    load_kunit_modules
}

# Unload test module
unload_selftests_module() {
    log_info "Unloading test module..."
    if lsmod | grep -q "$TEST_MODULE"; then
        rmmod "$TEST_MODULE" || {
            log_warning "Failed to unload test module cleanly"
        }
    fi
}

# Unload test module
unload_kunit_modules() {
    for TEST in $KUNIT_TESTS; do
        log_info "Unloading $TEST test module..."
        if lsmod | grep -q "nvfs_${TEST}_kunit"; then
            rmmod "nvfs_${TEST}_kunit" || {
                log_warning "Failed to unload test nvfs_${TEST}_kunit module cleanly"
            }
        fi
    done
}

# Unload test modules
unload_modules() {
    unload_selftests_module
    unload_kunit_modules
}

# Run specific test suite
run_test_suite() {
    local suite="$1"
    log_info "Running $suite tests..."
    
    echo "$suite" > "$DEBUGFS_PATH" || {
        log_error "Failed to trigger $suite tests"
        return 1
    }
    
    # Give tests time to complete
    sleep 2
    
    log_success "$suite tests completed"
}

# Show test results
show_results() {
    log_info "Test Results:"
    echo "=============================================="
    cat "$RESULTS_PATH" 2>/dev/null || {
        log_error "Could not read test results"
        return 1
    }
    echo "=============================================="
}

# Extract test summary from dmesg
show_dmesg_results() {
    log_info "Recent test output from kernel log:"
    echo "=============================================="
    dmesg | grep "NVFS_TEST:" | tail -20
    echo "=============================================="
}

# Main test execution
run_tests() {
    local test_suite="${1:-all}"
    
    check_root
    check_debugfs
    build_tests
    load_modules
    
    log_info "Starting NVFS self-tests..."
    
    case "$test_suite" in
        "all")
            run_test_suite "all"
            run_userspace_tests
            ;;
        "core")
            run_test_suite "core"
            ;;
        "mmap")
            run_test_suite "mmap"
            ;;
        "dma")
            run_test_suite "dma"
            ;;
        "memory")
            run_test_suite "memory"
            ;;
        "stress")
            run_test_suite "stress"
            ;;
        "userspace")
            # Skip kernel tests, run only userspace
            unload_module
            run_userspace_tests
            return 0
            ;;
        *)
            log_error "Unknown test suite: $test_suite"
            log_info "Available test suites: all, core, mmap, dma, memory, stress, userspace"
            exit 1
            ;;
    esac
    
    show_results
    show_dmesg_results
    
    # Cleanup
    unload_modules
    
    log_success "NVFS self-tests completed"
}

# Show usage information
show_usage() {
    echo "NVFS Self-Test Runner"
    echo ""
    echo "Usage: $0 [OPTIONS] [TEST_SUITE]"
    echo ""
    echo "Test Suites:"
    echo "  all      - Run all test suites (default)"
    echo "  core     - Run core functionality tests"
    echo "  mmap     - Run memory mapping tests"
    echo "  dma      - Run DMA functionality tests"
    echo "  memory   - Run memory management tests"
    echo "  stress   - Run stress and edge case tests"
    echo ""
    echo "Options:"
    echo "  -h, --help     Show this help message"
    echo "  -b, --build    Build tests only (don't run)"
    echo "  -c, --clean    Clean build artifacts"
    echo "  -u, --unload   Unload test module"
    echo ""
    echo "Examples:"
    echo "  $0                 # Run all tests"
    echo "  $0 core            # Run core tests only"
    echo "  $0 --build         # Build tests without running"
    echo "  $0 --clean         # Clean build artifacts"
}

# Parse command line arguments
case "${1:-}" in
    -h|--help)
        show_usage
        exit 0
        ;;
    -b|--build)
        check_root
        build_tests
        exit 0
        ;;
    -c|--clean)
        cd "$SCRIPT_DIR"
        make clean
        # Also clean userspace tests
        userspace_dir="$(dirname "$SCRIPT_DIR")/userspace"
        if [[ -d "$userspace_dir" ]]; then
            cd "$userspace_dir"
            make clean >/dev/null 2>&1 || true
        fi
        log_success "Build artifacts cleaned"
        exit 0
        ;;
    -u|--unload)
        check_root
        unload_module
        exit 0
        ;;
    --userspace)
        run_userspace_tests
        exit 0
        ;;
    "")
        run_tests "all"
        ;;
    *)
        run_tests "$1"
        ;;
esac
