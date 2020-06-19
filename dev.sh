#!/bin/bash
mkdir -p initramfs/dev
mknod initramfs/dev/null c 1 3
mknod initramfs/dev/console c 5 1

mkdir -p initfs/dev
mknod initfs/dev/null c 1 3
mknod initfs/dev/console c 5 1
