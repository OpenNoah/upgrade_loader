#!/bin/bash -x
mkdir -p rootfs
umount -l rootfs

set -e
dd if=/dev/zero of=initfs.bin bs=40M count=1
mke2fs -F initfs.bin
mount -t ext2 -o rw initfs.bin rootfs
cp -a initfs/* rootfs/
df -h rootfs
umount -l rootfs
rmdir rootfs
