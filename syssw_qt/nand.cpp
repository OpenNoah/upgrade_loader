#include "nand.h"
#include "bootdialog.h"

Nand::Nand(QWidget *parent): QWidget(parent)
{
	QGridLayout *glay = new QGridLayout(this, 1, 1, 0, 4);
	QPushButton *boot = new QPushButton(tr("启动"), this);
	connect(boot, SIGNAL(clicked()), this, SLOT(boot()));
}

void Nand::boot()
{
	BootDialog *b = new BootDialog("mount -t ubifs ubi0:rootfs /mnt/root", this);
}
