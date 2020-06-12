#include "mainw.h"
#include "cgucontrol.h"
#include "regs.h"

MainW::MainW(QWidget *parent) : QMainWindow(parent, 0, Qt::WStyle_Customize | Qt::WStyle_NoBorderEx)
{
	QTabWidget *tab = new QTabWidget(this);
	setCentralWidget(tab);

	Regs *regs = new Regs(this);
	if (regs->init()) {
		tab->addTab(new CGUControl(this), tr("时钟频率"));
	}

	QPushButton *pb = new QPushButton(tr("Hello, world!\n中文测试\n点击退出..."), this);
	tab->addTab(pb, "Exit");

	connect(pb, SIGNAL(clicked()), qApp, SLOT(quit()));
}
