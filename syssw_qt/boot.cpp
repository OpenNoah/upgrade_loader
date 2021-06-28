#include <sys/types.h>
#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include "boot.h"
#include "bootdialog.h"

#include <iostream>

Boot::Boot(QWidget *parent): QWidget(parent)
{
	QBoxLayout *lay = new QBoxLayout(this, QBoxLayout::LeftToRight);
	QVButtonGroup *btns = new QVButtonGroup(this);
	lay->addWidget(btns);

	QString cmd("mount -t ubifs ubi0:rootfs /mnt/root");
	if (BootDialog::BootCheck(cmd)) {
		names << "ubi0:rootfs";
		mounts << cmd;
		offsets.append(0);
	}

	static struct {
		const char * const dir;
		QRegExp match;
	} ext2list[] = {
		{"/dev", QRegExp("mmcblk.*")},
		{"/mnt/mmc", QRegExp("rootfs.*\\.img")},
		{"/mnt/mmc/rootfs", QRegExp(".*\\.img")},
		{0, QRegExp()},
	};
	for (int i = 0; ext2list[i].dir != 0; i++) {
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(ext2list[i].dir)) == 0)
			continue;
		while ((ent = readdir(dir)) != 0) {
			QString fname(ent->d_name);
			if (fname.startsWith("."))
				continue;
			if (ext2list[i].match.find(fname, 0) == -1)
				continue;
			QString path(QString("%1/%2").arg(ext2list[i].dir).arg(fname));
			cmd = QString("mount -t ext2 -o noatime %1 /mnt/root").arg(path);
			if (BootDialog::BootCheck(cmd)) {
				names << path;
				mounts << cmd;
				offsets.append(0);
			}
		}
		closedir(dir);
	}

	static struct {
		const char * const dir;
		QRegExp match;
	} updlist[] = {
		{"/mnt/mmc", QRegExp("upgrade.*\\.bin")},
		{"/mnt/mmc", QRegExp("rootfs.*\\.bin")},
		{"/mnt/mmc/rootfs", QRegExp(".*\\.bin")},
		{0, QRegExp()},
	};
	for (int i = 0; updlist[i].dir != 0; i++) {
		DIR *dir;
		struct dirent *ent;
		if ((dir = opendir(updlist[i].dir)) == 0)
			continue;
		while ((ent = readdir(dir)) != 0) {
			QString fname(ent->d_name);
			if (fname.startsWith("."))
				continue;
			if (updlist[i].match.find(fname, 0) == -1)
				continue;
			QString path(QString("%1/%2").arg(updlist[i].dir).arg(fname));
			unsigned long off = offset(path);
			if (off == 0)
				continue;
			cmd = path;
			if (BootDialog::BootCheck(cmd, off)) {
				names << path;
				mounts << cmd;
				offsets.append(off);
			}
		}
		closedir(dir);
	}

	for (unsigned int i = 0; i < names.count(); i++)
		new QPushButton(names[i], btns);
	connect(btns, SIGNAL(clicked(int)), this, SLOT(boot(int)));
}

void Boot::boot(int id)
{
	new BootDialog(mounts[id], offsets[id], true, this);
}

void Boot::codec(void *p, unsigned long size)
{
	uint64_t *pv = (uint64_t *)p;
	while (size) {
		uint64_t v = *pv;
		// Swap every 2 bits
		*pv++ = ((v & 0xaaaaaaaaaaaaaaaaULL) >> 1) | ((v & 0x5555555555555555ULL) << 1);
		size -= 8;
	}
}

unsigned long Boot::offset(QString path)
{
#pragma pack(push, 1)
	struct header_t {
		union {
			struct {
				char tag[8];
				uint32_t ver;
			};
			uint8_t _blk[64];
		};
		union pkg_t {
			struct {
				uint32_t size;
				uint32_t offset;
				uint32_t ver;
				uint32_t fstype;
				uint32_t crc;
				char dev[64 - 4 * 5];
			};
			uint8_t _blk[64];
		} pkg[31];
	};
#pragma pack(pop)

	QFile f(path);
	if (!f.open(IO_ReadOnly))
		return 0;
	header_t header;
	if (f.readBlock((char *)&header, sizeof(header)) != sizeof(header))
		return 0;
	codec((void *)&header, sizeof(header));
	for (unsigned int i = 0; i < 31; i++) {
		header_t::pkg_t &pkg = header.pkg[i];
		if (pkg.size == 0)
			continue;
		if (strncmp(pkg.dev, "/", sizeof(pkg.dev)) == 0)
			return pkg.offset;
	}
	return 0;
}
