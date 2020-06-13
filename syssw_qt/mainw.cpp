#include <stdlib.h>
#include "mainw.h"
#include "cgucontrol.h"
#include "regs.h"
#include "boot.h"
#include "bootdialog.h"

MainW::MainW(QWidget *parent) : QMainWindow(parent, 0, Qt::WStyle_Customize | Qt::WStyle_NoBorderEx)
{
	QTabWidget *tab = new QTabWidget(this);
	setCentralWidget(tab);

	BootDialog::init();
	tab->addTab(new Boot(this), tr("启动项"));

	Regs *regs = new Regs(this);
	if (regs->init())
		tab->addTab(new CGUControl(this), tr("时钟频率"));

	QPushButton *pb = new QPushButton(tr("Hello, world!\n点击退出..."), this);
	tab->addTab(pb, tr("选项"));

	connect(pb, SIGNAL(clicked()), qApp, SLOT(quit()));
}
