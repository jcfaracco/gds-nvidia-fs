savedcmd_nvfs_selftest.o := ld -m elf_x86_64 -z noexecstack --no-warn-rwx-segments   -r -o nvfs_selftest.o @nvfs_selftest.mod  ; /usr/src/kernels/6.17.4-200.fc42.x86_64/tools/objtool/objtool --hacks=jump_label --hacks=noinstr --hacks=skylake --ibt --orc --retpoline --rethunk --sls --static-call --uaccess --prefix=16  --link  --module nvfs_selftest.o

nvfs_selftest.o: $(wildcard /usr/src/kernels/6.17.4-200.fc42.x86_64/tools/objtool/objtool)
