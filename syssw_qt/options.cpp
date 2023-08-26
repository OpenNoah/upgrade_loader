#include <stdlib.h>
#include "options.h"
#include "lcd_fix.h"

Options::Options(QWidget *parent): QWidget(parent)
{
	QVBoxLayout *vlay = new QVBoxLayout(this, 0, 4);

	QPushButton *pbSave = new QPushButton(tr("保存设置 (触屏校正)"), this);
	vlay->addWidget(pbSave);
	QPushButton *pbLcdFix = new QPushButton(tr("修正 LCD 屏幕参数"), this);
	vlay->addWidget(pbLcdFix);

	vlay->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));

	QPushButton *pbShutdown = new QPushButton(tr("关机"), this);
	vlay->addWidget(pbShutdown);
	QPushButton *pbReboot = new QPushButton(tr("重启"), this);
	vlay->addWidget(pbReboot);

	QPushButton *pbQuit = new QPushButton(tr("退出到 tty"), this);
	vlay->addWidget(pbQuit);

	connect(pbSave, SIGNAL(clicked()), this, SLOT(save()));
	connect(pbLcdFix, SIGNAL(clicked()), this, SLOT(lcd_fix()));

	connect(pbShutdown, SIGNAL(clicked()), this, SLOT(poweroff()));
	connect(pbReboot, SIGNAL(clicked()), this, SLOT(reboot()));

	connect(pbQuit, SIGNAL(clicked()), qApp, SLOT(quit()));
}

void Options::save()
{
	system("mkdir -p /mnt/mmc/loaderfs_cfg; cp /tmp/pointercal /mnt/mmc/loaderfs_cfg/pointercal");
}

void Options::lcd_fix()
{
}

void Options::poweroff()
{
	system("sync; busybox poweroff -f");
}

void Options::reboot()
{
	system("sync; busybox reboot -f");
}
