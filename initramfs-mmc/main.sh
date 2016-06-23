#!/bin/sh -e
export PATH=$PATH:/usr/local/sbin
insmod /lib/modules/ubi.ko
insmod /lib/modules/ubifs.ko
#modprobe ubifs

# Mount memory card
mount -t vfat /dev/mmcblk0p1 /mnt/mmc
[ -e /mnt/mmc/rootfs/init.tar.bz2 ]

# Generate a single rootfs
ubiformat /dev/mtd3
ubiattach /dev/ubi_ctrl -m 3
ubimkvol /dev/ubi0 -N rootfs -m

# Reattach ubifs
#ubidetach /dev/ubi_ctrl -m 3
#ubiattach /dev/ubi_ctrl -m 3

# Mount filesystems
mount -t ubifs ubi0:rootfs /mnt/UsrDisk

# Extract rootfs
tar jxvf /mnt/mmc/rootfs/init.tar.bz2 -C /mnt/UsrDisk

# Cleanup
umount -l /mnt/UsrDisk
umount -l /mnt/mmc
