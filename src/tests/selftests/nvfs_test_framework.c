// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS Kernel Module Self-Test Framework Implementation
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include "nvfs_test.h"

/* Forward declaration */
long nvfs_count_ops(void);

/* Weak stub for nvfs_count_ops when main NVFS module is not loaded */
long __weak nvfs_count_ops(void)
{
	/* Return 1 to indicate stub/test mode */
	return 1;
}

static struct dentry *nvfs_test_dir;
static struct dentry *nvfs_test_results_file;
static struct nvfs_test_stats global_stats;

/* Test framework implementation */
int nvfs_run_test_suite(struct nvfs_test_suite *suite)
{
	struct nvfs_test_stats stats = {0};
	int i, result;

	if (!suite) {
		pr_err("NVFS_TEST: NULL test suite\n");
		return -EINVAL;
	}

	pr_info("NVFS_TEST: Running test suite: %s\n", suite->name);
	stats.start_time = jiffies;

	/* Run setup if provided */
	if (suite->setup) {
		result = suite->setup();
		if (result != 0) {
			pr_err("NVFS_TEST: Setup failed for suite %s: %d\n",
			       suite->name, result);
			return result;
		}
	}

	/* Run all tests in the suite */
	for (i = 0; i < suite->num_tests; i++) {
		struct nvfs_test_case *test = &suite->tests[i];

		pr_info("NVFS_TEST: Running test: %s - %s\n",
			test->name, test->description);

		result = test->test_func();
		stats.total_tests++;

		switch (result) {
		case NVFS_TEST_PASS:
			stats.passed_tests++;
			pr_info("NVFS_TEST: PASS - %s\n", test->name);
			break;
		case NVFS_TEST_FAIL:
			stats.failed_tests++;
			pr_err("NVFS_TEST: FAIL - %s\n", test->name);
			break;
		case NVFS_TEST_SKIP:
			stats.skipped_tests++;
			pr_info("NVFS_TEST: SKIP - %s\n", test->name);
			break;
		default:
			stats.failed_tests++;
			pr_err("NVFS_TEST: FAIL - %s (unknown result: %d)\n",
			       test->name, result);
			break;
		}
	}

	/* Run teardown if provided */
	if (suite->teardown)
		suite->teardown();

	stats.end_time = jiffies;
	nvfs_test_print_results(&stats);

	/* Update global statistics */
	global_stats.total_tests += stats.total_tests;
	global_stats.passed_tests += stats.passed_tests;
	global_stats.failed_tests += stats.failed_tests;
	global_stats.skipped_tests += stats.skipped_tests;

	return stats.failed_tests > 0 ? -1 : 0;
}

void nvfs_test_print_results(struct nvfs_test_stats *stats)
{
	unsigned long elapsed = stats->end_time - stats->start_time;

	pr_info("NVFS_TEST: ========== TEST RESULTS ==========\n");
	pr_info("NVFS_TEST: Total tests: %d\n", stats->total_tests);
	pr_info("NVFS_TEST: Passed: %d\n", stats->passed_tests);
	pr_info("NVFS_TEST: Failed: %d\n", stats->failed_tests);
	pr_info("NVFS_TEST: Skipped: %d\n", stats->skipped_tests);
	pr_info("NVFS_TEST: Elapsed time: %lu jiffies (%lu ms)\n",
		elapsed, (unsigned long)jiffies_to_msecs(elapsed));
	pr_info("NVFS_TEST: =================================\n");
}

/* Debugfs interface for running tests */
static ssize_t nvfs_test_write(struct file *file, const char __user *buf,
			       size_t count, loff_t *ppos)
{
	char cmd[32];
	int ret;

	if (count >= sizeof(cmd))
		return -EINVAL;

	if (copy_from_user(cmd, buf, count))
		return -EFAULT;

	cmd[count] = '\0';

	/* Reset global stats */
	memset(&global_stats, 0, sizeof(global_stats));
	global_stats.start_time = jiffies;

	if (strncmp(cmd, "all", 3) == 0) {
		ret = nvfs_run_all_tests();
	} else if (strncmp(cmd, "core", 4) == 0) {
		ret = nvfs_run_test_suite(&nvfs_core_test_suite);
	} else if (strncmp(cmd, "mmap", 4) == 0) {
		ret = nvfs_run_test_suite(&nvfs_mmap_test_suite);
	} else if (strncmp(cmd, "dma", 3) == 0) {
		ret = nvfs_run_test_suite(&nvfs_dma_test_suite);
	} else if (strncmp(cmd, "memory", 6) == 0) {
		ret = nvfs_run_test_suite(&nvfs_memory_test_suite);
	} else if (strncmp(cmd, "stress", 6) == 0) {
		ret = nvfs_run_test_suite(&nvfs_stress_test_suite);
	} else {
		pr_err("NVFS_TEST: Unknown command: %s\n", cmd);
		pr_info("NVFS_TEST: Available commands: all, core, mmap, dma, memory, stress\n");
		return -EINVAL;
	}

	global_stats.end_time = jiffies;

	return ret < 0 ? ret : count;
}

static int nvfs_test_show(struct seq_file *m, void *v)
{
	unsigned long elapsed = global_stats.end_time - global_stats.start_time;

	seq_puts(m, "NVFS Self-Test Results\n");
	seq_puts(m, "======================\n");
	seq_printf(m, "Total tests: %d\n", global_stats.total_tests);
	seq_printf(m, "Passed: %d\n", global_stats.passed_tests);
	seq_printf(m, "Failed: %d\n", global_stats.failed_tests);
	seq_printf(m, "Skipped: %d\n", global_stats.skipped_tests);
	seq_printf(m, "Success rate: %d%%\n",
		   global_stats.total_tests > 0 ?
		   (global_stats.passed_tests * 100) / global_stats.total_tests : 0);
	seq_printf(m, "Elapsed time: %lu jiffies (%lu ms)\n",
		   elapsed, (unsigned long)jiffies_to_msecs(elapsed));
	seq_puts(m, "\nUsage:\n");
	seq_puts(m, "  echo 'all' > /sys/kernel/debug/nvfs_test/run_tests\n");
	seq_puts(m, "  echo 'core' > /sys/kernel/debug/nvfs_test/run_tests\n");
	seq_puts(m, "  echo 'mmap' > /sys/kernel/debug/nvfs_test/run_tests\n");
	seq_puts(m, "  echo 'dma' > /sys/kernel/debug/nvfs_test/run_tests\n");
	seq_puts(m, "  echo 'memory' > /sys/kernel/debug/nvfs_test/run_tests\n");
	seq_puts(m, "  echo 'stress' > /sys/kernel/debug/nvfs_test/run_tests\n");

	return 0;
}

static int nvfs_test_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvfs_test_show, NULL);
}

static const struct file_operations nvfs_test_fops = {
	.open = nvfs_test_open,
	.read = seq_read,
	.write = nvfs_test_write,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Initialize test framework */
int nvfs_test_init(void)
{
	nvfs_test_dir = debugfs_create_dir("nvfs_test", NULL);
	if (IS_ERR(nvfs_test_dir)) {
		pr_err("NVFS_TEST: Failed to create debugfs directory\n");
		return PTR_ERR(nvfs_test_dir);
	}

	nvfs_test_results_file = debugfs_create_file("run_tests", 0644,
						     nvfs_test_dir, NULL,
						     &nvfs_test_fops);
	if (IS_ERR(nvfs_test_results_file)) {
		pr_err("NVFS_TEST: Failed to create debugfs file\n");
		debugfs_remove_recursive(nvfs_test_dir);
		return PTR_ERR(nvfs_test_results_file);
	}

	pr_info("NVFS_TEST: Test framework initialized\n");
	pr_info("NVFS_TEST: Use /sys/kernel/debug/nvfs_test/run_tests to run tests\n");

	return 0;
}

/* Cleanup test framework */
void nvfs_test_exit(void)
{
	debugfs_remove_recursive(nvfs_test_dir);
	pr_info("NVFS_TEST: Test framework cleanup complete\n");
}

/* Run all test suites */
int nvfs_run_all_tests(void)
{
	int ret = 0;

	pr_info("NVFS_TEST: Running all test suites\n");

	ret |= nvfs_run_test_suite(&nvfs_core_test_suite);
	ret |= nvfs_run_test_suite(&nvfs_mmap_test_suite);
	ret |= nvfs_run_test_suite(&nvfs_dma_test_suite);
	ret |= nvfs_run_test_suite(&nvfs_memory_test_suite);
	ret |= nvfs_run_test_suite(&nvfs_stress_test_suite);

	pr_info("NVFS_TEST: All test suites completed\n");
	nvfs_test_print_results(&global_stats);

	return ret;
}

/* Module initialization */
static int __init nvfs_selftest_init(void)
{
	int ret;

	pr_info("NVFS_TEST: Initializing NVFS selftest module\n");

	ret = nvfs_test_init();
	if (ret) {
		pr_err("NVFS_TEST: Failed to initialize test framework: %d\n", ret);
		return ret;
	}

	pr_info("NVFS_TEST: Selftest module loaded successfully\n");
	pr_info("NVFS_TEST: Available test suites: core, mmap, dma, memory, stress\n");
	return 0;
}

/* Module cleanup */
static void __exit nvfs_selftest_exit(void)
{
	pr_info("NVFS_TEST: Unloading NVFS selftest module\n");
	nvfs_test_exit();
	pr_info("NVFS_TEST: Selftest module unloaded\n");
}

module_init(nvfs_selftest_init);
module_exit(nvfs_selftest_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NVFS Self-Test Framework");
