#include <unistd.h>
#include <fstream>
#include "bootdialog.h"

#define INFO(str)	do { \
	info->insertItem(str); \
	info->setBottomItem(info->numRows() - 1); \
	qApp->processEvents(0); \
} while (0)

BootDialog::BootDialog(QString cmd, QWidget *parent): QDialog(parent, 0, true,
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
	if (system(cmd.local8Bit()) != 0) {
		INFO(tr("挂载失败"));
		pbClose->show();
		return;
	}

	static const char *init[3] = {
		"/init",
		"/sbin/init",
		"/linuxrc",
	};

	const char *pinit = 0;
	for (unsigned int i = 0; i < 3; i++) {
		const char *p = init[i];
		INFO(tr("检查启动程序: %1").arg(p));
		if (system(QString("test -x /mnt/root%1").arg(p).local8Bit()) == 0) {
			INFO(tr("找到启动程序: %1").arg(p));
			pinit = p;
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
