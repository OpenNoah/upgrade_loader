#######################################################
# Makefile for upgrade.bin
# Suitable for: NP1380 <1.6.0 template>
# Created by zhiyb @ GT-Soft
#

PKGCFG = pkg.cfg
LEBSIZE = 516096
PAGESIZE = 4096

MKPKG = ./mkpkg/mkpkg

CLEAN	= upgrade.bin
upgrade.bin: $(PKGCFG) $(MKPKG) uImage initfs.bin
	$(MKPKG) --type=np1000 --create $< $@

./mkpkg/mkpkg:
	cd mkpkg; $(MAKE) mkpkg

.DELETE_ON_ERROR:

CLEAN	+= init.tar.bz2
init.tar.bz2:
	find rootfs/ -name '.*~' -delete
	tar jcvf $@ -C rootfs/ .

CLEAN	+= initramfs.gz
initramfs.gz: initramfs
	(cd initramfs; find . -print | cpio -o -H newc -R 0:0) | gzip -9 > $@

CLEAN	+= initfs.bin
initfs.bin:
	sudo ./initfs.sh

CLEAN	+= vmlinux.bin.head
vmlinux.bin.head: vmlinux.bin
	dd if=$< of=$@ count=1 bs=$(shell bash -c "grep -ab -m 1 -o -F $$'\x1F\x8B\x08' $< | head -1 | cut -f 1 -d :")

CLEAN	+= vmlinux.bin.new
vmlinux.bin.new: vmlinux.bin.head initramfs.gz vmlinux.bin.zero
	cat $^ > $@

CLEAN	+= vmlinux.bin.zero
vmlinux.bin.zero: vmlinux.bin vmlinux.bin.head initramfs.gz
	dd if=/dev/zero of=$@ count=1 bs=$$(($(shell stat -c %s $<) - $(shell stat -c %s $(word 2,$^)) - $(shell stat -c %s $(word 3,$^))))

CLEAN	+= uImage.gz
uImage.gz: vmlinux.bin.new
	gzip -c $< -9 > $@

CLEAN	+= uImage
uImage: uImage.gz
	mkimage -A mips -O linux -T kernel -C gzip -a 80010000 -e 802e3800 -n Linux-2.6.24.3 -d $< $@

.PHONY: clean
clean:
	rm -f $(CLEAN) $(TARG)
	cd mkpkg; $(MAKE) clean
