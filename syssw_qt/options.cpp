#include "options.h"

Options::Options(QWidget *parent): QWidget(parent)
{
	QVBoxLayout *vlay = new QVBoxLayout(this, 0, 4);

	QPushButton *pbSave = new QPushButton(tr("保存设置 (没做)"), this);
	vlay->addWidget(pbSave);

	vlay->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding));

	QPushButton *pbQuit = new QPushButton(tr("Hello, world!\t点击退出..."), this);
	vlay->addWidget(pbQuit);

	connect(pbQuit, SIGNAL(clicked()), qApp, SLOT(quit()));
}
