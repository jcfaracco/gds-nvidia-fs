# SPDX-License-Identifier: GPL-2.0
# NVFS Selftests Makefile

# Kernel build variables
KDIR ?= /lib/modules/$(shell uname -r)/build
KVER ?= $(shell uname -r)
PWD := $(shell pwd)

# Try to get it based on available nvidia module version (just in case there are sources for couple of versions)
nv_version=$(shell /sbin/modinfo -F version -k $(KVER) nvidia 2>/dev/null)
nv_sources=$(shell /bin/ls -d /usr/src/nvidia-$(nv_version)/ 2>/dev/null)
ifneq ($(shell test -d "$(nv_sources)" && echo "true" || echo "" ),)
        NVIDIA_SRC_DIR ?= $(shell find "$(nv_sources)" -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
else
        NVIDIA_SRC_DIR ?= $(shell find /usr/src/nvidia-* -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
endif

# Test module name
TEST_MODULE := nvfs_selftest

# Source files
TEST_SOURCES := nvfs_test_framework.c \
                nvfs_core_tests.c \
                nvfs_stress_tests.c \
                nvfs_stub_tests.c

# Object files
TEST_OBJECTS := $(TEST_SOURCES:.c=.o)

# Module object
obj-m := $(TEST_MODULE).o
$(TEST_MODULE)-objs := $(TEST_OBJECTS)

# Compiler flags
ccflags-y := -I$(src)/../.. -DDEBUG -Werror -Wno-declaration-after-statement -I$(NVIDIA_SRC_DIR)

# Default target
all: modules

# Build kernel modules
modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build artifacts
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.order *.symvers

# Install test module
install: modules
	sudo insmod $(TEST_MODULE).ko
	@echo "NVFS selftest module loaded"
	@echo "Run tests using: echo 'all' > /sys/kernel/debug/nvfs_test/run_tests"

# Remove test module
uninstall:
	sudo rmmod $(TEST_MODULE) || true
	@echo "NVFS selftest module unloaded"

# Run all tests (requires module to be loaded)
test: install
	@echo "Running all NVFS selftests..."
	@echo 'all' | sudo tee /sys/kernel/debug/nvfs_test/run_tests > /dev/null
	@echo "Check dmesg for test results"

# Run specific test suite
test-core: install
	@echo "Running NVFS core tests..."
	@echo 'core' | sudo tee /sys/kernel/debug/nvfs_test/run_tests > /dev/null

test-stress: install
	@echo "Running NVFS stress tests..."
	@echo 'stress' | sudo tee /sys/kernel/debug/nvfs_test/run_tests > /dev/null

# Show test results
results:
	@echo "=== NVFS Selftest Results ==="
	@cat /sys/kernel/debug/nvfs_test/run_tests 2>/dev/null || echo "Test module not loaded or debugfs not available"

# Help target
help:
	@echo "NVFS Selftests Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build test modules"
	@echo "  clean      - Clean build artifacts"
	@echo "  install    - Load test module"
	@echo "  uninstall  - Unload test module"
	@echo "  test       - Run all tests"
	@echo "  test-core  - Run core tests only"
	@echo "  test-stress- Run stress tests only"
	@echo "  results    - Show test results"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Manual test execution:"
	@echo "  echo 'all' > /sys/kernel/debug/nvfs_test/run_tests"
	@echo "  echo 'core' > /sys/kernel/debug/nvfs_test/run_tests"
	@echo "  echo 'stress' > /sys/kernel/debug/nvfs_test/run_tests"

.PHONY: all modules clean install uninstall test test-core test-stress results help
