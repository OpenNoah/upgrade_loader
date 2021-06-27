/* eraseall.c -- erase the whole of a MTD device

   Copyright (C) 2000 Arcom Control System Ltd

   Renamed to flash_eraseall.c

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#include <sys/types.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <libgen.h>
#include <ctype.h>
#include <time.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#include <string>

#include <mtd/mtd-user.h>

#define PROGRAM "flash_eraseall"
#define VERSION "$Revision: 1.1.1.1 $"

static const char *mtd_device;
static int quiet;		/* true -- don't output progress */

static void process_options (int argc, char *argv[]);
static void display_help (void);
static void display_version (void);
int target_endian = __BYTE_ORDER;

/*
 * Interface to C++
 */
static int fd;
static mtd_info_t meminfo;
static erase_info_t erase;
static int isNAND;

int eraseall_open(std::string &serr, const char *devpath)
{
	serr.clear();

	// Options
	mtd_device = devpath;

	if ((fd = open(mtd_device, O_RDWR)) < 0) {
		serr = strerror(errno);
		return -1;
	}

	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		serr = "unable to get MTD device info";
		return -1;
	}

	erase.length = meminfo.erasesize;
	isNAND = meminfo.type == MTD_NANDFLASH ? 1 : 0;

	erase.start = 0;
	return 0;
}

int eraseall_erase(std::string &serr, int &esize)
{
	serr.clear();

	int bbtest = 1;
	esize = meminfo.erasesize;

	if (erase.start >= meminfo.size)
		return 1;

		if (bbtest) {
			unsigned long long offset = erase.start;
			int ret = ioctl(fd, MEMGETBADBLOCK, &offset);
			if (ret > 0) {
				if (!quiet)
					printf ("\nSkipping bad block at 0x%09llx\n", erase.start);
				goto cont;
			} else if (ret < 0) {
				if (errno == EOPNOTSUPP) {
					bbtest = 0;
					if (isNAND) {
						serr = "Bad block check not available";
						return -1;
					}
				} else {
					serr = std::string("MTD get bad block failed: ") + strerror(errno);
					return -1;
				}
			}
		}

		if (!quiet) {
			printf
				("\rErasing %d Kibyte @ %llx -- %2u %% complete.",
				 meminfo.erasesize / 1024, erase.start,
				 (unsigned long long)
				 (erase.start + meminfo.erasesize) * 100 / meminfo.size);
		}
		fflush(stdout);

		if (ioctl(fd, MEMERASE, &erase) != 0) {
			fprintf(stderr, "\n%s: MTD Erase failure: %s\n", mtd_device, strerror(errno));
			goto cont;
		}

cont:
	erase.start += meminfo.erasesize;
	return 0;
}

void eraseall_close()
{
	close(fd);
}
