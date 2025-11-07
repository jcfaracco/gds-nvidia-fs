// SPDX-License-Identifier: GPL-2.0
/*
 * NVFS Userspace Tests - Device File Operations
 * Tests NVFS character device interfaces
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
#include <sys/sysmacros.h>
#include <sys/ioctl.h>

#define NVFS_DEV_PATH "/dev/nvidia-fs"
#define NVFS_DEV_PATTERN "/dev/nvidia-fs%d"
#define MAX_DEVICES 16

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_skipped = 0;

#define TEST_PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)
#define TEST_SKIP(msg) do { tests_skipped++; printf("SKIP: %s\n", msg); } while(0)

static void test_device_node_exists(void)
{
    struct stat st;
    
    tests_run++;
    printf("Testing device node exists ... ");
    
    if (stat(NVFS_DEV_PATH, &st) == 0) {
        if (S_ISCHR(st.st_mode)) {
            TEST_PASS();
        } else {
            TEST_FAIL("not a character device");
        }
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_permissions(void)
{
    struct stat st;
    
    tests_run++;
    printf("Testing device permissions ... ");
    
    if (stat(NVFS_DEV_PATH, &st) == 0) {
        mode_t mode = st.st_mode & 0777;
        /* Device should be readable/writable by owner, readable by group */
        if (mode == 0644 || mode == 0664 || mode == 0666) {
            TEST_PASS();
        } else {
            printf("FAIL: unexpected permissions 0%o\n", mode);
            tests_failed++;
        }
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_major_minor(void)
{
    struct stat st;
    
    tests_run++;
    printf("Testing device major/minor numbers ... ");
    
    if (stat(NVFS_DEV_PATH, &st) == 0) {
        int major_num = major(st.st_rdev);
        int minor_num = minor(st.st_rdev);
        
        /* Check for reasonable major/minor numbers */
        if (major_num > 0 && major_num < 512 && minor_num >= 0) {
            printf("PASS (major=%d, minor=%d)\n", major_num, minor_num);
            tests_passed++;
        } else {
            printf("FAIL: invalid major=%d, minor=%d\n", major_num, minor_num);
            tests_failed++;
        }
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_open_close(void)
{
    int fd;
    
    tests_run++;
    printf("Testing device open/close ... ");
    
    fd = open(NVFS_DEV_PATH, O_RDWR);
    if (fd >= 0) {
        close(fd);
        TEST_PASS();
    } else {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_multiple_open(void)
{
    int fd1, fd2;
    
    tests_run++;
    printf("Testing multiple device opens ... ");
    
    fd1 = open(NVFS_DEV_PATH, O_RDWR);
    if (fd1 < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    fd2 = open(NVFS_DEV_PATH, O_RDWR);
    if (fd2 >= 0) {
        close(fd2);
        close(fd1);
        TEST_PASS();
    } else {
        close(fd1);
        if (errno == EBUSY) {
            TEST_PASS(); /* Single-open device is also valid */
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_read_basic(void)
{
    int fd;
    char buffer[64];
    ssize_t result;
    
    tests_run++;
    printf("Testing device read operation ... ");
    
    fd = open(NVFS_DEV_PATH, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    result = read(fd, buffer, sizeof(buffer));
    close(fd);
    
    if (result >= 0) {
        TEST_PASS();
    } else {
        if (errno == EINVAL || errno == ENOSYS) {
            TEST_SKIP("read not supported");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_write_basic(void)
{
    int fd;
    char buffer[64] = "test data";
    ssize_t result;
    
    tests_run++;
    printf("Testing device write operation ... ");
    
    fd = open(NVFS_DEV_PATH, O_WRONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    result = write(fd, buffer, strlen(buffer));
    close(fd);
    
    if (result >= 0) {
        TEST_PASS();
    } else {
        if (errno == EINVAL || errno == ENOSYS) {
            TEST_SKIP("write not supported");
        } else {
            TEST_FAIL(strerror(errno));
        }
    }
}

static void test_device_ioctl_basic(void)
{
    int fd;
    int result;
    
    tests_run++;
    printf("Testing device ioctl operation ... ");
    
    fd = open(NVFS_DEV_PATH, O_RDWR);
    if (fd < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    /* Try a basic ioctl - use a safe command that should return error */
    result = ioctl(fd, 0);
    close(fd);
    
    /* Any response (including error) indicates ioctl is implemented */
    if (result == 0 || errno == EINVAL || errno == ENOTTY) {
        TEST_PASS();
    } else {
        TEST_FAIL(strerror(errno));
    }
}

static void test_device_invalid_operations(void)
{
    int fd;
    
    tests_run++;
    printf("Testing invalid device operations ... ");
    
    fd = open(NVFS_DEV_PATH, O_RDWR);
    if (fd < 0) {
        if (errno == ENOENT) {
            TEST_SKIP("NVFS device not present");
        } else if (errno == EACCES || errno == EPERM) {
            TEST_SKIP("insufficient permissions");
        } else {
            TEST_FAIL(strerror(errno));
        }
        return;
    }
    
    /* Test invalid operations - these should fail gracefully */
    off_t seek_result = lseek(fd, 100, SEEK_SET);
    close(fd);
    
    if (seek_result == -1 && errno == ESPIPE) {
        TEST_PASS(); /* Character devices typically don't support seek */
    } else if (seek_result >= 0) {
        TEST_PASS(); /* Some devices do support seek */
    } else {
        TEST_FAIL("unexpected seek behavior");
    }
}

static void test_numbered_devices(void)
{
    char device_path[64];
    struct stat st;
    int found_devices = 0;
    
    tests_run++;
    printf("Testing numbered device nodes ... ");
    
    /* Check for numbered device nodes (nvidia-fs0, nvidia-fs1, etc.) */
    for (int i = 0; i < MAX_DEVICES; i++) {
        snprintf(device_path, sizeof(device_path), NVFS_DEV_PATTERN, i);
        if (stat(device_path, &st) == 0) {
            if (S_ISCHR(st.st_mode)) {
                found_devices++;
            }
        }
    }
    
    if (found_devices > 0) {
        printf("PASS (found %d numbered devices)\n", found_devices);
        tests_passed++;
    } else {
        TEST_SKIP("no numbered devices found");
    }
}

static void run_all_device_tests(void)
{
    printf("=== NVFS Device File Tests ===\n");
    
    test_device_node_exists();
    test_device_permissions();
    test_device_major_minor();
    test_device_open_close();
    test_device_multiple_open();
    test_device_read_basic();
    test_device_write_basic();
    test_device_ioctl_basic();
    test_device_invalid_operations();
    test_numbered_devices();
}

int main(void)
{
    printf("NVFS Device Interface Tests\n");
    printf("===========================\n");
    
    run_all_device_tests();
    
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