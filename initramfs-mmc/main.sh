#!/bin/sh -e
export PATH=$PATH:/usr/local/sbin
insmod /lib/modules/ubi.ko
insmod /lib/modules/ubifs.ko
#modprobe ubifs

# Mount memory card
#mount -t vfat /dev/mmcblk0p1 /mnt/mmc

nand_write()
{
	[ -e $2 ] || return 0
	echo "Press any key in 2 seconds to skip writing $2 to $1."
	read -s -n 1 -t 2 var && return || true
	flash_eraseall $1 && nandwrite -p $1 $2 || true
}

# Write u-boot, uImage, and/or upgrade.bin
nand_write /dev/mtd0 /mnt/mmc/u-boot-nand.bin
nand_write /dev/mtd2 /mnt/mmc/uImage
if [ -e /mnt/mmc/upgrade.bin ]; then
	echo "Press any key in 2 seconds to flash upgrade.bin."
	read -s -n 1 -t 2 var && upgrade /etc/upgrade.cfg /mnt/mmc/upgrade.bin || true
fi

echo "Press any key in 2 seconds to skip rootfs rebuilding."
read -s -n 1 -t 2 var && exit 0 || true

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
[ -e /mnt/mmc/rootfs/init.tar.bz2 ] && tar jxvf /mnt/mmc/rootfs/init.tar.bz2 -C /mnt/UsrDisk || true

# Cleanup
umount -l /mnt/UsrDisk
#umount -l /mnt/mmc
