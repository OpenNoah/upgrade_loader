#!/bin/bash -x
base=$1
img=$base.bin

mkdir -p rootfs_$img
umount -l rootfs_$img

set -e
dd if=/dev/zero of=$img bs=48M count=1
mke2fs -F $img
mount -t ext2 -o rw $img rootfs_$img
cp -a $base/* rootfs_$img/
df -h rootfs_$img
umount -l rootfs_$img
rmdir rootfs_$img
