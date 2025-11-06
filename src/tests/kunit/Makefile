# SPDX-License-Identifier: GPL-2.0

# NVFS KUnit Tests Makefile

KVER ?= $(shell uname -r)

# Try to get it based on available nvidia module version (just in case there are sources for couple of versions)
nv_version=$(shell /sbin/modinfo -F version -k $(KVER) nvidia 2>/dev/null)
nv_sources=$(shell /bin/ls -d /usr/src/nvidia-$(nv_version)/ 2>/dev/null)
ifneq ($(shell test -d "$(nv_sources)" && echo "true" || echo "" ),)
        NVIDIA_SRC_DIR ?= $(shell find "$(nv_sources)" -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
else
        NVIDIA_SRC_DIR ?= $(shell find /usr/src/nvidia-* -name "nv-p2p.h"|head -1|xargs dirname || echo "NVIDIA_DRIVER_MISSING")
endif

# Include path for NVFS headers
ccflags-y += -I$(src)/../.. -I$(NVIDIA_SRC_DIR) -Werror

# Core functionality tests
obj-$(CONFIG_NVFS_KUNIT_TEST_CORE) += nvfs_core_kunit.o

# Stress and memory pressure tests  
obj-$(CONFIG_NVFS_KUNIT_TEST_STRESS) += nvfs_stress_kunit.o
