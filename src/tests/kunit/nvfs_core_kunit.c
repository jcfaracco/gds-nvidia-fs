// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit Unit Tests for NVFS Core Functions
 * Tests individual functions in isolation with minimal dependencies
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>

/*
 * Unit test: Memory allocation parameter validation
 */
static void nvfs_allocation_params_test(struct kunit *test)
{
	unsigned int order;
	size_t expected_size;

	/* Test order to size calculations */
	for (order = 0; order < 4; order++) {
		expected_size = PAGE_SIZE << order;
		KUNIT_EXPECT_EQ(test, expected_size, PAGE_SIZE * (1 << order));
	}

	/* Test boundary conditions */
	KUNIT_EXPECT_EQ(test, PAGE_SIZE, PAGE_SIZE << 0);
	KUNIT_EXPECT_EQ(test, 1, 1 << 0);
}

/*
 * Unit test: Folio to page pointer arithmetic
 */
static void nvfs_folio_page_arithmetic_test(struct kunit *test)
{
	struct folio *folio;
	struct page *page0, *page1;

	/* Test with multi-page folio */
	folio = folio_alloc(GFP_KERNEL, 1); /* 2 pages */
	if (!folio) {
		kunit_skip(test, "Multi-page allocation failed");
		return;
	}

	page0 = folio_page(folio, 0);
	page1 = folio_page(folio, 1);

	KUNIT_ASSERT_NOT_NULL(test, page0);
	KUNIT_ASSERT_NOT_NULL(test, page1);

	/* Verify page pointer arithmetic */
	KUNIT_EXPECT_PTR_EQ(test, page1, page0 + 1);
	KUNIT_EXPECT_EQ(test, 2, folio_nr_pages(folio));

	folio_put(folio);
}

/*
 * Unit test: Reference counting behavior
 */
static void nvfs_refcount_test(struct kunit *test)
{
	struct folio *folio;
	int initial_refcount, after_get;

	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	initial_refcount = folio_ref_count(folio);
	KUNIT_EXPECT_GE(test, initial_refcount, 1);

	/* Test folio_get increases refcount */
	folio_get(folio);
	after_get = folio_ref_count(folio);
	KUNIT_EXPECT_EQ(test, initial_refcount + 1, after_get);

	/* Clean up */
	folio_put(folio); /* From folio_get */
	folio_put(folio); /* From folio_alloc */
}

/*
 * Unit test: Address alignment checks
 */
static void nvfs_alignment_test(struct kunit *test)
{
	struct folio *folio;
	void *addr;
	unsigned long addr_val;

	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	addr = folio_address(folio);
	if (addr) {
		addr_val = (unsigned long)addr;

		/* Test page alignment */
		KUNIT_EXPECT_EQ(test, 0, addr_val & (PAGE_SIZE - 1));

		/* Test that address is non-zero */
		KUNIT_EXPECT_NE(test, 0, addr_val);
	}

	folio_put(folio);
}

/*
 * Unit test: Folio flag operations
 */
static void nvfs_folio_flags_test(struct kunit *test)
{
	struct folio *small_folio, *large_folio;

	/* Test single page folio */
	small_folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, small_folio);

	KUNIT_EXPECT_FALSE(test, folio_test_large(small_folio));
	KUNIT_EXPECT_EQ(test, 1, folio_nr_pages(small_folio));

	folio_put(small_folio);

	/* Test multi-page folio */
	large_folio = folio_alloc(GFP_KERNEL, 1);
	if (large_folio) {
		KUNIT_EXPECT_TRUE(test, folio_test_large(large_folio));
		KUNIT_EXPECT_GT(test, folio_nr_pages(large_folio), 1);
		folio_put(large_folio);
	} else {
		kunit_skip(test, "Large folio allocation failed");
	}
}

static struct kunit_case nvfs_core_test_cases[] = {
	KUNIT_CASE(nvfs_allocation_params_test),
	KUNIT_CASE(nvfs_folio_page_arithmetic_test),
	KUNIT_CASE(nvfs_refcount_test),
	KUNIT_CASE(nvfs_alignment_test),
	KUNIT_CASE(nvfs_folio_flags_test),
	{}
};

static struct kunit_suite nvfs_core_test_suite = {
	.name = "nvfs_core_unit_tests",
	.test_cases = nvfs_core_test_cases,
};

kunit_test_suite(nvfs_core_test_suite);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NVFS Core Unit Tests");
MODULE_AUTHOR("NVIDIA Corporation");
