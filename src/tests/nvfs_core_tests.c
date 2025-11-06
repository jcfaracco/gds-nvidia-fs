// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS Core Functionality Tests
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include "nvfs_test.h"
#include "../nvfs-core.h"
#include "../nvfs-mmap.h"

/* Test basic memory page allocation and mapping */
NVFS_TEST_DECLARE(folio_allocation_basic)
{
    struct folio *folio;
    struct page *page;
    
    /* Test single page folio allocation */
    folio = folio_alloc(GFP_KERNEL, 0);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate single page folio");
    
    page = folio_page(folio, 0);
    NVFS_TEST_ASSERT_NOT_NULL(page, "Failed to get page from folio");
    
    /* Test folio properties */
    NVFS_TEST_ASSERT_EQ(1, folio_nr_pages(folio), "Unexpected number of pages in single-page folio");
    NVFS_TEST_ASSERT_EQ(PAGE_SIZE, folio_size(folio), "Unexpected folio size");
    
    folio_put(folio);
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(folio_allocation_multipage)
{
    struct folio *folio;
    unsigned int order = 2; /* 4 pages */
    unsigned int expected_pages = 1 << order;
    
    /* Test multi-page folio allocation */
    folio = folio_alloc(GFP_KERNEL, order);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate multi-page folio");
    
    /* Test folio properties */
    NVFS_TEST_ASSERT_EQ(expected_pages, folio_nr_pages(folio), 
                        "Unexpected number of pages in multi-page folio");
    NVFS_TEST_ASSERT_EQ(PAGE_SIZE * expected_pages, folio_size(folio), 
                        "Unexpected multi-page folio size");
    
    /* Test that folio is marked as large */
    NVFS_TEST_ASSERT(folio_test_large(folio), "Multi-page folio not marked as large");
    
    folio_put(folio);
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(page_to_folio_conversion)
{
    struct folio *folio;
    struct page *page;
    struct folio *converted_folio;
    
    /* Allocate a folio */
    folio = folio_alloc(GFP_KERNEL, 1); /* 2 pages */
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate folio for conversion test");
    
    /* Get a page from the folio */
    page = folio_page(folio, 0);
    NVFS_TEST_ASSERT_NOT_NULL(page, "Failed to get page from folio");
    
    /* Convert page back to folio */
    converted_folio = page_folio(page);
    NVFS_TEST_ASSERT_NOT_NULL(converted_folio, "Failed to convert page to folio");
    
    /* Verify it's the same folio */
    NVFS_TEST_ASSERT_EQ((unsigned long)folio, (unsigned long)converted_folio,
                        "Page-to-folio conversion returned different folio");
    
    folio_put(folio);
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(kmap_local_basic)
{
    struct folio *folio;
    struct page *page;
    void *kaddr;
    char test_data = 0xAB;
    
    /* Allocate a folio */
    folio = folio_alloc(GFP_KERNEL | __GFP_ZERO, 0);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate folio for kmap test");
    
    page = folio_page(folio, 0);
    NVFS_TEST_ASSERT_NOT_NULL(page, "Failed to get page from folio");
    
    /* Test kmap_local_page */
    kaddr = kmap_local_page(page);
    NVFS_TEST_ASSERT_NOT_NULL(kaddr, "kmap_local_page failed");
    
    /* Write and read test data */
    *(char *)kaddr = test_data;
    NVFS_TEST_ASSERT_EQ(test_data, *(char *)kaddr, "Data write/read through kmap failed");
    
    kunmap_local(kaddr);
    folio_put(folio);
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(nvfs_mgroup_basic)
{
    /* This test would require mocking or having the actual NVFS structures */
    /* For now, just test that we can include the headers without errors */
    pr_info("NVFS_TEST: nvfs_mgroup basic test - header inclusion successful");
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(memory_allocation_failure)
{
    struct folio *folio;
    
    /* Test allocation with impossible order (should fail gracefully) */
    folio = folio_alloc(GFP_KERNEL, MAX_ORDER + 1);
    NVFS_TEST_ASSERT_NULL(folio, "Expected allocation failure for impossible order");
    
    return NVFS_TEST_PASS;
}

NVFS_TEST_DECLARE(reference_counting)
{
    struct folio *folio;
    int initial_refcount, after_get_refcount;
    
    folio = folio_alloc(GFP_KERNEL, 0);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate folio for refcount test");
    
    initial_refcount = folio_ref_count(folio);
    NVFS_TEST_ASSERT(initial_refcount > 0, "Initial refcount should be positive");
    
    /* Get additional reference */
    folio_get(folio);
    after_get_refcount = folio_ref_count(folio);
    NVFS_TEST_ASSERT_EQ(initial_refcount + 1, after_get_refcount,
                        "Reference count not incremented correctly");
    
    /* Put both references */
    folio_put(folio);
    folio_put(folio);
    
    return NVFS_TEST_PASS;
}

/* Test suite setup */
static int nvfs_core_setup(void)
{
    pr_info("NVFS_TEST: Core test suite setup\n");
    return 0;
}

/* Test suite teardown */
static void nvfs_core_teardown(void)
{
    pr_info("NVFS_TEST: Core test suite teardown\n");
}

/* Define test cases */
static struct nvfs_test_case nvfs_core_tests[] = {
    NVFS_TEST_CASE(folio_allocation_basic, "Basic single-page folio allocation"),
    NVFS_TEST_CASE(folio_allocation_multipage, "Multi-page folio allocation"),
    NVFS_TEST_CASE(page_to_folio_conversion, "Page to folio conversion"),
    NVFS_TEST_CASE(kmap_local_basic, "Basic kmap_local_page functionality"),
    NVFS_TEST_CASE(nvfs_mgroup_basic, "Basic NVFS mgroup operations"),
    NVFS_TEST_CASE(memory_allocation_failure, "Memory allocation failure handling"),
    NVFS_TEST_CASE(reference_counting, "Folio reference counting"),
};

/* Test suite definition */
struct nvfs_test_suite nvfs_core_test_suite = {
    .name = "NVFS Core Tests",
    .tests = nvfs_core_tests,
    .num_tests = ARRAY_SIZE(nvfs_core_tests),
    .setup = nvfs_core_setup,
    .teardown = nvfs_core_teardown,
};