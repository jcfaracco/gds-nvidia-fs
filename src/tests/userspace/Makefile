# NVFS Userspace Tests Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -D_GNU_SOURCE
TARGET_DIR = .
BINARIES = nvfs_proc_tests nvfs_device_tests

# Default target
all: $(BINARIES)

# Individual test binaries
nvfs_proc_tests: nvfs_proc_tests.c
	$(CC) $(CFLAGS) -o $(TARGET_DIR)/nvfs_proc_tests nvfs_proc_tests.c

nvfs_device_tests: nvfs_device_tests.c
	$(CC) $(CFLAGS) -o $(TARGET_DIR)/nvfs_device_tests nvfs_device_tests.c

# Run all tests
test: all
	@echo "Running NVFS userspace tests..."
	@echo "================================"
	@for test in $(BINARIES); do \
		echo "Running $$test..."; \
		./$$test || echo "$$test failed"; \
		echo ""; \
	done

# Run tests with verbose output
test-verbose: all
	@echo "Running NVFS userspace tests (verbose)..."
	@echo "========================================="
	@for test in $(BINARIES); do \
		echo "=== Running $$test ==="; \
		./$$test; \
		echo "=== $$test completed ==="; \
		echo ""; \
	done

# Install tests to system location (optional)
install: all
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BINARIES) $(DESTDIR)/usr/local/bin/

# Clean build artifacts
clean:
	rm -f $(BINARIES)
	rm -f *.o

# Help target
help:
	@echo "NVFS Userspace Tests Makefile"
	@echo "=============================="
	@echo "Available targets:"
	@echo "  all           - Build all test binaries"
	@echo "  test          - Build and run all tests"
	@echo "  test-verbose  - Build and run all tests with verbose output"
	@echo "  clean         - Remove build artifacts"
	@echo "  install       - Install tests to system location"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Individual test targets:"
	@echo "  nvfs_proc_tests    - Build proc filesystem tests"
	@echo "  nvfs_device_tests  - Build device file tests"

.PHONY: all test test-verbose clean install help
