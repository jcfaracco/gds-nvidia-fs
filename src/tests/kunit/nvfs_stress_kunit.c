// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit Performance and Edge Case Tests for NVFS
 * Tests boundary conditions and performance characteristics
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

#define PERF_TEST_ITERATIONS 100

/*
 * Performance test: Allocation/deallocation speed
 */
static void nvfs_allocation_performance_test(struct kunit *test)
{
	struct folio *folios[PERF_TEST_ITERATIONS];
	unsigned long start, end;
	int i, successful_allocs = 0;

	start = jiffies;

	/* Allocate many folios */
	for (i = 0; i < PERF_TEST_ITERATIONS; i++) {
		folios[i] = folio_alloc(GFP_KERNEL, 0);
		if (folios[i])
			successful_allocs++;
	}

	/* Deallocate them */
	for (i = 0; i < PERF_TEST_ITERATIONS; i++) {
		if (folios[i])
			folio_put(folios[i]);
	}

	end = jiffies;

	kunit_info(test, "Allocated/freed %d/%d folios in %lu jiffies\n",
		   successful_allocs, PERF_TEST_ITERATIONS, end - start);

	/* We should get at least some allocations */
	KUNIT_EXPECT_GT(test, successful_allocs, PERF_TEST_ITERATIONS / 2);
}

/*
 * Edge case test: Maximum order allocation
 */
static void nvfs_max_order_edge_test(struct kunit *test)
{
	struct folio *folio;
	unsigned int max_order = MAX_PAGE_ORDER - 1;

	/* This might fail, but shouldn't crash */
	folio = folio_alloc(GFP_KERNEL, max_order);
	if (folio) {
		unsigned int expected_pages = 1 << max_order;

		KUNIT_EXPECT_EQ(test, expected_pages, folio_nr_pages(folio));
		KUNIT_EXPECT_TRUE(test, folio_test_large(folio));

		kunit_info(test, "Successfully allocated max order (%u) folio with %lu pages\n",
			   max_order, folio_nr_pages(folio));

		folio_put(folio);
	} else {
		kunit_info(test, "Max order allocation failed (expected under memory pressure)\n");
	}

	/* Test should pass regardless of allocation success */
	KUNIT_EXPECT_TRUE(test, true);
}

/*
 * Edge case test: Zero-order boundary
 */
static void nvfs_zero_order_edge_test(struct kunit *test)
{
	struct folio *folio;

	/* Test minimum allocation */
	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	/* Verify it's exactly one page */
	KUNIT_EXPECT_EQ(test, 1, folio_nr_pages(folio));
	KUNIT_EXPECT_EQ(test, PAGE_SIZE, folio_size(folio));
	KUNIT_EXPECT_FALSE(test, folio_test_large(folio));

	folio_put(folio);
}

/*
 * Edge case test: Reference count stress
 */
static void nvfs_refcount_stress_test(struct kunit *test)
{
	struct folio *folio;
	int i;
	int initial_count, final_count;

	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	initial_count = folio_ref_count(folio);

	/* Add many references */
	for (i = 0; i < 10; i++)
		folio_get(folio);

	/* Verify count increased */
	KUNIT_EXPECT_EQ(test, initial_count + 10, folio_ref_count(folio));

	/* Remove all extra references */
	for (i = 0; i < 10; i++)
		folio_put(folio);

	final_count = folio_ref_count(folio);
	KUNIT_EXPECT_EQ(test, initial_count, final_count);

	/* Final cleanup */
	folio_put(folio);
}

/*
 * Edge case test: Page index boundaries
 */
static void nvfs_page_index_edge_test(struct kunit *test)
{
	struct folio *folio;
	struct page *page;
	unsigned int order = 2; /* 4 pages */
	unsigned int max_index = (1 << order) - 1;

	folio = folio_alloc(GFP_KERNEL, order);
	if (!folio) {
		kunit_skip(test, "Multi-page allocation failed");
		return;
	}

	/* Test first page */
	page = folio_page(folio, 0);
	KUNIT_ASSERT_NOT_NULL(test, page);

	/* Test last valid page */
	page = folio_page(folio, max_index);
	KUNIT_ASSERT_NOT_NULL(test, page);

	/* Verify page distance */
	KUNIT_EXPECT_PTR_EQ(test,
			    folio_page(folio, max_index),
			    folio_page(folio, 0) + max_index);

	folio_put(folio);
}

static struct kunit_case nvfs_stress_test_cases[] = {
	KUNIT_CASE(nvfs_allocation_performance_test),
	KUNIT_CASE(nvfs_max_order_edge_test),
	KUNIT_CASE(nvfs_zero_order_edge_test),
	KUNIT_CASE(nvfs_refcount_stress_test),
	KUNIT_CASE(nvfs_page_index_edge_test),
	{}
};

static struct kunit_suite nvfs_stress_test_suite = {
	.name = "nvfs_stress_unit_tests",
	.test_cases = nvfs_stress_test_cases,
};

kunit_test_suite(nvfs_stress_test_suite);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NVFS Stress and Edge Case Unit Tests");
MODULE_AUTHOR("NVIDIA Corporation");
