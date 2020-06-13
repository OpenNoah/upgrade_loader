#include <unistd.h>
#include <fstream>
#include "bootdialog.h"

#define LOOP_DEV	"/dev/loop1"

#define INFO(str)	do { \
	info->insertItem(str); \
	info->setBottomItem(info->numRows() - 1); \
	qApp->processEvents(0); \
} while (0)

QStringList BootDialog::inits;

BootDialog::BootDialog(QString cmd, unsigned long offset, bool ro, QWidget *parent): QDialog(parent, 0, true,
			Qt::WStyle_Customize | Qt::WStyle_NoBorderEx | Qt::WStyle_StaysOnTop | Qt::WStyle_Dialog)
{
	QVBoxLayout *vlay = new QVBoxLayout(this);

	info = new QListBox(this);
	vlay->addWidget(info);
	info->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	vlay->addWidget(pbClose = new QPushButton(tr("关闭"), this));
	pbClose->hide();
	connect(pbClose, SIGNAL(clicked()), this, SLOT(reject()));

	vlay->addWidget(pbCancel = new QPushButton(tr("取消"), this));
	pbCancel->hide();
	connect(pbCancel, SIGNAL(clicked()), this, SLOT(cancel()));

	showFullScreen();
	qApp->processEvents(0);

	INFO(tr("挂载系统: %1").arg(cmd));
	if (offset != 0) {
		// loop0 in use by initfs, use loop1
		if (system(QString("losetup -r -o %1 " LOOP_DEV " %2").arg(offset).arg(cmd).local8Bit()) != 0) {
			INFO(tr("加载环回设备失败"));
			pbClose->show();
			return;
		}
		if (system("mount -t ext2 -o ro " LOOP_DEV " /mnt/root") != 0) {
			system("losetup -d " LOOP_DEV);
			INFO(tr("环回设备挂载失败"));
			pbClose->show();
			return;
		}
	} else {
		if (system(cmd.local8Bit()) != 0) {
			INFO(tr("挂载失败"));
			pbClose->show();
			return;
		}
	}

	QString pinit;
        for (QStringList::Iterator it = inits.begin(); it != inits.end(); ++it) {
		INFO(tr("检查启动程序: %1").arg(*it));
		if (system(QString("chroot /mnt/root test -x %1").arg(*it).local8Bit()) == 0) {
			INFO(tr("找到启动程序: %1").arg(*it));
			pinit = *it;
			break;
		}
	}

	if (pinit != 0) {
		std::ofstream sinit("/tmp/init", std::ofstream::out);
		sinit << pinit << std::endl;
		sinit.close();
		if (sinit) {
			INFO(tr("准备完成, 一秒后切换系统"));
			setResult(Accepted);
			pbCancel->show();
			QTimer::singleShot(1000, this, SLOT(confirm()));
			return;
		}
		unlink("/tmp/init");
		INFO(tr("启动记录 /tmp/init 失败"));
	}

	if (system("umount -l /mnt/root") == 0)
		INFO(tr("根目录已卸载"));
	pbClose->show();
}

void BootDialog::init()
{
	inits << "/init";
	inits << "/init.sh";
	inits << "/linuxrc";
	inits << "/sbin/init";
}

bool BootDialog::BootCheck(QString cmd, unsigned long off)
{
	if (off != 0) {
		// loop0 in use by initfs, use loop1
		if (system(QString("losetup -r -o %1 " LOOP_DEV " %2").arg(off).arg(cmd).local8Bit()) != 0)
			return false;
		if (system("mount -t ext2 -o ro " LOOP_DEV " /mnt/root") != 0) {
			system("losetup -d " LOOP_DEV);
			return false;
		}
	} else {
		if (system((cmd + " -o ro").local8Bit()) != 0)
			return false;
	}

	bool ok = false;
        for (QStringList::Iterator it = inits.begin(); it != inits.end(); ++it) {
		if (system(QString("chroot /mnt/root test -x %1").arg(*it).local8Bit()) == 0) {
			ok = true;
			break;
		}
	}

	// Apparently this automatically detaches loop dev
	system("umount -l /mnt/root");
	return ok;
}

void BootDialog::confirm()
{
	if (result() == Accepted)
		qApp->quit();
}

void BootDialog::cancel()
{
	setResult(Rejected);
	unlink("/tmp/init");
	pbCancel->hide();
	pbClose->show();
	if (system("umount -l /mnt/root") == 0)
		INFO(tr("根目录已卸载"));
	INFO(tr("系统切换已取消"));
}
