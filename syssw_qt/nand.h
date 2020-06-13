#pragma once

#include <qt.h>

class Nand: public QWidget
{
	Q_OBJECT
public:
	Nand(QWidget *parent);

private slots:
	void boot();
};
