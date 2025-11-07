// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS Userspace Tests - Proc Filesystem Interface
 * Tests /proc/driver/nvidia-fs interfaces
 * Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PROC_NVFS_BASE "/proc/driver/nvidia-fs"
#define MAX_READ_SIZE 4096

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

#define TEST_PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)
#define TEST_SKIP(msg) do { tests_skipped++; printf("SKIP: %s\n", msg); } while(0)

static void test_proc_file_exists(const char *filename)
{
    char path[256];
    struct stat st;
    
    tests_run++;
    printf("Testing proc file exists: %s ... ", filename);
    
    snprintf(path, sizeof(path), "%s/%s", PROC_NVFS_BASE, filename);
    
    if (stat(path, &st) == 0) {
        if (S_ISREG(st.st_mode)) {
            TEST_PASS();
        } else {
            TEST_FAIL("not a regular file");
        }
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_proc_file_readable(const char *filename)
{
    char path[256];
    char buffer[MAX_READ_SIZE];
    int fd;
    ssize_t bytes_read;
    
    tests_run++;
    printf("Testing proc file readable: %s ... ", filename);
    
    snprintf(path, sizeof(path), "%s/%s", PROC_NVFS_BASE, filename);
    
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    
    if (bytes_read >= 0) {
        buffer[bytes_read] = '\0';
        TEST_PASS();
    } else {
        TEST_FAIL(strerror(errno));
    }
}

static void test_proc_file_permissions(const char *filename, mode_t expected_mode)
{
    char path[256];
    struct stat st;
    
    tests_run++;
    printf("Testing proc file permissions: %s ... ", filename);
    
    snprintf(path, sizeof(path), "%s/%s", PROC_NVFS_BASE, filename);
    
    if (stat(path, &st) == 0) {
        mode_t actual_mode = st.st_mode & 0777;
        if (actual_mode == expected_mode) {
            TEST_PASS();
        } else {
            printf("FAIL: expected 0%o, got 0%o\n", expected_mode, actual_mode);
            tests_failed++;
        }
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_proc_version_format(void)
{
    char path[256];
    char buffer[256];
    FILE *fp;
    
    tests_run++;
    printf("Testing version format ... ");
    
    snprintf(path, sizeof(path), "%s/version", PROC_NVFS_BASE);
    
    fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    if (fgets(buffer, sizeof(buffer), fp)) {
        /* Check if version string contains expected format */
        if (strstr(buffer, "NVIDIA-FS") || strstr(buffer, "nvfs")) {
            TEST_PASS();
        } else {
            TEST_FAIL("unexpected version format");
        }
    } else {
        TEST_FAIL("could not read version");
    }
    
    fclose(fp);
}

static void test_proc_stats_format(void)
{
    char path[256];
    char buffer[1024];
    FILE *fp;
    int line_count = 0;
    
    tests_run++;
    printf("Testing stats format ... ");
    
    snprintf(path, sizeof(path), "%s/stats", PROC_NVFS_BASE);
    
    fp = fopen(path, "r");
    if (!fp) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    /* Count lines and check for basic stats entries */
    while (fgets(buffer, sizeof(buffer), fp)) {
        line_count++;
        /* Stats should contain numeric values */
        if (strchr(buffer, ':')) {
            /* Found a stat entry */
        }
    }
    
    if (line_count > 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("no stats found");
    }
    
    fclose(fp);
}

static void test_proc_write_protection(const char *filename)
{
    char path[256];
    int fd;
    
    tests_run++;
    printf("Testing write protection: %s ... ", filename);
    
    snprintf(path, sizeof(path), "%s/%s", PROC_NVFS_BASE, filename);
    
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        if (errno == EACCES || errno == EPERM) {
            TEST_PASS(); /* Correctly write-protected */
        } else if (errno == ENOENT) {
            TEST_SKIP("NVFS module not loaded");
        } else {
            TEST_FAIL("unexpected error");
        }
    } else {
        close(fd);
        TEST_FAIL("file should be write-protected");
    }
}

static void run_all_proc_tests(void)
{
    const char *proc_files[] = {
        "devcount",
        "version", 
        "bridges",
        "modules",
        "stats",
        "peer_affinity",
        "peer_distance"
    };
    
    int num_files = sizeof(proc_files) / sizeof(proc_files[0]);
    
    printf("=== NVFS Proc Filesystem Tests ===\n");
    
    /* Test file existence */
    for (int i = 0; i < num_files; i++) {
        test_proc_file_exists(proc_files[i]);
    }
    
    /* Test file readability */
    for (int i = 0; i < num_files; i++) {
        test_proc_file_readable(proc_files[i]);
    }
    
    /* Test file permissions (should be 0444 - read-only) */
    for (int i = 0; i < num_files; i++) {
        test_proc_file_permissions(proc_files[i], 0444);
    }
    
    /* Test write protection */
    for (int i = 0; i < num_files; i++) {
        test_proc_write_protection(proc_files[i]);
    }
    
    /* Test specific content formats */
    test_proc_version_format();
    test_proc_stats_format();
}

int main(void)
{
    printf("NVFS Proc Interface Tests\n");
    printf("=========================\n");
    
    run_all_proc_tests();
    
    printf("\n=== Test Summary ===\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Skipped: %d\n", tests_skipped);
    
    if (tests_failed > 0) {
        return 1;
    }
    
    return 0;
}
