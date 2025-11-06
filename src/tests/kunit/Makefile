# SPDX-License-Identifier: GPL-2.0

# NVFS KUnit Tests Makefile

# Include path for NVFS headers
ccflags-y += -I$(src)/../..

# Core functionality tests
obj-$(CONFIG_NVFS_KUNIT_TEST_CORE) += nvfs_core_kunit.o

# Stress and memory pressure tests  
obj-$(CONFIG_NVFS_KUNIT_TEST_STRESS) += nvfs_stress_kunit.o