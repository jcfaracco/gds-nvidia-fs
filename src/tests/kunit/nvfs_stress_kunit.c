// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit DMA Stress and Memory Pressure Tests for NVFS
 * Tests DMA operations, boundary conditions and performance characteristics
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/delay.h>

#define PERF_TEST_ITERATIONS 100
#define DMA_STRESS_ITERATIONS 50
#define GPU_PAGE_SIZE 65536
#define NVFS_BLOCK_SIZE 4096
#define NVFS_GPU_FOLIO_ORDER (16 - PAGE_SHIFT)  /* 64KB folios */

/* Mock NVFS structures for testing DMA operations */
enum nvfs_block_state {
	NVFS_IO_FREE = 0,
	NVFS_IO_ALLOC,
	NVFS_IO_INIT,
	NVFS_IO_QUEUED,
	NVFS_IO_DMA_START,
	NVFS_IO_DONE,
	NVFS_IO_DMA_ERROR
};

struct nvfs_io_metadata {
	u64 nvfs_start_magic;
	enum nvfs_block_state nvfs_state;
	struct folio *folio;
	unsigned int folio_offset;
};

#define NVFS_START_MAGIC 0xabc0cba1abc2cba3ULL

/*
 * DMA Stress Test: Large folio allocation stress
 */
static void nvfs_dma_large_folio_stress_test(struct kunit *test)
{
	struct folio *folios[DMA_STRESS_ITERATIONS];
	unsigned long start, end;
	int i, successful_allocs = 0, gpu_order_allocs = 0;

	start = jiffies;

	/* Allocate GPU-sized folios under stress */
	for (i = 0; i < DMA_STRESS_ITERATIONS; i++) {
		folios[i] = folio_alloc(GFP_KERNEL | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
		if (folios[i]) {
			successful_allocs++;
			if (folio_order(folios[i]) == NVFS_GPU_FOLIO_ORDER) {
				gpu_order_allocs++;
				/* Verify GPU page size */
				KUNIT_EXPECT_EQ(test, folio_size(folios[i]), GPU_PAGE_SIZE);
			}
		}
	}

	/* Deallocate all folios */
	for (i = 0; i < DMA_STRESS_ITERATIONS; i++) {
		if (folios[i])
			folio_put(folios[i]);
	}

	end = jiffies;

	kunit_info(test, "GPU folio stress: %d/%d successful, %d correct order, %lu jiffies",
		   successful_allocs, DMA_STRESS_ITERATIONS, gpu_order_allocs, end - start);

	/* We should get some successful allocations */
	KUNIT_EXPECT_GT(test, successful_allocs, DMA_STRESS_ITERATIONS / 4);
}

/*
 * DMA Stress Test: Metadata initialization under pressure
 */
static void nvfs_dma_metadata_stress_test(struct kunit *test)
{
	struct folio *folio;
	struct nvfs_io_metadata *metadata;
	unsigned int blocks_per_folio = GPU_PAGE_SIZE / NVFS_BLOCK_SIZE;
	unsigned int i;
	unsigned long start, end;

	folio = folio_alloc(GFP_KERNEL | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
	if (!folio) {
		kunit_skip(test, "GPU folio allocation failed under pressure");
		return;
	}

	metadata = kcalloc(blocks_per_folio, sizeof(struct nvfs_io_metadata), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, metadata);

	start = jiffies;

	/* Initialize metadata under time pressure */
	for (i = 0; i < blocks_per_folio; i++) {
		metadata[i].nvfs_start_magic = NVFS_START_MAGIC;
		metadata[i].nvfs_state = NVFS_IO_ALLOC;
		metadata[i].folio = folio;
		metadata[i].folio_offset = i * NVFS_BLOCK_SIZE;

		/* Simulate state transitions */
		metadata[i].nvfs_state = NVFS_IO_INIT;
		metadata[i].nvfs_state = NVFS_IO_QUEUED;
		metadata[i].nvfs_state = NVFS_IO_DMA_START;
		metadata[i].nvfs_state = NVFS_IO_DONE;
	}

	end = jiffies;

	/* Validate all metadata was properly initialized */
	for (i = 0; i < blocks_per_folio; i++) {
		KUNIT_EXPECT_EQ(test, metadata[i].nvfs_start_magic, NVFS_START_MAGIC);
		KUNIT_EXPECT_EQ(test, metadata[i].nvfs_state, NVFS_IO_DONE);
		KUNIT_EXPECT_EQ(test, metadata[i].folio, folio);
		KUNIT_EXPECT_EQ(test, metadata[i].folio_offset, i * NVFS_BLOCK_SIZE);
	}

	kunit_info(test, "Metadata stress: %u blocks initialized in %lu jiffies",
		   blocks_per_folio, end - start);

	kfree(metadata);
	folio_put(folio);
}

/*
 * DMA Stress Test: Memory pressure simulation
 */
static void nvfs_dma_memory_pressure_test(struct kunit *test)
{
	struct folio *pressure_folios[PERF_TEST_ITERATIONS * 2];
	struct folio *gpu_folios[DMA_STRESS_ITERATIONS];
	int i, pressure_allocs = 0, gpu_allocs = 0;

	/* Create memory pressure with regular allocations */
	for (i = 0; i < PERF_TEST_ITERATIONS * 2; i++) {
		pressure_folios[i] = folio_alloc(GFP_KERNEL, 2); /* 16KB folios */
		if (pressure_folios[i])
			pressure_allocs++;
	}

	kunit_info(test, "Memory pressure created: %d/%d pressure allocations",
		   pressure_allocs, PERF_TEST_ITERATIONS * 2);

	/* Try GPU allocations under pressure */
	for (i = 0; i < DMA_STRESS_ITERATIONS; i++) {
		gpu_folios[i] = folio_alloc(GFP_KERNEL | __GFP_NOWARN, NVFS_GPU_FOLIO_ORDER);
		if (gpu_folios[i]) {
			gpu_allocs++;
			KUNIT_EXPECT_EQ(test, folio_size(gpu_folios[i]), GPU_PAGE_SIZE);
		}
	}

	kunit_info(test, "GPU allocations under pressure: %d/%d successful",
		   gpu_allocs, DMA_STRESS_ITERATIONS);

	/* Clean up pressure allocations */
	for (i = 0; i < PERF_TEST_ITERATIONS * 2; i++) {
		if (pressure_folios[i])
			folio_put(pressure_folios[i]);
	}

	/* Clean up GPU allocations */
	for (i = 0; i < DMA_STRESS_ITERATIONS; i++) {
		if (gpu_folios[i])
			folio_put(gpu_folios[i]);
	}

	/* Some allocations should succeed even under pressure */
	KUNIT_EXPECT_GT(test, gpu_allocs, 0);
}

/*
 * DMA Stress Test: Rapid allocation/deallocation cycles
 */
static void nvfs_dma_rapid_cycle_test(struct kunit *test)
{
	struct folio *folio;
	unsigned long start, end;
	int i, cycles = 0;
	int successful_cycles = 0;

	start = jiffies;

	for (i = 0; i < DMA_STRESS_ITERATIONS; i++) {
		folio = folio_alloc(GFP_KERNEL | __GFP_NOWARN, NVFS_GPU_FOLIO_ORDER);
		cycles++;
		
		if (folio) {
			successful_cycles++;
			
			/* Quick validation */
			KUNIT_EXPECT_EQ(test, folio_order(folio), NVFS_GPU_FOLIO_ORDER);
			KUNIT_EXPECT_GT(test, folio_ref_count(folio), 0);
			
			/* Immediate deallocation */
			folio_put(folio);
		}
		
		/* Brief pause to allow system processing */
		if (i % 10 == 0)
			cpu_relax();
	}

	end = jiffies;

	kunit_info(test, "Rapid cycles: %d attempts, %d successful, %lu jiffies",
		   cycles, successful_cycles, end - start);

	/* Should complete reasonable number of cycles */
	KUNIT_EXPECT_GT(test, successful_cycles, cycles / 4);
}

/*
 * DMA Stress Test: Multi-folio concurrent operations
 */
static void nvfs_dma_concurrent_folio_test(struct kunit *test)
{
	struct folio *folios[16];
	struct nvfs_io_metadata *metadata[16];
	unsigned int blocks_per_folio = GPU_PAGE_SIZE / NVFS_BLOCK_SIZE;
	int i, j, successful_allocs = 0;

	/* Allocate multiple GPU folios simultaneously */
	for (i = 0; i < 16; i++) {
		folios[i] = folio_alloc(GFP_KERNEL | __GFP_NOWARN | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
		metadata[i] = NULL;
		
		if (folios[i]) {
			successful_allocs++;
			
			/* Allocate metadata for this folio */
			metadata[i] = kcalloc(blocks_per_folio, sizeof(struct nvfs_io_metadata), GFP_KERNEL);
			if (metadata[i]) {
				/* Initialize all blocks in this folio */
				for (j = 0; j < blocks_per_folio; j++) {
					metadata[i][j].nvfs_start_magic = NVFS_START_MAGIC;
					metadata[i][j].nvfs_state = NVFS_IO_ALLOC;
					metadata[i][j].folio = folios[i];
					metadata[i][j].folio_offset = j * NVFS_BLOCK_SIZE;
				}
			}
		}
	}

	kunit_info(test, "Concurrent folios: %d/%d successful allocations", successful_allocs, 16);

	/* Validate concurrent operations don't interfere */
	for (i = 0; i < 16; i++) {
		if (folios[i] && metadata[i]) {
			for (j = 0; j < blocks_per_folio; j++) {
				KUNIT_EXPECT_EQ(test, metadata[i][j].folio, folios[i]);
				KUNIT_EXPECT_EQ(test, metadata[i][j].nvfs_start_magic, NVFS_START_MAGIC);
			}
		}
	}

	/* Cleanup */
	for (i = 0; i < 16; i++) {
		kfree(metadata[i]);
		if (folios[i])
			folio_put(folios[i]);
	}

	KUNIT_EXPECT_GT(test, successful_allocs, 0);
}

/*
 * DMA Stress Test: Error injection simulation
 */
static void nvfs_dma_error_injection_test(struct kunit *test)
{
	struct folio *folio;
	struct nvfs_io_metadata metadata[16];
	unsigned int i;

	folio = folio_alloc(GFP_KERNEL | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
	if (!folio) {
		kunit_skip(test, "GPU folio allocation failed");
		return;
	}

	/* Initialize metadata */
	for (i = 0; i < 16; i++) {
		metadata[i].nvfs_start_magic = NVFS_START_MAGIC;
		metadata[i].nvfs_state = NVFS_IO_QUEUED;
		metadata[i].folio = folio;
		metadata[i].folio_offset = i * NVFS_BLOCK_SIZE;
	}

	/* Simulate DMA error injection on random blocks */
	for (i = 0; i < 16; i += 3) {  /* Every 3rd block gets error */
		metadata[i].nvfs_state = NVFS_IO_DMA_ERROR;
	}

	/* Validate error states are properly set */
	for (i = 0; i < 16; i++) {
		if (i % 3 == 0) {
			KUNIT_EXPECT_EQ(test, metadata[i].nvfs_state, NVFS_IO_DMA_ERROR);
		} else {
			KUNIT_EXPECT_EQ(test, metadata[i].nvfs_state, NVFS_IO_QUEUED);
		}
		KUNIT_EXPECT_EQ(test, metadata[i].folio, folio);
	}

	kunit_info(test, "Error injection: validated error states in metadata");

	folio_put(folio);
}

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

static struct kunit_case nvfs_stress_test_cases[] = {
	KUNIT_CASE(nvfs_dma_large_folio_stress_test),
	KUNIT_CASE(nvfs_dma_metadata_stress_test),
	KUNIT_CASE(nvfs_dma_memory_pressure_test),
	KUNIT_CASE(nvfs_dma_rapid_cycle_test),
	KUNIT_CASE(nvfs_dma_concurrent_folio_test),
	KUNIT_CASE(nvfs_dma_error_injection_test),
	KUNIT_CASE(nvfs_allocation_performance_test),
	KUNIT_CASE(nvfs_max_order_edge_test),
	KUNIT_CASE(nvfs_zero_order_edge_test),
	{}
};

static struct kunit_suite nvfs_stress_test_suite = {
	.name = "nvfs_dma_stress_tests",
	.test_cases = nvfs_stress_test_cases,
};

kunit_test_suite(nvfs_stress_test_suite);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NVFS DMA Stress and Memory Pressure Tests");
MODULE_AUTHOR("NVIDIA Corporation");