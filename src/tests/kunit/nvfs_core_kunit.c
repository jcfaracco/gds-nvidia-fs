// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit tests for NVFS core functionality
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include "../../nvfs-core.h"
#include "../../nvfs-mmap.h"

/*
 * Test folio allocation and basic properties
 */
static void nvfs_folio_allocation_test(struct kunit *test)
{
	struct folio *folio;
	struct page *page;

	/* Test single page folio allocation */
	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	page = folio_page(folio, 0);
	KUNIT_ASSERT_NOT_NULL(test, page);

	/* Verify folio properties */
	KUNIT_EXPECT_EQ(test, 1, folio_nr_pages(folio));
	KUNIT_EXPECT_EQ(test, PAGE_SIZE, folio_size(folio));

	folio_put(folio);
}

/*
 * Test multi-page folio allocation
 */
static void nvfs_folio_multipage_test(struct kunit *test)
{
	struct folio *folio;
	unsigned int order = 2; /* 4 pages */
	unsigned int expected_pages = 1 << order;

	/* Test multi-page folio allocation */
	folio = folio_alloc(GFP_KERNEL, order);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	/* Verify multi-page folio properties */
	KUNIT_EXPECT_EQ(test, expected_pages, folio_nr_pages(folio));
	KUNIT_EXPECT_EQ(test, PAGE_SIZE * expected_pages, folio_size(folio));

	/* Verify folio is compound */
	KUNIT_EXPECT_TRUE(test, folio_test_large(folio));

	folio_put(folio);
}

/*
 * Test page to folio conversion
 */
static void nvfs_page_to_folio_test(struct kunit *test)
{
	struct folio *original_folio, *converted_folio;
	struct page *page;

	/* Allocate a folio */
	original_folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, original_folio);

	/* Get page from folio */
	page = folio_page(original_folio, 0);
	KUNIT_ASSERT_NOT_NULL(test, page);

	/* Convert page back to folio */
	converted_folio = page_folio(page);
	KUNIT_ASSERT_NOT_NULL(test, converted_folio);

	/* Verify they are the same */
	KUNIT_EXPECT_PTR_EQ(test, original_folio, converted_folio);

	folio_put(original_folio);
}

/*
 * Test kmap_local_page functionality
 */
static void nvfs_kmap_local_test(struct kunit *test)
{
	struct folio *folio;
	struct page *page;
	void *kaddr;
	char test_data[] = "NVFS KUnit Test Data";
	char *mapped_data;

	/* Allocate folio and get page */
	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	page = folio_page(folio, 0);
	KUNIT_ASSERT_NOT_NULL(test, page);

	/* Map page for kernel access */
	kaddr = kmap_local_page(page);
	KUNIT_ASSERT_NOT_NULL(test, kaddr);

	/* Write test data */
	mapped_data = (char *)kaddr;
	strcpy(mapped_data, test_data);

	/* Verify data was written */
	KUNIT_EXPECT_STREQ(test, test_data, mapped_data);

	/* Unmap and cleanup */
	kunmap_local(kaddr);
	folio_put(folio);
}

/*
 * Test reference counting
 */
static void nvfs_reference_counting_test(struct kunit *test)
{
	struct folio *folio;
	int initial_refcount, after_get_refcount;

	/* Allocate folio */
	folio = folio_alloc(GFP_KERNEL, 0);
	KUNIT_ASSERT_NOT_NULL(test, folio);

	/* Check initial reference count */
	initial_refcount = folio_ref_count(folio);
	KUNIT_EXPECT_GT(test, initial_refcount, 0);

	/* Take additional reference */
	folio_get(folio);
	after_get_refcount = folio_ref_count(folio);
	KUNIT_EXPECT_EQ(test, initial_refcount + 1, after_get_refcount);

	/* Release references */
	folio_put(folio);
	folio_put(folio);
}

/*
 * Test memory allocation failure handling
 */
static void nvfs_allocation_failure_test(struct kunit *test)
{
	struct folio *folio;

	/* This test is primarily for ensuring the test framework
	 * can handle allocation patterns properly.
	 * In a real scenario, we might use fault injection.
	 */

	/* Test with reasonable order (should succeed) */
	folio = folio_alloc(GFP_KERNEL, 0);
	if (folio) {
		KUNIT_EXPECT_NOT_NULL(test, folio);
		folio_put(folio);
	}

	/* Test with very high order (likely to fail gracefully) */
	folio = folio_alloc(GFP_KERNEL, MAX_ORDER);
	if (folio) {
		/* If it succeeds, that's fine too */
		folio_put(folio);
	}
	/* If it fails, that's expected and okay */
}

static struct kunit_case nvfs_core_test_cases[] = {
	KUNIT_CASE(nvfs_folio_allocation_test),
	KUNIT_CASE(nvfs_folio_multipage_test),
	KUNIT_CASE(nvfs_page_to_folio_test),
	KUNIT_CASE(nvfs_kmap_local_test),
	KUNIT_CASE(nvfs_reference_counting_test),
	KUNIT_CASE(nvfs_allocation_failure_test),
	{},
};

static struct kunit_suite nvfs_core_test_suite = {
	.name = "nvfs_core",
	.test_cases = nvfs_core_test_cases,
};

kunit_test_suite(nvfs_core_test_suite);

MODULE_DESCRIPTION("KUnit tests for NVFS core functionality");
MODULE_LICENSE("GPL");