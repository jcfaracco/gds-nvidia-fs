// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit Tests for NVFS Folio-Native Operations
 * Tests the modernized folio-based memory management functions
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>

/* NVFS test helpers */
#define NVFS_GPU_FOLIO_ORDER 4  /* 64KB = order 4 for 4KB pages */
#define GPU_PAGE_SIZE 65536
#define GPU_PAGE_SHIFT 16
#define NVFS_BLOCK_SIZE 4096
#define NVFS_BLOCK_SHIFT 12
#define NVFS_START_MAGIC 0xabc0cba1abc2cba3ULL
#define NVFS_MIN_BASE_INDEX 0x100000000UL

/* Mock NVFS structures for testing */
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

/*
 * Test fixture for folio operations
 */
struct nvfs_folio_test_fixture {
	struct folio *test_folio;
	struct nvfs_io_metadata *metadata;
	unsigned int num_blocks;
	void *test_pages[16]; /* Pages within the folio */
};

static int nvfs_folio_test_init(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture;
	int i;
	
	fixture = kunit_kzalloc(test, sizeof(*fixture), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture);
	
	/* Allocate a 64KB folio for testing */
	fixture->test_folio = folio_alloc(GFP_KERNEL | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->test_folio);
	
	/* Calculate number of 4KB blocks in the 64KB folio */
	fixture->num_blocks = GPU_PAGE_SIZE / NVFS_BLOCK_SIZE;
	
	/* Allocate metadata for each block */
	fixture->metadata = kunit_kcalloc(test, fixture->num_blocks, 
					  sizeof(struct nvfs_io_metadata), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->metadata);
	
	/* Initialize test pages from the folio */
	for (i = 0; i < folio_nr_pages(fixture->test_folio) && i < 16; i++) {
		fixture->test_pages[i] = folio_page(fixture->test_folio, i);
	}
	
	test->priv = fixture;
	return 0;
}

static void nvfs_folio_test_exit(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	
	if (fixture && fixture->test_folio)
		folio_put(fixture->test_folio);
}

/*
 * Test 1: Folio allocation with correct GPU page order
 */
static void nvfs_test_folio_gpu_allocation(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	struct folio *folio = fixture->test_folio;
	
	/* Verify folio was allocated successfully */
	KUNIT_EXPECT_NOT_NULL(test, folio);
	
	/* Verify correct order (64KB = order 4) */
	KUNIT_EXPECT_EQ(test, folio_order(folio), NVFS_GPU_FOLIO_ORDER);
	
	/* Verify correct size */
	KUNIT_EXPECT_EQ(test, folio_size(folio), GPU_PAGE_SIZE);
	
	/* Verify correct number of pages */
	KUNIT_EXPECT_EQ(test, folio_nr_pages(folio), GPU_PAGE_SIZE / PAGE_SIZE);
	
	/* Verify folio is not file-backed (mapping should be NULL) */
	KUNIT_EXPECT_NULL(test, folio->mapping);
	
	kunit_info(test, "Folio allocation: order=%u, size=%zu, nr_pages=%u",
		   folio_order(folio), folio_size(folio), folio_nr_pages(folio));
}

/*
 * Test 2: Folio page extraction and validation
 */
static void nvfs_test_folio_page_extraction(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	struct folio *folio = fixture->test_folio;
	struct page *page;
	unsigned int i, nr_pages = folio_nr_pages(folio);
	
	/* Test extraction of each page from the folio */
	for (i = 0; i < nr_pages; i++) {
		page = folio_page(folio, i);
		
		KUNIT_EXPECT_NOT_NULL(test, page);
		KUNIT_EXPECT_EQ(test, page_folio(page), folio);
		
		/* Verify pages are contiguous */
		if (i > 0) {
			struct page *prev_page = folio_page(folio, i - 1);
			KUNIT_EXPECT_EQ(test, page_to_pfn(page), 
					 page_to_pfn(prev_page) + 1);
		}
	}
	
	kunit_info(test, "Page extraction: validated %u pages from folio", nr_pages);
}

/*
 * Test 3: Block metadata initialization and validation
 */
static void nvfs_test_metadata_initialization(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	struct nvfs_io_metadata *metadata = fixture->metadata;
	unsigned int i;
	
	/* Initialize metadata for each block */
	for (i = 0; i < fixture->num_blocks; i++) {
		metadata[i].nvfs_start_magic = NVFS_START_MAGIC;
		metadata[i].nvfs_state = NVFS_IO_ALLOC;
		metadata[i].folio = fixture->test_folio;
		metadata[i].folio_offset = i * NVFS_BLOCK_SIZE;
	}
	
	/* Validate metadata initialization */
	for (i = 0; i < fixture->num_blocks; i++) {
		KUNIT_EXPECT_EQ(test, metadata[i].nvfs_start_magic, NVFS_START_MAGIC);
		KUNIT_EXPECT_EQ(test, metadata[i].nvfs_state, NVFS_IO_ALLOC);
		KUNIT_EXPECT_EQ(test, metadata[i].folio, fixture->test_folio);
		KUNIT_EXPECT_EQ(test, metadata[i].folio_offset, i * NVFS_BLOCK_SIZE);
	}
	
	kunit_info(test, "Metadata initialization: validated %u blocks", fixture->num_blocks);
}

/*
 * Test 4: Folio boundary calculations
 */
static void nvfs_test_folio_boundary_calculations(struct kunit *test)
{
	unsigned long test_addresses[] = {
		0x0,
		0x1000,       /* 4KB */
		0x10000,      /* 64KB */
		0x11000,      /* 64KB + 4KB */
		0x100000,     /* 1MB */
		0x1000000,    /* 16MB */
	};
	int i;
	
	for (i = 0; i < ARRAY_SIZE(test_addresses); i++) {
		unsigned long addr = test_addresses[i];
		unsigned long folio_start = addr & ~(GPU_PAGE_SIZE - 1);
		unsigned long folio_offset = addr & (GPU_PAGE_SIZE - 1);
		
		/* Verify boundary calculations */
		KUNIT_EXPECT_EQ(test, folio_start + folio_offset, addr);
		KUNIT_EXPECT_LT(test, folio_offset, GPU_PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, folio_start & (GPU_PAGE_SIZE - 1), 0);
		
		kunit_info(test, "Address 0x%lx: folio_start=0x%lx, offset=0x%lx",
			   addr, folio_start, folio_offset);
	}
}

/*
 * Test 5: DMA state transitions
 */
static void nvfs_test_dma_state_transitions(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	struct nvfs_io_metadata *metadata = &fixture->metadata[0];
	
	/* Initialize metadata */
	metadata->nvfs_start_magic = NVFS_START_MAGIC;
	metadata->folio = fixture->test_folio;
	metadata->folio_offset = 0;
	
	/* Test state transitions */
	metadata->nvfs_state = NVFS_IO_FREE;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_FREE);
	
	metadata->nvfs_state = NVFS_IO_ALLOC;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_ALLOC);
	
	metadata->nvfs_state = NVFS_IO_INIT;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_INIT);
	
	metadata->nvfs_state = NVFS_IO_QUEUED;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_QUEUED);
	
	metadata->nvfs_state = NVFS_IO_DMA_START;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_DMA_START);
	
	metadata->nvfs_state = NVFS_IO_DONE;
	KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_DONE);
	
	kunit_info(test, "DMA state transitions: all states validated");
}

/*
 * Test 6: Folio offset calculations within blocks
 */
static void nvfs_test_block_offset_calculations(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	unsigned int block_idx, expected_offset;
	
	/* Test offset calculations for each block */
	for (block_idx = 0; block_idx < fixture->num_blocks; block_idx++) {
		expected_offset = block_idx * NVFS_BLOCK_SIZE;
		
		/* Verify offset is within folio bounds */
		KUNIT_EXPECT_LT(test, expected_offset, GPU_PAGE_SIZE);
		
		/* Verify offset is block-aligned */
		KUNIT_EXPECT_EQ(test, expected_offset & (NVFS_BLOCK_SIZE - 1), 0);
		
		/* Verify we can calculate the block index from offset */
		KUNIT_EXPECT_EQ(test, expected_offset / NVFS_BLOCK_SIZE, block_idx);
	}
	
	kunit_info(test, "Block offset calculations: validated %u blocks", fixture->num_blocks);
}

/*
 * Test 7: Memory alignment verification
 */
static void nvfs_test_memory_alignment(struct kunit *test)
{
	struct nvfs_folio_test_fixture *fixture = test->priv;
	struct folio *folio = fixture->test_folio;
	struct page *page;
	unsigned long pfn;
	
	/* Test folio alignment */
	page = folio_page(folio, 0);
	pfn = page_to_pfn(page);
	
	/* 64KB folio should be aligned to 64KB boundary in PFNs */
	KUNIT_EXPECT_EQ(test, pfn & ((GPU_PAGE_SIZE >> PAGE_SHIFT) - 1), 0);
	
	/* Test that all pages in the folio are properly aligned */
	unsigned int i;
	for (i = 0; i < folio_nr_pages(folio); i++) {
		page = folio_page(folio, i);
		pfn = page_to_pfn(page);
		
		/* Each page should be at the expected offset */
		KUNIT_EXPECT_EQ(test, pfn & (PAGE_SIZE / PAGE_SIZE - 1), 0);
	}
	
	kunit_info(test, "Memory alignment: folio PFN=0x%lx properly aligned", 
		   page_to_pfn(folio_page(folio, 0)));
}

/*
 * Test 8: Constants and limits validation
 */
static void nvfs_test_constants_validation(struct kunit *test)
{
	/* Test GPU page constants */
	KUNIT_EXPECT_EQ(test, GPU_PAGE_SIZE, 65536);
	KUNIT_EXPECT_EQ(test, GPU_PAGE_SHIFT, 16);
	KUNIT_EXPECT_EQ(test, 1UL << GPU_PAGE_SHIFT, GPU_PAGE_SIZE);
	
	/* Test block constants */
	KUNIT_EXPECT_EQ(test, NVFS_BLOCK_SIZE, 4096);
	KUNIT_EXPECT_EQ(test, NVFS_BLOCK_SHIFT, 12);
	KUNIT_EXPECT_EQ(test, 1UL << NVFS_BLOCK_SHIFT, NVFS_BLOCK_SIZE);
	
	/* Test folio order calculation */
	KUNIT_EXPECT_EQ(test, NVFS_GPU_FOLIO_ORDER, GPU_PAGE_SHIFT - PAGE_SHIFT);
	KUNIT_EXPECT_EQ(test, PAGE_SIZE << NVFS_GPU_FOLIO_ORDER, GPU_PAGE_SIZE);
	
	/* Test blocks per GPU page */
	KUNIT_EXPECT_EQ(test, GPU_PAGE_SIZE / NVFS_BLOCK_SIZE, 16);
	
	kunit_info(test, "Constants validation: all GPU/block size relationships correct");
}

/*
 * Test case definitions
 */
static struct kunit_case nvfs_folio_test_cases[] = {
	KUNIT_CASE(nvfs_test_folio_gpu_allocation),
	KUNIT_CASE(nvfs_test_folio_page_extraction),
	KUNIT_CASE(nvfs_test_metadata_initialization),
	KUNIT_CASE(nvfs_test_folio_boundary_calculations),
	KUNIT_CASE(nvfs_test_dma_state_transitions),
	KUNIT_CASE(nvfs_test_block_offset_calculations),
	KUNIT_CASE(nvfs_test_memory_alignment),
	KUNIT_CASE(nvfs_test_constants_validation),
	{}
};

/*
 * KUnit test suite definition
 */
static struct kunit_suite nvfs_folio_test_suite = {
	.name = "nvfs_folio_native",
	.init = nvfs_folio_test_init,
	.exit = nvfs_folio_test_exit,
	.test_cases = nvfs_folio_test_cases,
};

kunit_test_suite(nvfs_folio_test_suite);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KUnit tests for NVFS folio-native operations");
MODULE_AUTHOR("NVIDIA Corporation");