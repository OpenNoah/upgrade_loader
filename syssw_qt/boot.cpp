#include <stdint.h>
#include <string.h>
#include "boot.h"
#include "bootdialog.h"

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

	QList<QDir> ext2;
	ext2.setAutoDelete(true);
	ext2.append(new QDir("/dev", "mmcblk*", QDir::Name, QDir::System));
	ext2.append(new QDir("/mnt/mmc", "rootfs*.img", QDir::Name, QDir::Files));
	ext2.append(new QDir("/mnt/mmc/rootfs", "*.img", QDir::Name, QDir::Files));
	for (QDir *dir = ext2.first(); dir != 0; dir = ext2.next()) {
		const QFileInfoList *list = dir->entryInfoList();
		if (list == 0)
			continue;
		QFileInfo *fi;
		for (QFileInfoListIterator it(*list); (fi=it.current()); ++it) {
			QString path(fi->absFilePath());
			cmd = QString("mount -t ext2 %1 /mnt/root").arg(path);
			if (BootDialog::BootCheck(cmd)) {
				names << path;
				mounts << cmd;
				offsets.append(0);
			}
		}
	}

	QList<QDir> upd;
	upd.setAutoDelete(true);
	upd.append(new QDir("/mnt/mmc", "upgrade*.bin", QDir::Name, QDir::Files));
	upd.append(new QDir("/mnt/mmc", "rootfs*.bin", QDir::Name, QDir::Files));
	for (QDir *dir = upd.first(); dir != 0; dir = upd.next()) {
		const QFileInfoList *list = dir->entryInfoList();
		if (list == 0)
			continue;
		QFileInfo *fi;
		for (QFileInfoListIterator it(*list); (fi=it.current()); ++it) {
			QString path(fi->absFilePath());
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
