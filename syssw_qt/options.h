#pragma once

#include <qt.h>

class Options: public QWidget
{
	Q_OBJECT

public:
	Options(QWidget *parent = 0);

private slots:
	void save();
	void poweroff();
	void reboot();
};
