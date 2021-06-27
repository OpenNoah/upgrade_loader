#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cstdlib>
#include <string>
#include <vector>
#include "backup.h"

#define SMALL_BUFFER_SIZE	(32 * 1024)
#define BUFFER_SIZE		(1024 * 1024)

Backup::Backup(QWidget *parent): QWidget(parent)
{
	dev.setAutoDelete(true);

	QFile f("/proc/mtd");
	if (f.open(IO_ReadOnly)) {
		QTextStream ts(&f);
		ts.readLine();
		while (!ts.eof()) {
			QString devname, size, esize, name;
			ts >> devname >> size >> esize;
			name = ts.readLine().replace(QRegExp("\""), "").stripWhiteSpace();
			QString path = "/dev/" + devname.left(devname.length() - 1);

			dev_t d;
			d.blkdev = path;
			d.name = name;
			d.size = size.toULong(0, 16);
			d.mtd = true;
			if (QFile(d.blkdev).exists())
				dev.append(new dev_t(d));
		}
		f.close();
	}

	system("ubinfo /dev/ubi0 | fgrep 'Volumes count' | grep -o '[0-9]\\+' > /tmp/stdout");
	f.setName("/tmp/stdout");
	if (f.open(IO_ReadOnly)) {
		QTextStream ts(&f);
		int num = ts.readLine().toInt();
		f.close();

		for (int v = 0; v < num; v++) {
			QString path(QString("/dev/ubiblock%1").arg(v));
			if (!QFile(path).exists())
				continue;
			QString cmd(QString("ubinfo /dev/ubi0_%1 | grep '\\(Size\\|Name\\)' > /tmp/stdout").arg(v));
			system(cmd.latin1());
			if (f.open(IO_ReadOnly)) {
				QTextStream ts(&f);
				QString sinfo, ssize;
				ts >> sinfo >> sinfo >> sinfo >> ssize;
				ssize = ssize.mid(1);
				ts.readLine();
				ts >> sinfo;
				QString name = ts.readLine().stripWhiteSpace();

				dev_t d;
				d.blkdev = path;
				d.name = name;
				d.size = ssize.toULong(0, 10);
				d.mtd = false;
				dev.append(new dev_t(d));
			}
		}
	}

	QVBoxLayout *vlay = new QVBoxLayout(this, 0, 4);

	lbBlkdev = new QListBox(this);
	for (dev_t *d = dev.first(); d != 0; d = dev.next())
		lbBlkdev->insertItem(tr("%1: %2").arg(d->blkdev).arg(d->name));
	vlay->addWidget(lbBlkdev);

	pbProgress = new QProgressBar(100, this);
	pbProgress->setCenterIndicator(true);
	pbProgress->setProgress(0);
	vlay->addWidget(pbProgress);

	QHBoxLayout *hlay = new QHBoxLayout();
	QPushButton *pbBackup = new QPushButton(tr("备份"), this);
	hlay->addWidget(pbBackup);
	pbRestore = new QPushButton(tr("恢复"), this);
	hlay->addWidget(pbRestore);
	QPushButton *pbVerify = new QPushButton(tr("验证"), this);
	hlay->addWidget(pbVerify);
	vlay->addLayout(hlay);

	connect(pbBackup, SIGNAL(clicked()), this, SLOT(backup()));
	connect(pbRestore, SIGNAL(clicked()), this, SLOT(restore()));
	connect(pbVerify, SIGNAL(clicked()), this, SLOT(verify()));
}

void Backup::process(QString op, QString sfin, QString sfout, uint32_t size)
{
	emit enabled(false);

	int in = ::open(sfin.latin1(), O_RDONLY);
	if (in < 0) {
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfin).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	int out = ::open(sfout.latin1(), O_WRONLY | O_CREAT | O_TRUNC);
	if (out < 0) {
		::close(in);
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfout).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);

	char buf[BUFFER_SIZE];
	uint32_t bsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : BUFFER_SIZE;
	uint32_t tsize = 0;
	for (;;) {
		int len = ::read(in, buf, bsz);
		if (len < 0) {
			QMessageBox::critical(this, op, tr("无法读取文件:\n%1\n%2 @ %3")
					.arg(sfin).arg(len).arg(tsize));
			break;
		} else if (len == 0) {
			break;
		}

		int wlen = ::write(out, buf, len);
		if (wlen != len) {
			QMessageBox::critical(this, op, tr("无法写入文件:\n%1\n%2 @ %3")
					.arg(sfout).arg(wlen).arg(tsize));
			break;
		}

		tsize += len;
		if (tsize > size)
			pbProgress->setTotalSteps(tsize);
		pbProgress->setProgress(tsize);
		qApp->processEvents();
	}

	::close(in);
	::close(out);
	pbProgress->setProgress(pbProgress->totalSteps());
	QMessageBox::information(this, op, tr("%1完成\n总计 %2 字节").arg(op).arg(tsize));

	emit enabled(true);
}

void Backup::verify()
{
	QString op = tr("验证");
	int sel = lbBlkdev->currentItem();
	if (sel < 0 || sel >= dev.count()) {
		QMessageBox::warning(this, op, tr("请选择一个分区"));
		return;
	}

	const dev_t *d = dev.at(sel);
	QString sfout(d->blkdev);
	QString sfin(QString("/mnt/mmc/backup/%1.img").arg(QFileInfo(d->blkdev).baseName()));
	uint32_t size = d->size;

	// Start
	emit enabled(false);

	if (d->mtd) {
		nandverify(op, sfout, sfin, size);
		emit enabled(true);
		return;
	}

	int in = ::open(sfin.latin1(), O_RDONLY);
	if (in < 0) {
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfin).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	int out = ::open(sfout.latin1(), O_WRONLY | O_CREAT | O_TRUNC);
	if (out < 0) {
		::close(in);
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfout).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);

	char buf[2][BUFFER_SIZE];
	uint32_t bsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : BUFFER_SIZE;
	uint32_t tsize = 0;
	for (;;) {
		int len = ::read(in, buf[0], bsz);
		if (len < 0) {
			QMessageBox::critical(this, op, tr("无法读取文件:\n%1\n%2 @ %3")
					.arg(sfin).arg(len).arg(tsize));
			break;
		} else if (len == 0) {
			break;
		}

		int wlen = ::read(out, buf[1], len);
		if (wlen < 0) {
			QMessageBox::critical(this, op, tr("无法读取文件:\n%1\n%2 @ %3")
					.arg(sfout).arg(wlen).arg(tsize));
			break;
		} else if (wlen != len) {
			QMessageBox::critical(this, op, tr("文件不完整:\n%1\n%2 @ %3")
					.arg(sfout).arg(wlen).arg(tsize));
			break;
		}

		tsize += len;

		if (memcmp(buf[0], buf[1], len) != 0) {
			int ofs;
			for (ofs = 0; ofs < len; ofs++)
				if (buf[0][ofs] != buf[1][ofs])
					break;
			QMessageBox::critical(this, op, tr("验证失败:\n%1\n%2\n%3/%4 @ %5")
					.arg(sfin).arg(sfout)
					.arg((int)(uint8_t)buf[0][ofs])
					.arg((int)(uint8_t)buf[1][ofs])
					.arg(tsize - len + ofs));
			break;
		}

		if (tsize > size)
			pbProgress->setTotalSteps(tsize);
		pbProgress->setProgress(tsize);
		qApp->processEvents();
	}

	::close(in);
	::close(out);
	pbProgress->setProgress(pbProgress->totalSteps());
	QMessageBox::information(this, op, tr("%1完成\n读取 %2 字节").arg(op).arg(tsize));

	emit enabled(true);
}

void Backup::nanddump(QString op, QString smtd, QString sfout, uint32_t size)
{
	int nanddump_open(std::string &serr, const char *devpath);
	int nanddump_dump(std::string &serr, std::vector<char> &dump_buf);
	std::string moredump_print_info();
	void nanddump_close();

	emit enabled(false);

	int out = ::open(sfout.latin1(), O_WRONLY | O_CREAT | O_TRUNC);
	if (out < 0) {
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfout).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	std::string serr;
	if (nanddump_open(serr, smtd.latin1()) != 0) {
		::close(out);
		QMessageBox::critical(this, op, tr("无法打开设备:\n%1\n%2").arg(smtd).arg(serr.c_str()));
		emit enabled(true);
		return;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);

	std::vector<char> buf;
	uint32_t rptsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : size / 1024;
	uint32_t tsize = 0, rptsize = 0;
	for (;;) {
		int ret = nanddump_dump(serr, buf);
		int len = buf.size();
		if (ret == 1) {
			break;
		} else if (ret != 0) {
			QMessageBox::critical(this, op, tr("无法读取设备:\n%1\n@ %2\n%3")
					.arg(smtd).arg(tsize).arg(serr.c_str()));
			break;
		} else if (len == 0) {
			continue;
		}

		int wlen = ::write(out, &buf.front(), len);
		if (wlen != len) {
			QMessageBox::critical(this, op, tr("无法写入文件:\n%1\n%2 @ %3")
					.arg(sfout).arg(wlen).arg(tsize));
			break;
		}

		tsize += len;
		if (tsize - rptsize >= rptsz) {
			rptsize = tsize;
			if (tsize > size)
				pbProgress->setTotalSteps(tsize);
			pbProgress->setProgress(tsize);
			qApp->processEvents();
		}
	}

	// Some extra MTD info
	QFile info(sfout + ".info");
	if (info.open(IO_WriteOnly)) {
		std::string sinfo = moredump_print_info();
		QTextStream ts(&info);
		ts << sinfo.c_str();
	}

	::close(out);
	nanddump_close();
	pbProgress->setProgress(pbProgress->totalSteps());
	QMessageBox::information(this, op, tr("%1完成\nmtd 读取 %2 字节").arg(op).arg(tsize));

	emit enabled(true);
}

void Backup::nandverify(QString op, QString smtd, QString sfile, uint32_t size)
{
	int nanddump_open(std::string &serr, const char *devpath);
	int nanddump_dump(std::string &serr, std::vector<char> &dump_buf);
	void nanddump_close();

	int out = ::open(sfile.latin1(), O_RDONLY);
	if (out < 0) {
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfile).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	std::string serr;
	if (nanddump_open(serr, smtd.latin1()) != 0) {
		::close(out);
		QMessageBox::critical(this, op, tr("无法打开设备:\n%1\n%2").arg(smtd).arg(serr.c_str()));
		return;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);

	std::vector<char> buf[2];
	uint32_t rptsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : size / 1024;
	uint32_t tsize = 0, rptsize = 0;
	for (;;) {
		int ret = nanddump_dump(serr, buf[0]);
		int len = buf[0].size();
		if (ret == 1) {
			break;
		} else if (ret != 0) {
			QMessageBox::critical(this, op, tr("无法读取设备:\n%1\n@ %2\n%3")
					.arg(smtd).arg(tsize).arg(serr.c_str()));
			break;
		} else if (len == 0) {
			continue;
		}

		buf[1].resize(len);
		int wlen = ::read(out, &buf[1].front(), len);
		if (wlen < 0) {
			QMessageBox::critical(this, op, tr("无法读取文件:\n%1\n%2 @ %3")
					.arg(sfile).arg(wlen).arg(tsize));
			break;
		} else if (wlen != len) {
			QMessageBox::critical(this, op, tr("文件不完整:\n%1\n%2 @ %3")
					.arg(sfile).arg(wlen).arg(tsize));
			break;
		}

		tsize += len;

		if (buf[0] != buf[1]) {
			int ofs;
			for (ofs = 0; ofs < len; ofs++)
				if (buf[0][ofs] != buf[1][ofs])
					break;
			QMessageBox::critical(this, op, tr("验证失败:\n%1\n%2\n%3/%4 @ %5")
					.arg(smtd).arg(sfile)
					.arg((int)(uint8_t)buf[0][ofs])
					.arg((int)(uint8_t)buf[1][ofs])
					.arg(tsize - len + ofs));
			break;
		}

		if (tsize > size)
			pbProgress->setTotalSteps(tsize);
		pbProgress->setProgress(tsize);
		qApp->processEvents();
	}

	nanddump_close();
	::close(out);
	pbProgress->setProgress(pbProgress->totalSteps());
	QMessageBox::information(this, op, tr("%1完成\nmtd 读取 %2 字节").arg(op).arg(tsize));
}

bool Backup::eraseall(QString op, QString sfin, QString smtd, uint32_t size)
{
	int eraseall_open(std::string &serr, const char *devpath);
	int eraseall_erase(std::string &serr, int &esize);
	void eraseall_close();

	std::string serr;
	if (eraseall_open(serr, smtd.latin1()) != 0) {
		QMessageBox::critical(this, op, tr("无法打开设备:\n%1\n%2").arg(smtd).arg(serr.c_str()));
		return false;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);
	pbRestore->setText(tr("擦除中..."));

	uint32_t rptsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : size / 1024;
	uint32_t tsize = 0, rptsize = 0;
	for (;;) {
		int len = 0;
		int ret = eraseall_erase(serr, len);
		if (ret == 1) {
			break;
		} else if (ret != 0) {
			QMessageBox::critical(this, op, tr("无法读取设备:\n%1\n@ %2\n%3")
					.arg(smtd).arg(tsize).arg(serr.c_str()));
			break;
		} else if (len == 0) {
			continue;
		}

		tsize += len;
		if (tsize - rptsize >= rptsz) {
			rptsize = tsize;
			if (tsize > size)
				pbProgress->setTotalSteps(tsize);
			pbProgress->setProgress(tsize);
			qApp->processEvents();
		}
	}

	eraseall_close();
	pbProgress->setProgress(pbProgress->totalSteps());

	return true;
}

void Backup::nandwrite(QString op, QString sfin, QString smtd, uint32_t size)
{
	int nandwrite_open(std::string &serr, const char *devpath);
	int nandwrite_pagelen();
	int nandwrite_write(std::string &serr, const std::vector<char> &write_buf);
	void nandwrite_close();

	emit enabled(false);

	int in = ::open(sfin.latin1(), O_RDONLY);
	if (in < 0) {
		QMessageBox::critical(this, op, tr("无法打开文件:\n%1\n%2")
				.arg(sfin).arg(strerror(errno)));
		emit enabled(true);
		return;
	}

	if (!eraseall(op, sfin, smtd, size)) {
		::close(in);
		emit enabled(true);
		return;
	}

	std::string serr;
	if (nandwrite_open(serr, smtd.latin1()) != 0) {
		::close(in);
		QMessageBox::critical(this, op, tr("无法打开设备:\n%1\n%2").arg(smtd).arg(serr.c_str()));
		emit enabled(true);
		return;
	}

	pbProgress->setTotalSteps(size);
	pbProgress->setProgress(0);
	pbRestore->setText(tr("恢复"));

	uint32_t page = nandwrite_pagelen();
	std::vector<char> buf(page);

	uint32_t rptsz = size / 1024 < BUFFER_SIZE ? SMALL_BUFFER_SIZE : size / 1024;
	uint32_t tsize = 0, rptsize = 0;

	for (;;) {
		buf.resize(page);
		int len = ::read(in, &buf.front(), page);
		if (len < 0) {
			QMessageBox::critical(this, op, tr("无法读取文件:\n%1\n%2 @ %3")
					.arg(sfin).arg(len).arg(tsize));
			break;
		} else if (len == 0) {
			break;
		}
		buf.resize(len);

		int ret = nandwrite_write(serr, buf);
		if (ret == 1) {
			QMessageBox::critical(this, op, tr("设备空间不足:\n%1\n%2 @ %3\n%4")
					.arg(smtd).arg(len).arg(tsize).arg(serr.c_str()));
			break;
		} else if (ret != 0) {
			QMessageBox::critical(this, op, tr("无法读取设备:\n%1\n%2 @ %3\n%4")
					.arg(smtd).arg(len).arg(tsize).arg(serr.c_str()));
			break;
		}

		tsize += len;
		if (tsize - rptsize >= rptsz) {
			rptsize = tsize;
			if (tsize > size)
				pbProgress->setTotalSteps(tsize);
			pbProgress->setProgress(tsize);
			qApp->processEvents();
		}
	}

	::close(in);
	nandwrite_close();
	pbProgress->setProgress(pbProgress->totalSteps());
	QMessageBox::information(this, op, tr("%1完成\nmtd 写入 %2 字节").arg(op).arg(tsize));

	emit enabled(true);
}

void Backup::backup()
{
	QString op = tr("备份");
	int sel = lbBlkdev->currentItem();
	if (sel < 0 || sel >= dev.count()) {
		QMessageBox::warning(this, op, tr("请选择一个分区"));
		return;
	}

	const dev_t *d = dev.at(sel);
	QString sfin(d->blkdev);
	QString sfout(QString("/mnt/mmc/backup/%1.img").arg(QFileInfo(d->blkdev).baseName()));
	QDir().mkdir("/mnt/mmc/backup");
	if (d->mtd)
		nanddump(op, sfin, sfout, d->size);
	else
		process(op, sfin, sfout, d->size);
}

void Backup::restore()
{
	QString op = tr("恢复");
	int sel = lbBlkdev->currentItem();
	if (sel < 0 || sel >= dev.count()) {
		QMessageBox::warning(this, op, tr("请选择一个分区"));
		return;
	}

	const dev_t *d = dev.at(sel);
	QString sfout(d->blkdev);
	QString sfin(QString("/mnt/mmc/backup/%1.img").arg(QFileInfo(d->blkdev).baseName()));
	QDir().mkdir("/mnt/mmc/backup");
	if (d->mtd)
		nandwrite(op, sfin, sfout, d->size);
	else
		process(op, sfin, sfout, d->size);
}
