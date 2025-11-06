// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit stress tests for NVFS memory management
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "../../nvfs-core.h"
#include "../../nvfs-mmap.h"

#define STRESS_TEST_ITERATIONS 100
#define STRESS_TEST_MAX_ORDER 3

/*
 * Test stress folio allocation and deallocation
 */
static void nvfs_stress_folio_allocation_test(struct kunit *test)
{
	struct folio *folios[STRESS_TEST_ITERATIONS];
	int i, allocated_count = 0;

	/* Allocate folios with varying orders */
	for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
		unsigned int order = i % (STRESS_TEST_MAX_ORDER + 1);

		folios[i] = folio_alloc(GFP_KERNEL, order);
		if (folios[i]) {
			allocated_count++;

			/* Verify allocation properties */
			KUNIT_EXPECT_EQ(test, 1 << order, folio_nr_pages(folios[i]));

			/* Add some variability */
			if (i % 10 == 0)
				cond_resched();
		}
	}

	/* We should have allocated most folios successfully */
	KUNIT_EXPECT_GT(test, allocated_count, STRESS_TEST_ITERATIONS / 2);

	/* Clean up all allocated folios */
	for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
		if (folios[i])
			folio_put(folios[i]);
	}
}

/*
 * Test concurrent kmap operations
 */
static void nvfs_stress_kmap_operations_test(struct kunit *test)
{
	struct folio *folios[10];
	void *kaddrs[10];
	int i, j;
	char test_pattern[10];

	/* Allocate multiple folios */
	for (i = 0; i < 10; i++) {
		folios[i] = folio_alloc(GFP_KERNEL, 0);
		KUNIT_ASSERT_NOT_NULL(test, folios[i]);
		kaddrs[i] = NULL;
	}

	/* Map all pages and write test patterns */
	for (i = 0; i < 10; i++) {
		struct page *page = folio_page(folios[i], 0);
		kaddrs[i] = kmap_local_page(page);
		KUNIT_ASSERT_NOT_NULL(test, kaddrs[i]);

		/* Write a unique pattern */
		test_pattern[i] = '0' + i;
		memset(kaddrs[i], test_pattern[i], PAGE_SIZE);
	}

	/* Verify patterns */
	for (i = 0; i < 10; i++) {
		char *data = (char *)kaddrs[i];
		KUNIT_EXPECT_EQ(test, test_pattern[i], data[0]);
		KUNIT_EXPECT_EQ(test, test_pattern[i], data[100]);
		KUNIT_EXPECT_EQ(test, test_pattern[i], data[PAGE_SIZE - 1]);
	}

	/* Unmap in reverse order */
	for (i = 9; i >= 0; i--) {
		kunmap_local(kaddrs[i]);
	}

	/* Clean up folios */
	for (i = 0; i < 10; i++) {
		folio_put(folios[i]);
	}
}

/*
 * Test edge case with maximum order allocation
 */
static void nvfs_edge_case_max_order_test(struct kunit *test)
{
	struct folio *folio;
	unsigned int max_order = MAX_ORDER - 1;

	/* Try to allocate maximum order folio */
	folio = folio_alloc(GFP_KERNEL, max_order);
	
	if (folio) {
		/* If successful, verify properties */
		KUNIT_EXPECT_EQ(test, 1 << max_order, folio_nr_pages(folio));
		KUNIT_EXPECT_TRUE(test, folio_test_large(folio));
		folio_put(folio);
	}
	/* If allocation fails, that's also acceptable for high-order allocations */
}

/*
 * Test rapid allocation and deallocation cycles
 */
static void nvfs_edge_case_rapid_cycles_test(struct kunit *test)
{
	struct folio *folio;
	int i;

	/* Perform rapid allocation/deallocation cycles */
	for (i = 0; i < 50; i++) {
		folio = folio_alloc(GFP_KERNEL, 0);
		if (folio) {
			/* Quick verification */
			KUNIT_EXPECT_EQ(test, 1, folio_nr_pages(folio));
			folio_put(folio);
		}

		/* Occasional reschedule */
		if (i % 10 == 0)
			cond_resched();
	}
}

/*
 * Test memory pressure simulation
 */
static void nvfs_memory_pressure_simulation_test(struct kunit *test)
{
	struct folio *folios[20];
	int i, allocated = 0;

	/* Try to allocate larger folios to create memory pressure */
	for (i = 0; i < 20; i++) {
		folios[i] = folio_alloc(GFP_KERNEL, 2); /* 4-page folios */
		if (folios[i]) {
			allocated++;
		}
	}

	/* We expect at least some allocations to succeed */
	KUNIT_EXPECT_GT(test, allocated, 0);

	/* Clean up */
	for (i = 0; i < 20; i++) {
		if (folios[i])
			folio_put(folios[i]);
	}
}

/*
 * Test reference counting under stress
 */
static void nvfs_stress_reference_counting_test(struct kunit *test)
{
	struct folio *folio;
	int initial_refcount;
	int i;

	/* Allocate a folio */
	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	initial_refcount = folio_ref_count(folio);

	/* Take and release multiple references */
	for (i = 0; i < 10; i++) {
		folio_get(folio);
	}

	for (i = 0; i < 10; i++) {
		folio_put(folio);
	}

	/* Verify reference count returned to initial value */
	KUNIT_EXPECT_EQ(test, initial_refcount, folio_ref_count(folio));

	/* Final cleanup */
	folio_put(folio);
}

static struct kunit_case nvfs_stress_test_cases[] = {
	KUNIT_CASE(nvfs_stress_folio_allocation_test),
	KUNIT_CASE(nvfs_stress_kmap_operations_test),
	KUNIT_CASE(nvfs_edge_case_max_order_test),
	KUNIT_CASE(nvfs_edge_case_rapid_cycles_test),
	KUNIT_CASE(nvfs_memory_pressure_simulation_test),
	KUNIT_CASE(nvfs_stress_reference_counting_test),
	{},
};

static struct kunit_suite nvfs_stress_test_suite = {
	.name = "nvfs_stress",
	.test_cases = nvfs_stress_test_cases,
};

kunit_test_suite(nvfs_stress_test_suite);

MODULE_DESCRIPTION("KUnit stress tests for NVFS memory management");
MODULE_LICENSE("GPL");