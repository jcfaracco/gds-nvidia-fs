savedcmd_nvfs_selftest.mod := printf '%s\n'   nvfs_test_framework.o nvfs_core_tests.o nvfs_stress_tests.o nvfs_stub_tests.o | awk '!x[$$0]++ { print("./"$$0) }' > nvfs_selftest.mod
