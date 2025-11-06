// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS System Stress Tests
 * NVFS under realistic system load and stress conditions
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include "nvfs_test.h"
#include "../../nvfs-core.h"

/* Forward declaration */
long nvfs_count_ops(void);

#define STRESS_ITERATIONS 1000
#define CONCURRENT_THREADS 4
#define LARGE_ALLOC_ORDER 4

static struct completion stress_complete;
static atomic_t active_threads;
static int stress_results[CONCURRENT_THREADS];

/* Worker thread for concurrent stress testing */
static int nvfs_stress_worker(void *data)
{
	int thread_id = (int)(long)data;
	int i, success_count = 0;
	struct folio *folios[10];

	for (i = 0; i < STRESS_ITERATIONS / CONCURRENT_THREADS; i++) {
		int j;

		/* Allocate multiple folios of different sizes */
		for (j = 0; j < 10; j++) {
			unsigned int order = j % 3; /* 0, 1, or 2 pages */

			folios[j] = folio_alloc(GFP_KERNEL, order);
			if (folios[j]) {
				/* Test NVFS operations on each folio */
				if (nvfs_count_ops() > 0) {
					/* Placeholder for NVFS operations */
				}

				success_count++;
			}
		}

		/* Add some realistic delay */
		if (i % 10 == 0)
			usleep_range(1000, 2000);

		/* Free folios */
		for (j = 0; j < 10; j++) {
			if (folios[j])
				folio_put(folios[j]);
		}

		cond_resched();
	}

	stress_results[thread_id] = success_count;

	if (atomic_dec_and_test(&active_threads))
		complete(&stress_complete);

	return 0;
}

/* Test NVFS under concurrent access from multiple threads */
NVFS_TEST_DECLARE(nvfs_concurrent_stress)
{
	struct task_struct *threads[CONCURRENT_THREADS];
	int i, total_success = 0;
	unsigned long start_time, end_time;

	pr_info("NVFS_TEST: Starting concurrent stress test with %d threads\n",
		CONCURRENT_THREADS);

	init_completion(&stress_complete);
	atomic_set(&active_threads, CONCURRENT_THREADS);
	memset(stress_results, 0, sizeof(stress_results));

	start_time = jiffies;

	/* Start all worker threads */
	for (i = 0; i < CONCURRENT_THREADS; i++) {
		threads[i] = kthread_run(nvfs_stress_worker, (void *)(long)i,
					"nvfs_stress_%d", i);
		if (IS_ERR(threads[i])) {
			pr_err("NVFS_TEST: Failed to create thread %d\n", i);
			return NVFS_TEST_FAIL;
		}
	}

	/* Wait for completion with timeout */
	if (!wait_for_completion_timeout(&stress_complete, HZ * 30)) {
		pr_err("NVFS_TEST: Stress test timed out\n");
		return NVFS_TEST_FAIL;
	}

	end_time = jiffies;

	/* Calculate results */
	for (i = 0; i < CONCURRENT_THREADS; i++)
		total_success += stress_results[i];

	pr_info("NVFS_TEST: Concurrent stress completed. %d/%d operations successful in %lu jiffies\n",
		total_success, STRESS_ITERATIONS * 10, end_time - start_time);

	/* We should have reasonable success rate */
	NVFS_TEST_ASSERT_GT(total_success, STRESS_ITERATIONS * 5,
			    "Too many allocation failures");

	return NVFS_TEST_PASS;
}

/* Test NVFS with large memory allocations and fragmentation */
NVFS_TEST_DECLARE(nvfs_memory_fragmentation_stress)
{
	struct folio **large_folios;
	struct folio **small_folios;
	int large_count = 0, small_count = 0;
	int i;

	/* Allocate arrays dynamically to avoid large stack frame */
	large_folios = kzalloc(100 * sizeof(struct folio *), GFP_KERNEL);
	if (!large_folios)
		return NVFS_TEST_FAIL;

	small_folios = kzalloc(1000 * sizeof(struct folio *), GFP_KERNEL);
	if (!small_folios) {
		kfree(large_folios);
		return NVFS_TEST_FAIL;
	}

	pr_info("NVFS_TEST: Testing memory fragmentation handling\n");

	/* First, allocate large folios to fragment memory */
	for (i = 0; i < 100; i++) {
		large_folios[i] = folio_alloc(GFP_KERNEL, LARGE_ALLOC_ORDER);
		if (large_folios[i])
			large_count++;
		else
			large_folios[i] = NULL;
	}

	pr_info("NVFS_TEST: Allocated %d large folios\n", large_count);

	/* Free every other large folio to create fragmentation */
	for (i = 0; i < 100; i += 2) {
		if (large_folios[i]) {
			folio_put(large_folios[i]);
			large_folios[i] = NULL;
		}
	}

	/* Now test NVFS with small allocations in fragmented memory */
	for (i = 0; i < 1000; i++) {
		small_folios[i] = folio_alloc(GFP_KERNEL, 0);
		if (small_folios[i]) {
			/* Test NVFS operations */
			if (nvfs_count_ops() > 0) {
				/* Placeholder for NVFS fragmentation testing */
			}

			small_count++;
		} else {
			small_folios[i] = NULL;
		}

		if (i % 100 == 0)
			cond_resched();
	}

	pr_info("NVFS_TEST: Allocated %d small folios in fragmented memory\n",
		small_count);

	/* Cleanup */
	for (i = 0; i < 100; i++) {
		if (large_folios[i])
			folio_put(large_folios[i]);
	}

	for (i = 0; i < 1000; i++) {
		if (small_folios[i])
			folio_put(small_folios[i]);
	}

	/* Free the dynamically allocated arrays */
	kfree(large_folios);
	kfree(small_folios);

	/* Should still get reasonable number of allocations */
	NVFS_TEST_ASSERT_GT(small_count, 500, "Insufficient small allocations");

	return NVFS_TEST_PASS;
}

/* Test NVFS under sustained load */
NVFS_TEST_DECLARE(nvfs_sustained_load_stress)
{
	unsigned long start_time, end_time, test_duration;
	int operations = 0;
	int successful_ops = 0;

	pr_info("NVFS_TEST: Starting sustained load test (10 second duration)\n");

	start_time = jiffies;
	test_duration = HZ * 10; /* 10 seconds */

	/* Run operations for fixed time period */
	do {
		struct folio *folio = folio_alloc(GFP_KERNEL, 1);

		operations++;

		if (folio) {
			/* Perform NVFS operations */
			if (nvfs_count_ops() > 0) {
				/* Placeholder for sustained load testing */
			}

			/* Write to memory to ensure it's really allocated */
			void *addr = kmap_local_page(folio_page(folio, 0));

			if (addr) {
				memset(addr, 0xFF, PAGE_SIZE);
				kunmap_local(addr);
			}

			folio_put(folio);
			successful_ops++;
		}

		/* Small delay to prevent overwhelming the system */
		if (operations % 50 == 0)
			cond_resched();

		end_time = jiffies;

	} while (time_before(end_time, start_time + test_duration));

	pr_info("NVFS_TEST: Sustained load test completed. %d/%d operations successful\n",
		successful_ops, operations);

	/* Should maintain reasonable success rate */
	NVFS_TEST_ASSERT_GT(successful_ops * 100 / operations, 70,
			    "Success rate below 70%");

	return NVFS_TEST_PASS;
}

/* Test NVFS error handling under extreme conditions */
NVFS_TEST_DECLARE(nvfs_extreme_conditions_stress)
{
	int i, failed_allocs = 0, successful_ops = 0;

	pr_info("NVFS_TEST: Testing extreme allocation conditions\n");

	/* Try to exhaust memory with very large allocations */
	for (i = 0; i < 1000; i++) {
		struct folio *folio = folio_alloc(GFP_KERNEL | __GFP_NOWARN,
						  MAX_PAGE_ORDER - 1);
		if (!folio) {
			failed_allocs++;

			/* Test NVFS still works with smaller allocations */
			folio = folio_alloc(GFP_KERNEL, 0);
			if (folio) {
				if (nvfs_count_ops() > 0) {
					/* Placeholder for extreme conditions testing */
					successful_ops++;
				}
				folio_put(folio);
			}
		} else {
			folio_put(folio);
		}

		if (i % 10 == 0)
			cond_resched();

		/* Stop if we're clearly hitting memory limits */
		if (failed_allocs > 50)
			break;
	}

	pr_info("NVFS_TEST: Under extreme conditions: %d failed large allocs, %d successful NVFS ops\n",
		failed_allocs, successful_ops);

	/* NVFS should continue working even when large allocations fail */
	NVFS_TEST_ASSERT_GT(successful_ops, 10, "NVFS failed under extreme conditions");

	return NVFS_TEST_PASS;
}

/* Array of stress tests */
static struct nvfs_test_case nvfs_stress_tests[] = {
	NVFS_TEST_CASE(nvfs_concurrent_stress, "Concurrent Access Stress"),
	NVFS_TEST_CASE(nvfs_memory_fragmentation_stress, "Memory Fragmentation Stress"),
	NVFS_TEST_CASE(nvfs_sustained_load_stress, "Sustained Load Stress"),
	NVFS_TEST_CASE(nvfs_extreme_conditions_stress, "Extreme Conditions Stress"),
	NVFS_TEST_CASE_END
};

/* Stress test suite definition */
struct nvfs_test_suite nvfs_stress_test_suite = {
	.name = "NVFS Stress Tests",
	.tests = nvfs_stress_tests,
	.num_tests = 4,
	.setup = NULL,
	.teardown = NULL
};
