/* SPDX-License-Identifier: GPL-2.0 */
/*
 * NVFS Kernel Module Self-Test Framework
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#ifndef NVFS_TEST_H
#define NVFS_TEST_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

/* Test result codes */
#define NVFS_TEST_PASS    0
#define NVFS_TEST_FAIL    -1
#define NVFS_TEST_SKIP    -2

/* Test framework macros */
#define NVFS_TEST_ASSERT(condition, msg) do { \
	if (!(condition)) { \
		pr_err("NVFS_TEST_FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); \
		return NVFS_TEST_FAIL; \
	} \
} while (0)

#define NVFS_TEST_ASSERT_EQ(expected, actual, msg) do { \
	if ((expected) != (actual)) { \
		pr_err("NVFS_TEST_FAIL: %s - expected %ld, got %ld at %s:%d\n", \
		       msg, (long)(expected), (long)(actual), __FILE__, __LINE__); \
		return NVFS_TEST_FAIL; \
	} \
} while (0)

#define NVFS_TEST_ASSERT_NULL(ptr, msg) do { \
	if ((ptr) != NULL) { \
		pr_err("NVFS_TEST_FAIL: %s - expected NULL, got %p at %s:%d\n", \
		       msg, ptr, __FILE__, __LINE__); \
		return NVFS_TEST_FAIL; \
	} \
} while (0)

#define NVFS_TEST_ASSERT_NOT_NULL(ptr, msg) do { \
	if ((ptr) == NULL) { \
		pr_err("NVFS_TEST_FAIL: %s - expected non-NULL, got NULL at %s:%d\n", \
		       msg, __FILE__, __LINE__); \
		return NVFS_TEST_FAIL; \
	} \
} while (0)

/* Test function signature */
typedef int (*nvfs_test_func_t)(void);

/* Test case structure */
struct nvfs_test_case {
	const char *name;
	nvfs_test_func_t test_func;
	const char *description;
};

/* Test suite structure */
struct nvfs_test_suite {
	const char *name;
	struct nvfs_test_case *tests;
	int num_tests;
	int (*setup)(void);
	void (*teardown)(void);
};

/* Test statistics */
struct nvfs_test_stats {
	int total_tests;
	int passed_tests;
	int failed_tests;
	int skipped_tests;
	unsigned long start_time;
	unsigned long end_time;
};

/* Test framework functions */
int nvfs_run_test_suite(struct nvfs_test_suite *suite);
int nvfs_run_all_tests(void);
void nvfs_test_print_results(struct nvfs_test_stats *stats);

/* Test registration macros */
#define NVFS_TEST_DECLARE(test_name) \
	static int test_##test_name(void)

#define NVFS_TEST_CASE(test_name, desc) \
	{ .name = #test_name, .test_func = test_##test_name, .description = desc }

/* Mock/stub framework for testing */
struct nvfs_test_mock {
	void *original_func;
	void *mock_func;
	int call_count;
	void *last_args;
};

/* External test suite declarations */
extern struct nvfs_test_suite nvfs_core_test_suite;
extern struct nvfs_test_suite nvfs_mmap_test_suite;
extern struct nvfs_test_suite nvfs_dma_test_suite;
extern struct nvfs_test_suite nvfs_memory_test_suite;
extern struct nvfs_test_suite nvfs_stress_test_suite;

#endif /* NVFS_TEST_H */