/*
 *  nandwrite.c
 *
 *  Copyright (C) 2000 Steven J. Hill (sjhill@realitydiluted.com)
 *		  2003 Thomas Gleixner (tglx@linutronix.de)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Overview:
 *   This utility writes a binary image directly to a NAND flash
 *   chip or NAND chips contained in DoC devices. This is the
 *   "inverse operation" of nanddump.
 *
 * tglx: Major rewrite to handle bad blocks, write data with or without ECC
 *	 write oob data only on request
 *
 * Bug/ToDo:
 */

//#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <getopt.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>

#include <string>
#include <vector>

#define PROGRAM "nandwrite"
#define VERSION "$Revision: 1.1.1.1 $"

#define MAX_PAGE_SIZE	8192
#define MAX_OOB_SIZE	256

/*
 * Buffer array used for writing data
 */
static unsigned char writebuf[MAX_PAGE_SIZE];
static unsigned char oobreadbuf[MAX_OOB_SIZE];

// oob layouts to pass into the kernel as default
static struct nand_oobinfo none_oobinfo;

static struct nand_oobinfo autoplace_oobinfo;

static void init_structs()
{
	none_oobinfo.useecc = MTD_NANDECC_OFF;
	autoplace_oobinfo.useecc = MTD_NANDECC_AUTOPLACE;
	autoplace_oobinfo.eccbytes = 36;
}

static const char *mtd_device;
static int	mtdoffset = 0;
static int	quiet = 0;
static int	writeoob = 0;
static int	markbad = 1;
static int	autoplace = 0;
static int	forcelegacy = 0;
static int	noecc = 0;
static int	pad = 0;
static int	blockalign = 1; /*default to using 16K block size */

/*
 * Interface to C++
 */
static int cnt, fd, pagelen, baderaseblock, blockstart;
static int oobinfochanged;
static struct mtd_info_user meminfo;
static struct nand_oobinfo old_oobinfo;
static struct mtd_page_buf oob;

int nandwrite_open(std::string &serr, const char *devpath)
{
	serr.clear();
	init_structs();

	blockstart = -1;
	oobinfochanged = 0;
	meminfo = (struct mtd_info_user){0};

	// Options
	mtd_device = devpath;
	mtdoffset = 0;
	writeoob = 1;
	pad = 0;

	if (pad && writeoob) {
		serr = "Can't pad when oob data is present.";
		return -1;
	}

	/* Open the device */
	if ((fd = open(mtd_device, O_RDWR)) == -1) {
		serr = "open flash";
		return -1;
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		serr = "MEMGETINFO";
		close(fd);
		return -1;
	}

	/* Set erasesize to specified number of blocks - to match jffs2
	 * (virtual) block size */
	meminfo.erasesize *= blockalign;

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
			!(meminfo.oobsize == 8 && meminfo.writesize == 256) &&
			!(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
			!(meminfo.oobsize == 128 && meminfo.writesize == 4096) &&
			!(meminfo.oobsize == 256 && meminfo.writesize == 8192)) {
		serr = "Unknown flash (not normal NAND)";
		close(fd);
		return -1;
	}

	if (autoplace) {
		/* Read the current oob info */
		if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
			serr = "MEMGETOOBSEL";
			close (fd);
			return -1;
		}

		// autoplace ECC ?
		if (autoplace && (old_oobinfo.useecc != MTD_NANDECC_AUTOPLACE)) {

			if (ioctl (fd, MEMSETOOBSEL, &autoplace_oobinfo) != 0) {
				serr = "MEMSETOOBSEL";
				close (fd);
				return -1;
			}
			oobinfochanged = 1;
		}
	}

	memset(oobreadbuf, 0xff, MAX_OOB_SIZE);

	if (autoplace) {
		oob.ooblength = meminfo.oobsize-old_oobinfo.eccbytes; /* Get ooblength from kernel */
		printf("oobsize=%d eccbytes=%d\n", meminfo.oobsize, old_oobinfo.eccbytes);
	} else {
		oob.ooblength = meminfo.oobsize-autoplace_oobinfo.eccbytes;
		printf("oobsize=%d eccbytes=%d\n", meminfo.oobsize, autoplace_oobinfo.eccbytes);
	}

	oob.oobptr = oobreadbuf;
	oob.datptr = writebuf;

	pagelen = meminfo.writesize + ((writeoob == 1) ? meminfo.oobsize : 0);

#if 0
	// get image length
	imglen = lseek(ifd, 0, SEEK_END);
	lseek (ifd, 0, SEEK_SET);

	// Check, if length fits into device
	if ( ((imglen / pagelen) * meminfo.writesize) > (meminfo.size - mtdoffset)) {
		fprintf (stderr, "Image %d bytes, NAND page %d bytes, OOB area %u bytes, device size %lld bytes\n",
				imglen, pagelen, meminfo.writesize, meminfo.size);
		perror ("Input file does not fit into device");
		goto closeall;
	}
#endif

	/* Get data from input and write to the device */
	return 0;
}

int nandwrite_pagelen()
{
	return pagelen;
}

int nandwrite_write(std::string &serr, const std::vector<char> &write_buf)
{
retry:
	/* Get data from input and write to the device */
	int imglen = write_buf.size();

	// Check, if file is pagealigned
	if ((!pad) && ((imglen % pagelen) != 0)) {
		serr = "Input file is not page aligned";
		return -1;
	}

	if (mtdoffset >= meminfo.size)
		return 1;

		// new eraseblock , check for bad block(s)
		// Stay in the loop to be sure if the mtdoffset changes because
		// of a bad block, that the next block that will be written to
		// is also checked. Thus avoiding errors if the block(s) after the
		// skipped block(s) is also bad (number of blocks depending on
		// the blockalign
		while (blockstart != (mtdoffset & (~meminfo.erasesize + 1))) {
			blockstart = mtdoffset & (~meminfo.erasesize + 1);
			loff_t offs = blockstart;
			baderaseblock = 0;
			if (!quiet)
				fprintf (stdout, "Writing data to block 0x%x\n", blockstart);

			/* Check all the blocks in an erase block for bad blocks */
			do {
				int ret;
				if ((ret = ioctl(fd, MEMGETBADBLOCK, &offs)) < 0) {
					serr = "ioctl(MEMGETBADBLOCK)";
					return -1;
				}
				if (ret == 1) {
					baderaseblock = 1;
					if (!quiet)
						fprintf (stderr, "Bad block at 0x%llx, %u block(s) "
								"from 0x%x will be skipped\n",
							 offs, blockalign, blockstart);
				}

				if (baderaseblock) {
					mtdoffset = blockstart + meminfo.erasesize;
				}
				offs +=  meminfo.erasesize / blockalign ;
			} while ( offs < blockstart + meminfo.erasesize );

		}
      
		int readlen = meminfo.writesize;
		if (pad && (imglen < readlen))
		{
			readlen = imglen;
			memset(writebuf + readlen, 0xff, meminfo.writesize - readlen);
		}

		/* Read Page Data from input file */
		memcpy(writebuf, &write_buf.front(), readlen);

		/* Read OOB data from input file, exit on failure */
		if(writeoob) {
			memcpy(oobreadbuf, &write_buf.front() + readlen, meminfo.oobsize);
		}
		oob.start = mtdoffset;

		// write a page include its oob to nand
		ioctl(fd, MEMWRITEPAGE, &oob);
		if(oob.datlength != meminfo.writesize){
			perror ("ioctl(MEMWRITEPAGE)");

			int rewind_blocks;
			off_t rewind_bytes;
			erase_info_t erase;

			/* Must rewind to blockstart if we can */
			rewind_blocks = (mtdoffset - blockstart) / meminfo.writesize; /* Not including the one we just attempted */
			rewind_bytes = (rewind_blocks * meminfo.writesize) + readlen;
			if (writeoob)
				rewind_bytes += (rewind_blocks + 1) * meminfo.oobsize;
			erase.start = blockstart;
			erase.length = meminfo.erasesize;
			fprintf(stderr, "Erasing failed write from 0x%09llx-0x%09llx\n",
				erase.start, erase.start+erase.length-1);
			if (ioctl(fd, MEMERASE, &erase) != 0) {
				serr = "MEMERASE";
				return -1;
			}

			if (markbad) {
				loff_t bad_addr = mtdoffset & (~(meminfo.erasesize / blockalign) + 1);
				fprintf(stderr, "Marking block at %09llx bad\n", (long long)bad_addr);
				if (ioctl(fd, MEMSETBADBLOCK, &bad_addr)) {
					perror("MEMSETBADBLOCK");
					/* But continue anyway */
				}
			}
			mtdoffset = blockstart + meminfo.erasesize;
			imglen += rewind_blocks * meminfo.writesize;

			goto retry;
		}

		if(writeoob)
			imglen -= meminfo.oobsize;
		imglen -= readlen;
		mtdoffset += meminfo.writesize;

	return 0;
}

void nandwrite_close()
{
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMSETOOBSEL");
			close (fd);
			exit (1);
		}
	}

	close(fd);
}
