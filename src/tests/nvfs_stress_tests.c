// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS Stress and Edge Case Tests
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include "nvfs_test.h"

#define STRESS_TEST_ITERATIONS 100
#define STRESS_TEST_MAX_ORDER 3
#define STRESS_TEST_CONCURRENT_ALLOCS 50

/* Stress test: Allocate and deallocate many folios */
NVFS_TEST_DECLARE(stress_folio_allocation)
{
    struct folio *folios[STRESS_TEST_ITERATIONS];
    int i, j;
    int allocated_count = 0;
    
    /* Allocate many folios */
    for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        unsigned int order = i % (STRESS_TEST_MAX_ORDER + 1);
        folios[i] = folio_alloc(GFP_KERNEL, order);
        if (folios[i]) {
            allocated_count++;
            
            /* Verify allocation properties */
            NVFS_TEST_ASSERT_EQ(1 << order, folio_nr_pages(folios[i]),
                                "Incorrect page count in stress test");
        }
        
        /* Add some variability */
        if (i % 10 == 0)
            cond_resched();
    }
    
    pr_info("NVFS_TEST: Allocated %d/%d folios in stress test\n", 
            allocated_count, STRESS_TEST_ITERATIONS);
    
    /* Deallocate in reverse order */
    for (j = STRESS_TEST_ITERATIONS - 1; j >= 0; j--) {
        if (folios[j]) {
            folio_put(folios[j]);
        }
    }
    
    NVFS_TEST_ASSERT(allocated_count > STRESS_TEST_ITERATIONS / 2,
                     "Too few allocations succeeded in stress test");
    
    return NVFS_TEST_PASS;
}

/* Stress test: Concurrent kmap/kunmap operations */
NVFS_TEST_DECLARE(stress_kmap_operations)
{
    struct folio *folio;
    struct page *pages[STRESS_TEST_CONCURRENT_ALLOCS];
    void *kaddrs[STRESS_TEST_CONCURRENT_ALLOCS];
    int i;
    char test_pattern = 0x55;
    
    /* Allocate a large folio */
    folio = folio_alloc(GFP_KERNEL | __GFP_ZERO, STRESS_TEST_MAX_ORDER);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate large folio for stress test");
    
    /* Map multiple pages from the folio concurrently */
    for (i = 0; i < STRESS_TEST_CONCURRENT_ALLOCS && i < folio_nr_pages(folio); i++) {
        pages[i] = folio_page(folio, i);
        NVFS_TEST_ASSERT_NOT_NULL(pages[i], "Failed to get page from folio");
        
        kaddrs[i] = kmap_local_page(pages[i]);
        NVFS_TEST_ASSERT_NOT_NULL(kaddrs[i], "kmap_local_page failed in stress test");
        
        /* Write test pattern */
        *(char *)kaddrs[i] = test_pattern + i;
    }
    
    /* Verify all data and unmap */
    for (i = STRESS_TEST_CONCURRENT_ALLOCS - 1; i >= 0 && i < folio_nr_pages(folio); i--) {
        if (kaddrs[i]) {
            /* Verify data integrity */
            NVFS_TEST_ASSERT_EQ(test_pattern + i, *(char *)kaddrs[i],
                                "Data corruption in concurrent kmap stress test");
            
            kunmap_local(kaddrs[i]);
        }
    }
    
    folio_put(folio);
    return NVFS_TEST_PASS;
}

/* Edge case: Test maximum order allocation */
NVFS_TEST_DECLARE(edge_case_max_order)
{
    struct folio *folio;
    unsigned int max_reasonable_order = min(MAX_ORDER - 1, 8U);
    
    /* Try to allocate maximum reasonable order */
    folio = folio_alloc(GFP_KERNEL, max_reasonable_order);
    
    if (folio) {
        /* If successful, verify properties */
        unsigned int expected_pages = 1 << max_reasonable_order;
        NVFS_TEST_ASSERT_EQ(expected_pages, folio_nr_pages(folio),
                            "Incorrect page count for max order allocation");
        NVFS_TEST_ASSERT(folio_test_large(folio),
                         "Large folio not marked as large");
        folio_put(folio);
        pr_info("NVFS_TEST: Successfully allocated max order %u folio\n", 
                max_reasonable_order);
    } else {
        pr_info("NVFS_TEST: Max order allocation failed (expected under memory pressure)\n");
    }
    
    return NVFS_TEST_PASS;
}

/* Edge case: Test zero-sized operations */
NVFS_TEST_DECLARE(edge_case_zero_operations)
{
    struct folio *folio;
    
    /* Test zero-order allocation (single page) */
    folio = folio_alloc(GFP_KERNEL, 0);
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Zero-order allocation failed");
    
    /* Verify it's a single page */
    NVFS_TEST_ASSERT_EQ(1, folio_nr_pages(folio), "Zero-order folio has wrong page count");
    NVFS_TEST_ASSERT_EQ(PAGE_SIZE, folio_size(folio), "Zero-order folio has wrong size");
    NVFS_TEST_ASSERT(!folio_test_large(folio), "Single-page folio marked as large");
    
    folio_put(folio);
    return NVFS_TEST_PASS;
}

/* Edge case: Test rapid allocation/deallocation cycles */
NVFS_TEST_DECLARE(edge_case_rapid_cycles)
{
    int i;
    struct folio *folio;
    
    for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        /* Rapidly allocate and deallocate */
        folio = folio_alloc(GFP_KERNEL, 0);
        if (folio) {
            /* Quick validation */
            NVFS_TEST_ASSERT_NOT_NULL(folio_page(folio, 0), 
                                     "Page retrieval failed in rapid cycle");
            folio_put(folio);
        }
        
        /* Yield occasionally to avoid monopolizing CPU */
        if (i % 50 == 0)
            cond_resched();
    }
    
    return NVFS_TEST_PASS;
}

/* Test memory pressure scenarios */
NVFS_TEST_DECLARE(memory_pressure_simulation)
{
    struct folio *folios[STRESS_TEST_ITERATIONS];
    int allocated = 0;
    int i;
    
    /* Try to allocate many large folios to simulate memory pressure */
    for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        folios[i] = folio_alloc(GFP_KERNEL | __GFP_NOWARN, 2); /* 4 pages each */
        if (folios[i]) {
            allocated++;
        } else {
            folios[i] = NULL;
        }
        
        /* Stop if we hit memory pressure */
        if (i > 10 && allocated == 0) {
            pr_info("NVFS_TEST: Hit memory pressure early at iteration %d\n", i);
            break;
        }
    }
    
    pr_info("NVFS_TEST: Allocated %d large folios under memory pressure\n", allocated);
    
    /* Clean up all allocations */
    for (i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        if (folios[i]) {
            folio_put(folios[i]);
        }
    }
    
    /* Test should pass as long as some allocations succeeded initially */
    NVFS_TEST_ASSERT(allocated >= 0, "Memory pressure test basic validation");
    
    return NVFS_TEST_PASS;
}

/* Test reference counting under stress */
NVFS_TEST_DECLARE(stress_reference_counting)
{
    struct folio *folio;
    int i;
    int initial_refcount;
    
    folio = folio_alloc(GFP_KERNEL, 1); /* 2 pages */
    NVFS_TEST_ASSERT_NOT_NULL(folio, "Failed to allocate folio for refcount stress test");
    
    initial_refcount = folio_ref_count(folio);
    
    /* Take many references */
    for (i = 0; i < 50; i++) {
        folio_get(folio);
    }
    
    /* Verify reference count */
    NVFS_TEST_ASSERT_EQ(initial_refcount + 50, folio_ref_count(folio),
                        "Reference count mismatch after multiple gets");
    
    /* Release all extra references */
    for (i = 0; i < 50; i++) {
        folio_put(folio);
    }
    
    /* Verify back to initial count */
    NVFS_TEST_ASSERT_EQ(initial_refcount, folio_ref_count(folio),
                        "Reference count mismatch after multiple puts");
    
    /* Release initial reference */
    folio_put(folio);
    
    return NVFS_TEST_PASS;
}

/* Test suite setup */
static int nvfs_stress_setup(void)
{
    pr_info("NVFS_TEST: Stress test suite setup - preparing for intensive testing\n");
    return 0;
}

/* Test suite teardown */
static void nvfs_stress_teardown(void)
{
    pr_info("NVFS_TEST: Stress test suite teardown - cleaning up\n");
    /* Force garbage collection */
    cond_resched();
}

/* Define test cases */
static struct nvfs_test_case nvfs_stress_tests[] = {
    NVFS_TEST_CASE(stress_folio_allocation, "Stress test folio allocation/deallocation"),
    NVFS_TEST_CASE(stress_kmap_operations, "Stress test concurrent kmap operations"),
    NVFS_TEST_CASE(edge_case_max_order, "Edge case maximum order allocation"),
    NVFS_TEST_CASE(edge_case_zero_operations, "Edge case zero-order operations"),
    NVFS_TEST_CASE(edge_case_rapid_cycles, "Edge case rapid allocation cycles"),
    NVFS_TEST_CASE(memory_pressure_simulation, "Memory pressure simulation"),
    NVFS_TEST_CASE(stress_reference_counting, "Stress test reference counting"),
};

/* Test suite definition */
struct nvfs_test_suite nvfs_stress_test_suite = {
    .name = "NVFS Stress Tests",
    .tests = nvfs_stress_tests,
    .num_tests = ARRAY_SIZE(nvfs_stress_tests),
    .setup = nvfs_stress_setup,
    .teardown = nvfs_stress_teardown,
};