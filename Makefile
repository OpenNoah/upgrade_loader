#######################################################
# Makefile for upgrade.bin
# Suitable for: NP1380 <1.6.0 template>
# Created by zhiyb @ GT-Soft
#

TYPE	?= np1380

PKGCFG	= $(TYPE)/pkg.cfg
BIN	= upgrade_$(TYPE).bin

MKPKG	= ./mkpkg/mkpkg

CLEAN	= $(BIN)
$(BIN): $(PKGCFG) $(MKPKG) uImage initfs.bin loaderfs.bin build-ipkg
	$(MKPKG) --type=np1000 --create $< $@

ENTRY	= $(shell mkimage -l $(TYPE)/uImage-base | grep 'Entry' | awk '{print $$3}')

./mkpkg/mkpkg:
	$(MAKE) -C mkpkg mkpkg

.DELETE_ON_ERROR:

.PHONY: build-ipk
CLEAN	+= $(wildcard sysloader_*.ipk)
build-ipkg:
	opkg-build -c -o root -Z gzip ipkg

CLEAN	+= init.tar.bz2
init.tar.bz2:
	find rootfs/ -name '.*~' -delete
	tar jcvf $@ -C rootfs/ .

CLEAN	+= initramfs.gz
initramfs.gz: initramfs
	(cd initramfs; find . -print | cpio -o -H newc -R 0:0) | gzip -9 > $@

CLEAN	+= initfs.bin loaderfs.bin
initfs.bin loaderfs.bin: %.bin:
	$(MAKE) -C syssw_qt
	cp syssw_qt/syssw $*/usr/bin/syssw
	sudo ./initfs.sh $*

CLEAN	+= vmlinux.bin
vmlinux.bin: $(TYPE)/uImage-base
	tail -c+65 $< | zcat > $@

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
	mkimage -A mips -O linux -T kernel -C gzip -a 80010000 -e $(ENTRY) -n Linux-2.6.24.3 -d $< $@

.PHONY: clean
clean:
	rm -f $(CLEAN) $(TARG)
	$(MAKE) -C mkpkg clean
	$(MAKE) -C syssw_qt clean
