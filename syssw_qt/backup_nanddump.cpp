/*
 *  nanddump.c
 *
 *  Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                     Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This utility dumps the contents of raw NAND chips or NAND
 *   chips contained in DoC devices.
 */

//#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>

#include <string>
#include <vector>

#define PROGRAM "nanddump"
#define VERSION "$Revision: 1.1.1.1 $"

static struct nand_oobinfo none_oobinfo;

// Option variables

static int	pretty_print;		// print nice in ascii
static int	noecc;			// don't error correct
static int	omitoob;		// omit oob data
static unsigned long	start_addr;	// start address
static unsigned long	length;		// dump length
static const char *mtddev;		// mtd device name
static char    *dumpfile;		// dump file name
static int	omitbad;

/*
 * Buffers for reading data from flash
 */
static unsigned char readbuf[8192];
static unsigned char oobbuf[256];

/*
 * Interface to C++
 */
static int ret, i, fd, bs, badblock;
static struct mtd_oob_buf oob;
static mtd_info_t meminfo;
static int oobinfochanged;
static struct nand_oobinfo old_oobinfo;
static struct mtd_ecc_stats stat1, stat2;
static int eccstats;
static unsigned long ofs, end_addr;
static unsigned long long blockstart;

int nanddump_open(std::string &serr, const char *devpath)
{
	int ret;
	serr.clear();

	none_oobinfo.useecc = MTD_NANDECC_OFF;

	badblock = 0;
	oobinfochanged = 0;
	eccstats = 0;
	end_addr = 0;
	oob = (struct mtd_oob_buf){0, 16, oobbuf};
	blockstart = 1;

	// Options
	mtddev = devpath;
	omitoob = 0;

	/* Open MTD device */
	if ((fd = open(mtddev, O_RDONLY)) == -1) {
		serr = "open flash";
		return -1;
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		close(fd);
		serr = "MEMGETINFO";
		return -1;
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 256 && meminfo.writesize == 8192) &&
	    !(meminfo.oobsize == 128 && meminfo.writesize == 4096) &&
	    !(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
	    !(meminfo.oobsize == 32 && meminfo.writesize == 1024) &&
	    !(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
	    !(meminfo.oobsize == 8 && meminfo.writesize == 256)) {
		close(fd);
		serr = "Unknown flash (not normal NAND)";
		return -1;
	}

	/* Read the real oob length */
	oob.length = meminfo.oobsize;

	if (noecc)  {
		ret = ioctl(fd, MTDFILEMODE, (void *) MTD_MODE_RAW);
		if (ret == 0) {
			oobinfochanged = 2;
		} else {
			switch (errno) {
			case ENOTTY:
				if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
					close (fd);
					serr = "MEMGETOOBSEL";
					return -1;
				}
				if (ioctl (fd, MEMSETOOBSEL, &none_oobinfo) != 0) {
					close (fd);
					serr = "MEMSETOOBSEL";
					return -1;
				}
				oobinfochanged = 1;
				break;
			default:
				close (fd);
				serr = "MTDFILEMODE";
				return -1;
			}
		}
	} else {

		/* check if we can read ecc stats */
		if (!ioctl(fd, ECCGETSTATS, &stat1)) {
			eccstats = 1;
			fprintf(stderr, "ECC failed: %d\n", stat1.failed);
			fprintf(stderr, "ECC corrected: %d\n", stat1.corrected);    
			fprintf(stderr, "Number of bad blocks: %d\n", stat1.badblocks);    
			fprintf(stderr, "Number of bbt blocks: %d\n", stat1.bbtblocks);    
		} else
			serr = "No ECC status information available";
	}

	/* Initialize start/end addresses and block size */
	if (length)
		end_addr = start_addr + length;
	if (!length || end_addr > meminfo.size)
		end_addr = meminfo.size;

	/* Print informative message */
	fprintf(stderr, "Block size %u, page size %u, OOB size %u\n",
			meminfo.erasesize, meminfo.writesize, meminfo.oobsize);
	fprintf(stderr,
			"Dumping data starting at 0x%08x and ending at 0x%08x...\n",
			(unsigned int) start_addr, (unsigned int) end_addr);

	bs = meminfo.writesize;

	/* Dump the flash contents */
	ofs = start_addr;

	return 0;
}

int nanddump_dump(std::string &serr, std::vector<char> &dump_buf)
{
	char pretty_buf[80];

	serr.clear();
	dump_buf.clear();

	/* Dump the flash contents */
	if (ofs >= end_addr)
		return 1;

		// new eraseblock , check for bad block
		if (blockstart != (ofs & (~meminfo.erasesize + 1))) {
			blockstart = ofs & (~meminfo.erasesize + 1);
			if ((badblock = ioctl(fd, MEMGETBADBLOCK, &blockstart)) < 0) {
				serr = "ioctl(MEMGETBADBLOCK)";
				return -1;
			}
		}

		if (badblock) {
			if (omitbad)
				goto cont;
			memset (readbuf, 0xff, bs);
		} else {
			/* Read page data and exit on failure */
			if (pread(fd, readbuf, bs, ofs) != bs) {
				serr = "pread";
				return -1;
			}
		}

		/* ECC stats available ? */
		if (eccstats) {
			if (ioctl(fd, ECCGETSTATS, &stat2)) {
				serr = "ioctl(ECCGETSTATS)";
				return -1;
			}
			if (stat1.failed != stat2.failed)
				fprintf(stderr, "ECC: %d uncorrectable bitflip(s)"
						" at offset 0x%08lx\n",
						stat2.failed - stat1.failed, ofs);
			if (stat1.corrected != stat2.corrected)
				fprintf(stderr, "ECC: %d corrected bitflip(s) at"
						" offset 0x%08lx\n",
						stat2.corrected - stat1.corrected, ofs);
			stat1 = stat2;
		}

		/* Write out page data */
		dump_buf.insert(dump_buf.end(), readbuf, readbuf + bs);
		if (pretty_print) {
			for (i = 0; i < bs; i += 16) {
				sprintf(pretty_buf,
						"0x%08x: %02x %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						(unsigned int) (ofs + i),  readbuf[i],
						readbuf[i+1], readbuf[i+2],
						readbuf[i+3], readbuf[i+4],
						readbuf[i+5], readbuf[i+6],
						readbuf[i+7], readbuf[i+8],
						readbuf[i+9], readbuf[i+10],
						readbuf[i+11], readbuf[i+12],
						readbuf[i+13], readbuf[i+14],
						readbuf[i+15]);
				write(STDERR_FILENO, pretty_buf, 60);
			}
		}

		if (omitoob)
			goto cont;

		if (badblock) {
			memset (readbuf, 0xff, meminfo.oobsize);
		} else {
			/* Read OOB data and exit on failure */
			oob.start = ofs;
			if (ioctl(fd, MEMREADOOB, &oob) != 0) {
				serr = "ioctl(MEMREADOOB)";
				return -1;
			}
		}

		/* Write out OOB data */
		dump_buf.insert(dump_buf.end(), oobbuf, oobbuf + meminfo.oobsize);
		if (pretty_print) {
			if (meminfo.oobsize < 16) {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
						"%02x %02x\n",
						oobbuf[0], oobbuf[1], oobbuf[2],
						oobbuf[3], oobbuf[4], oobbuf[5],
						oobbuf[6], oobbuf[7]);
				write(STDERR_FILENO, pretty_buf, 48);
				goto cont;
			}

			for (i = 0; i < meminfo.oobsize; i += 16) {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
						"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						oobbuf[i], oobbuf[i+1], oobbuf[i+2],
						oobbuf[i+3], oobbuf[i+4], oobbuf[i+5],
						oobbuf[i+6], oobbuf[i+7], oobbuf[i+8],
						oobbuf[i+9], oobbuf[i+10], oobbuf[i+11],
						oobbuf[i+12], oobbuf[i+13], oobbuf[i+14],
						oobbuf[i+15]);
				write(STDERR_FILENO, pretty_buf, 60);
			}
		}

cont:
	ofs += bs;
	return 0;
}

void nanddump_close()
{
	/* The new mode change is per file descriptor ! */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0)  {
			perror ("MEMSETOOBSEL");
		}
	}
	close(fd);
}
