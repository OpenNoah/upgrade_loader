#######################################################
# Makefile for upgrade.bin
# Suitable for: NP1380 <1.6.0 template>
# Created by zhiyb @ GT-Soft
#

PKGCFG = pkg.cfg
LEBSIZE = 516096
PAGESIZE = 4096

TARG = upgrade.bin
DIRS = 
INIT = -mmc
IMGS = $(addsuffix .img, $(DIRS))
OBJS = $(addsuffix .ref, $(DIRS))

MKUBIFS = ./mkfs.ubifs
UBIREFIMG = ./ubirefimg
MKPKG = ./mkpkg

.PHONY: $(INIT:%=initramfs%)
$(TARG): $(PKGCFG) $(INIT:%=uImage%) $(OBJS)
	$(MKPKG) $<
	mv *_*.bin $@

.PHONY: mnt
mnt: $(TARG) init.tar.bz2
	mkdir -p mnt
	mount -t vfat /dev/sdd1 mnt
	cp -a $(TARG) mnt/
	mkdir -p mnt/rootfs
	cp -a init.tar.bz2 mnt/rootfs/
	umount -l mnt
	rmdir mnt

CLEAN += init.tar.bz2
init.tar.bz2:
	find rootfs/ -name '.*~' -delete
	tar jcvf $@ -C rootfs/ .

%.img: %
	$(MKUBIFS) -r $(basename $@) -m $(PAGESIZE) -e $(LEBSIZE) -o $@

%.ref: %.img
	$(UBIREFIMG) $< $@

CLEAN += $(INIT:%=initramfs%.gz)
initramfs%.gz:
	$(MAKE) initramfs$*

$(INIT:%=initramfs%):
	(cd $@; find -name '.*~' -delete; find . | cpio -o -H newc -R 0:0) | gzip -9 > $@.gz

CLEAN	+= vmlinux.bin.head
vmlinux.bin.head: vmlinux.bin
	dd if=$< of=$@ count=1 bs=$(shell bash -c "grep -ab -m 1 -o -F $$'\x1F\x8B\x08' $< | head -1 | cut -f 1 -d :")

CLEAN	+= $(INIT:%=vmlinux%.bin.new)
vmlinux%.bin.new: vmlinux.bin.head initramfs%.gz vmlinux%.bin.zero
	cat $^ > $@

CLEAN	+= $(INIT:%=vmlinux%.bin.zero)
vmlinux%.bin.zero: vmlinux.bin vmlinux.bin.head initramfs%.gz
	dd if=/dev/zero of=$@ count=1 bs=$$(($(shell stat -c %s $<) - $(shell stat -c %s $(word 2,$^)) - $(shell stat -c %s $(word 3,$^))))

CLEAN	+= $(INIT:%=uImage%.gz)
uImage%.gz: vmlinux%.bin.new
	gzip -c $< -9 > $@

CLEAN	+= $(INIT:%=uImage%)
uImage%: uImage%.gz
	mkimage -A mips -O linux -T kernel -C gzip -a 80010000 -e 802e3800 -n Linux-2.6.24.3 -d $< $@

.PHONY: clean
clean:
	rm -f $(CLEAN) $(IMGS) $(OBJS) $(TARG)
