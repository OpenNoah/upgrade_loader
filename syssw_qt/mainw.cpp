#include <stdlib.h>
#include "mainw.h"
#include "cgucontrol.h"
#include "regs.h"
#include "nand.h"

MainW::MainW(QWidget *parent) : QMainWindow(parent, 0, Qt::WStyle_Customize | Qt::WStyle_NoBorderEx)
{
	QTabWidget *tab = new QTabWidget(this);
	setCentralWidget(tab);

	if (system("mount -t ubifs ubi0:rootfs /mnt/root") == 0) {
		system("umount -l /mnt/root");
		tab->addTab(new Nand(this), tr("NAND 储存"));
	}

	Regs *regs = new Regs(this);
	if (regs->init())
		tab->addTab(new CGUControl(this), tr("时钟频率"));

	QPushButton *pb = new QPushButton(tr("Hello, world!\n点击退出..."), this);
	tab->addTab(pb, tr("选项"));

	connect(pb, SIGNAL(clicked()), qApp, SLOT(quit()));
}
