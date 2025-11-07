# SPDX-License-Identifier: GPL-2.0
# NVFS Test Suite Makefile

# Kernel build variables
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Include subdirectories
obj-y += kunit/
obj-y += selftests/

# Default targets
all: kunit selftests userspace

# Build KUnit tests
kunit:
	@echo "Building KUnit tests..."
	$(MAKE) -C $(KDIR) M=$(PWD)/kunit modules

# Build selftests  
selftests:
	@echo "Building selftests..."
	$(MAKE) -C $(PWD)/selftests

# Build userspace tests
userspace:
	@echo "Building userspace tests..."
	$(MAKE) -C $(PWD)/userspace

# Clean all build artifacts
clean:
	@echo "Cleaning KUnit tests..."
	$(MAKE) -C $(KDIR) M=$(PWD)/kunit clean
	@echo "Cleaning selftests..."
	$(MAKE) -C $(PWD)/selftests clean
	@echo "Cleaning userspace tests..."
	$(MAKE) -C $(PWD)/userspace clean
	rm -f *.o *.ko *.mod.c *.mod *.order *.symvers

# Run KUnit tests
test-kunit:
	@echo "Running KUnit tests..."
	@echo "Use: kunit.py run --kunitconfig=src/tests/kunit/"
	@echo "Or build with CONFIG_NVFS_KUNIT_TEST=y and boot kernel"

# Run selftests
test-selftests:
	@echo "Running selftests..."
	$(MAKE) -C $(PWD)/selftests test

# Run userspace tests
test-userspace:
	@echo "Running userspace tests..."
	$(MAKE) -C $(PWD)/userspace test

# Install all test modules
install: kunit selftests userspace
	@echo "Installing KUnit test modules..."
	find kunit/ -name "*.ko" -exec sudo insmod {} \; || true
	@echo "Installing selftest modules..."
	$(MAKE) -C $(PWD)/selftests install
	@echo "Installing userspace test binaries..."
	$(MAKE) -C $(PWD)/userspace install

# Uninstall all test modules
uninstall:
	@echo "Uninstalling test modules..."
	sudo rmmod nvfs_core_kunit || true
	sudo rmmod nvfs_stress_kunit || true
	$(MAKE) -C $(PWD)/selftests uninstall || true

# Show help
help:
	@echo "NVFS Test Suite Makefile"
	@echo ""
	@echo "Test Types:"
	@echo "  KUnit Tests    - Modern kernel unit testing framework"
	@echo "  Selftests      - Original debugfs-based self-tests"
	@echo "  Userspace Tests- Userspace interface and integration tests"
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build all tests (KUnit + selftests + userspace)"
	@echo "  kunit         - Build KUnit tests only"
	@echo "  selftests     - Build selftests only"
	@echo "  userspace     - Build userspace tests only"
	@echo "  clean         - Clean all build artifacts"
	@echo "  test-kunit    - Run KUnit tests"
	@echo "  test-selftests- Run selftests"
	@echo "  test-userspace- Run userspace tests"
	@echo "  install       - Install all test modules"
	@echo "  uninstall     - Uninstall all test modules"
	@echo "  help          - Show this help"
	@echo ""
	@echo "KUnit Test Usage:"
	@echo "  # Build kernel with tests"
	@echo "  CONFIG_NVFS_KUNIT_TEST=y"
	@echo "  CONFIG_NVFS_KUNIT_TEST_CORE=y"
	@echo "  CONFIG_NVFS_KUNIT_TEST_STRESS=y"
	@echo ""
	@echo "  # Run with kunit tool"
	@echo "  kunit.py run --kunitconfig=src/tests/kunit/"
	@echo ""
	@echo "Selftest Usage:"
	@echo "  make test-selftests"
	@echo "  echo 'all' > /sys/kernel/debug/nvfs_test/run_tests"
	@echo ""
	@echo "Userspace Test Usage:"
	@echo "  make test-userspace"
	@echo "  cd userspace && ./nvfs_proc_tests"
	@echo "  cd userspace && ./nvfs_device_tests"

.PHONY: all kunit selftests userspace clean test-kunit test-selftests test-userspace install uninstall help