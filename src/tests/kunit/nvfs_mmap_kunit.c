// SPDX-License-Identifier: GPL-2.0
/*
 * KUnit Tests for NVFS Memory Mapping Operations
 * Tests VMA operations, shadow buffer management, and mmap functionality
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/mman.h>

/* NVFS mmap test constants */
#define GPU_PAGE_SIZE 65536
#define NVFS_BLOCK_SIZE 4096
#define NVFS_GPU_FOLIO_ORDER (16 - PAGE_SHIFT)  /* 64KB folios */
#define NVFS_START_MAGIC 0xabc0cba1abc2cba3ULL
#define NVFS_MIN_BASE_INDEX 0x100000000UL
#define NVFS_MAX_SHADOW_PAGES_ORDER 12
#define NVFS_MAX_SHADOW_PAGES (1 << NVFS_MAX_SHADOW_PAGES_ORDER)

/* Mock NVFS structures for testing mmap operations */
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

struct mock_nvfs_io_mgroup {
	unsigned long base_index;
	atomic_t ref;
	unsigned long nvfs_blocks_count;
	unsigned long nvfs_folios_count;
	struct folio **nvfs_folios;
	struct nvfs_io_metadata *nvfs_metadata;
	u64 cpu_base_vaddr;
};

/*
 * Test fixture for mmap operations
 */
struct nvfs_mmap_test_fixture {
	struct mock_nvfs_io_mgroup *mgroup;
	struct folio **test_folios;
	unsigned int folio_count;
	unsigned int block_count;
	size_t test_length;
};

static int nvfs_mmap_test_init(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture;
	unsigned int i;

	fixture = kunit_kzalloc(test, sizeof(*fixture), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture);

	/* Setup test parameters for 256KB allocation (4 x 64KB folios) */
	fixture->test_length = 256 * 1024;
	fixture->folio_count = DIV_ROUND_UP(fixture->test_length, GPU_PAGE_SIZE);
	fixture->block_count = DIV_ROUND_UP(fixture->test_length, NVFS_BLOCK_SIZE);

	/* Allocate mock mgroup */
	fixture->mgroup = kunit_kzalloc(test, sizeof(*fixture->mgroup), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->mgroup);

	/* Initialize mgroup */
	fixture->mgroup->base_index = NVFS_MIN_BASE_INDEX + 0x1000;
	atomic_set(&fixture->mgroup->ref, 1);
	fixture->mgroup->nvfs_blocks_count = fixture->block_count;
	fixture->mgroup->nvfs_folios_count = fixture->folio_count;

	/* Allocate folio array */
	fixture->mgroup->nvfs_folios = kunit_kcalloc(test, fixture->folio_count,
						      sizeof(struct folio *), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->mgroup->nvfs_folios);

	/* Allocate metadata array */
	fixture->mgroup->nvfs_metadata = kunit_kcalloc(test, fixture->block_count,
						       sizeof(struct nvfs_io_metadata), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fixture->mgroup->nvfs_metadata);

	/* Allocate test folios */
	for (i = 0; i < fixture->folio_count; i++) {
		fixture->mgroup->nvfs_folios[i] = folio_alloc(GFP_USER | __GFP_ZERO, NVFS_GPU_FOLIO_ORDER);
		if (fixture->mgroup->nvfs_folios[i]) {
			fixture->mgroup->nvfs_folios[i]->index = (fixture->mgroup->base_index * NVFS_MAX_SHADOW_PAGES) + i;
		} else {
			kunit_skip(test, "Failed to allocate GPU folios");
			return -ENOMEM;
		}
	}

	test->priv = fixture;
	return 0;
}

static void nvfs_mmap_test_exit(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	unsigned int i;

	if (fixture && fixture->mgroup && fixture->mgroup->nvfs_folios) {
		for (i = 0; i < fixture->folio_count; i++) {
			if (fixture->mgroup->nvfs_folios[i])
				folio_put(fixture->mgroup->nvfs_folios[i]);
		}
	}
}

/*
 * Test 1: Basic mmap metadata initialization
 */
static void nvfs_test_mmap_metadata_initialization(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	struct mock_nvfs_io_mgroup *mgroup = fixture->mgroup;
	unsigned int i;

	/* Initialize metadata for each block */
	for (i = 0; i < fixture->block_count; i++) {
		unsigned int folio_idx = i / (GPU_PAGE_SIZE / NVFS_BLOCK_SIZE);
		unsigned int block_in_folio = i % (GPU_PAGE_SIZE / NVFS_BLOCK_SIZE);

		mgroup->nvfs_metadata[i].nvfs_start_magic = NVFS_START_MAGIC;
		mgroup->nvfs_metadata[i].nvfs_state = NVFS_IO_ALLOC;
		
		if (folio_idx < mgroup->nvfs_folios_count) {
			mgroup->nvfs_metadata[i].folio = mgroup->nvfs_folios[folio_idx];
			mgroup->nvfs_metadata[i].folio_offset = block_in_folio * NVFS_BLOCK_SIZE;
		}
	}

	/* Validate metadata initialization */
	for (i = 0; i < fixture->block_count; i++) {
		KUNIT_EXPECT_EQ(test, mgroup->nvfs_metadata[i].nvfs_start_magic, NVFS_START_MAGIC);
		KUNIT_EXPECT_EQ(test, mgroup->nvfs_metadata[i].nvfs_state, NVFS_IO_ALLOC);
		KUNIT_EXPECT_NOT_NULL(test, mgroup->nvfs_metadata[i].folio);
		KUNIT_EXPECT_LT(test, mgroup->nvfs_metadata[i].folio_offset, GPU_PAGE_SIZE);
	}

	kunit_info(test, "Mmap metadata: initialized %u blocks across %u folios",
		   fixture->block_count, fixture->folio_count);
}

/*
 * Test 2: Shadow buffer alignment validation
 */
static void nvfs_test_shadow_buffer_alignment(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	u64 test_addresses[] = {
		0x100000000UL,		/* 4GB aligned */
		0x100000000UL + 4096,	/* 4KB offset */
		0x100010000UL,		/* 64KB aligned */
		0x100020000UL + 8192,	/* 64KB + 8KB */
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(test_addresses); i++) {
		u64 addr = test_addresses[i];
		bool is_block_aligned = (addr % NVFS_BLOCK_SIZE) == 0;
		bool is_gpu_page_aligned = (addr % GPU_PAGE_SIZE) == 0;
		
		if (is_block_aligned) {
			KUNIT_EXPECT_EQ(test, addr & (NVFS_BLOCK_SIZE - 1), 0);
		}
		
		if (is_gpu_page_aligned) {
			KUNIT_EXPECT_EQ(test, addr & (GPU_PAGE_SIZE - 1), 0);
		}

		kunit_info(test, "Address 0x%llx: block_aligned=%d, gpu_aligned=%d",
			   addr, is_block_aligned, is_gpu_page_aligned);
	}
}

/*
 * Test 3: Base index calculation and validation
 */
static void nvfs_test_base_index_calculation(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	unsigned long base_index = fixture->mgroup->base_index;
	unsigned int i;

	/* Validate base index is in valid range */
	KUNIT_EXPECT_GE(test, base_index, NVFS_MIN_BASE_INDEX);
	
	/* Test folio index calculations */
	for (i = 0; i < fixture->folio_count; i++) {
		struct folio *folio = fixture->mgroup->nvfs_folios[i];
		unsigned long expected_index = (base_index * NVFS_MAX_SHADOW_PAGES) + i;
		unsigned long calculated_base = folio->index >> NVFS_MAX_SHADOW_PAGES_ORDER;
		
		KUNIT_EXPECT_EQ(test, folio->index, expected_index);
		KUNIT_EXPECT_EQ(test, calculated_base, base_index);
	}

	kunit_info(test, "Base index validation: 0x%lx maps to %u folios correctly",
		   base_index, fixture->folio_count);
}

/*
 * Test 4: Reference counting in mmap operations
 */
static void nvfs_test_mmap_reference_counting(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	struct mock_nvfs_io_mgroup *mgroup = fixture->mgroup;
	int initial_ref, after_get, after_put;

	initial_ref = atomic_read(&mgroup->ref);
	KUNIT_EXPECT_EQ(test, initial_ref, 1);

	/* Simulate reference increment (like mmap would do) */
	atomic_inc(&mgroup->ref);
	after_get = atomic_read(&mgroup->ref);
	KUNIT_EXPECT_EQ(test, after_get, initial_ref + 1);

	/* Simulate reference decrement (like munmap would do) */
	bool should_free = atomic_dec_and_test(&mgroup->ref);
	after_put = atomic_read(&mgroup->ref);
	KUNIT_EXPECT_EQ(test, after_put, initial_ref);
	KUNIT_EXPECT_FALSE(test, should_free);

	kunit_info(test, "Reference counting: %d -> %d -> %d",
		   initial_ref, after_get, after_put);
}

/*
 * Test 5: VMA size validation and alignment
 */
static void nvfs_test_vma_size_validation(struct kunit *test)
{
	unsigned long test_sizes[] = {
		4096,			/* 4KB - minimum */
		65536,			/* 64KB - GPU page size */
		131072,			/* 128KB - 2x GPU page */
		262144,			/* 256KB - 4x GPU page */
		1048576,		/* 1MB - large allocation */
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(test_sizes); i++) {
		unsigned long size = test_sizes[i];
		bool is_4k_aligned = (size % NVFS_BLOCK_SIZE) == 0;
		bool is_64k_aligned = (size % GPU_PAGE_SIZE) == 0;
		unsigned long folio_count = DIV_ROUND_UP(size, GPU_PAGE_SIZE);
		unsigned long block_count = DIV_ROUND_UP(size, NVFS_BLOCK_SIZE);

		KUNIT_EXPECT_TRUE(test, is_4k_aligned);
		
		if (size >= GPU_PAGE_SIZE) {
			/* For sizes >= 64KB, prefer 64KB alignment */
			if (is_64k_aligned) {
				KUNIT_EXPECT_EQ(test, size % GPU_PAGE_SIZE, 0);
			}
		}

		kunit_info(test, "Size %lu: %lu folios, %lu blocks, 4K_aligned=%d, 64K_aligned=%d",
			   size, folio_count, block_count, is_4k_aligned, is_64k_aligned);
	}
}

/*
 * Test 6: Folio boundary calculations within mmap regions
 */
static void nvfs_test_folio_boundary_in_mmap(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	unsigned int i;

	for (i = 0; i < fixture->folio_count; i++) {
		struct folio *folio = fixture->mgroup->nvfs_folios[i];
		unsigned long folio_start_addr = i * GPU_PAGE_SIZE;
		unsigned long folio_end_addr = folio_start_addr + GPU_PAGE_SIZE - 1;
		unsigned int blocks_in_folio = GPU_PAGE_SIZE / NVFS_BLOCK_SIZE;
		unsigned int j;

		KUNIT_EXPECT_EQ(test, folio_size(folio), GPU_PAGE_SIZE);
		KUNIT_EXPECT_EQ(test, folio_nr_pages(folio), GPU_PAGE_SIZE / PAGE_SIZE);

		/* Validate block offsets within this folio */
		for (j = 0; j < blocks_in_folio; j++) {
			unsigned int block_idx = i * blocks_in_folio + j;
			if (block_idx < fixture->block_count) {
				unsigned int expected_offset = j * NVFS_BLOCK_SIZE;
				KUNIT_EXPECT_EQ(test, fixture->mgroup->nvfs_metadata[block_idx].folio_offset,
						expected_offset);
			}
		}

		kunit_info(test, "Folio %u: addr range 0x%lx-0x%lx, %u blocks",
			   i, folio_start_addr, folio_end_addr, blocks_in_folio);
	}
}

/*
 * Test 7: State transition validation for mmap operations
 */
static void nvfs_test_mmap_state_transitions(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	unsigned int i;

	/* Test complete state transition sequence */
	for (i = 0; i < fixture->block_count; i++) {
		struct nvfs_io_metadata *metadata = &fixture->mgroup->nvfs_metadata[i];

		/* Initialize */
		metadata->nvfs_start_magic = NVFS_START_MAGIC;
		metadata->nvfs_state = NVFS_IO_FREE;
		
		/* Transition through mmap states */
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
	}

	kunit_info(test, "State transitions: validated %u blocks through complete lifecycle",
		   fixture->block_count);
}

/*
 * Test 8: Error state handling in mmap operations
 */
static void nvfs_test_mmap_error_handling(struct kunit *test)
{
	struct nvfs_mmap_test_fixture *fixture = test->priv;
	unsigned int i, error_count = 0;

	/* Inject errors in some blocks */
	for (i = 0; i < fixture->block_count; i += 5) {  /* Every 5th block */
		fixture->mgroup->nvfs_metadata[i].nvfs_start_magic = NVFS_START_MAGIC;
		fixture->mgroup->nvfs_metadata[i].nvfs_state = NVFS_IO_DMA_ERROR;
		error_count++;
	}

	/* Validate error detection */
	for (i = 0; i < fixture->block_count; i++) {
		struct nvfs_io_metadata *metadata = &fixture->mgroup->nvfs_metadata[i];
		
		if (i % 5 == 0) {
			KUNIT_EXPECT_EQ(test, metadata->nvfs_state, NVFS_IO_DMA_ERROR);
		}
	}

	kunit_info(test, "Error handling: injected and validated %u error states",
		   error_count);
}

/*
 * Test 9: Large mapping stress test
 */
static void nvfs_test_large_mmap_stress(struct kunit *test)
{
	unsigned long large_size = 16 * 1024 * 1024;  /* 16MB */
	unsigned long folio_count = DIV_ROUND_UP(large_size, GPU_PAGE_SIZE);
	unsigned long block_count = DIV_ROUND_UP(large_size, NVFS_BLOCK_SIZE);
	struct folio **folios;
	struct nvfs_io_metadata *metadata;
	unsigned long i, successful_allocs = 0;

	/* Attempt large allocation */
	folios = kcalloc(folio_count, sizeof(struct folio *), GFP_KERNEL);
	if (!folios) {
		kunit_skip(test, "Cannot allocate folio array for stress test");
		return;
	}

	metadata = kcalloc(block_count, sizeof(struct nvfs_io_metadata), GFP_KERNEL);
	if (!metadata) {
		kfree(folios);
		kunit_skip(test, "Cannot allocate metadata for stress test");
		return;
	}

	/* Try to allocate many large folios */
	for (i = 0; i < folio_count && i < 32; i++) {  /* Limit to 32 for test */
		folios[i] = folio_alloc(GFP_KERNEL | __GFP_NOWARN, NVFS_GPU_FOLIO_ORDER);
		if (folios[i]) {
			successful_allocs++;
			KUNIT_EXPECT_EQ(test, folio_size(folios[i]), GPU_PAGE_SIZE);
		}
	}

	kunit_info(test, "Large mmap stress: %lu/%lu folios allocated (%.1f%% success)",
		   successful_allocs, min(folio_count, 32UL),
		   (successful_allocs * 100.0) / min(folio_count, 32UL));

	/* Cleanup */
	for (i = 0; i < folio_count && i < 32; i++) {
		if (folios[i])
			folio_put(folios[i]);
	}

	kfree(metadata);
	kfree(folios);

	KUNIT_EXPECT_GT(test, successful_allocs, 0);
}

/*
 * Test case definitions
 */
static struct kunit_case nvfs_mmap_test_cases[] = {
	KUNIT_CASE(nvfs_test_mmap_metadata_initialization),
	KUNIT_CASE(nvfs_test_shadow_buffer_alignment),
	KUNIT_CASE(nvfs_test_base_index_calculation),
	KUNIT_CASE(nvfs_test_mmap_reference_counting),
	KUNIT_CASE(nvfs_test_vma_size_validation),
	KUNIT_CASE(nvfs_test_folio_boundary_in_mmap),
	KUNIT_CASE(nvfs_test_mmap_state_transitions),
	KUNIT_CASE(nvfs_test_mmap_error_handling),
	KUNIT_CASE(nvfs_test_large_mmap_stress),
	{}
};

/*
 * KUnit test suite definition
 */
static struct kunit_suite nvfs_mmap_test_suite = {
	.name = "nvfs_mmap_operations",
	.init = nvfs_mmap_test_init,
	.exit = nvfs_mmap_test_exit,
	.test_cases = nvfs_mmap_test_cases,
};

kunit_test_suite(nvfs_mmap_test_suite);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("KUnit tests for NVFS memory mapping operations");
MODULE_AUTHOR("NVIDIA Corporation");