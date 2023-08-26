#pragma once

#include <qt.h>

class Options: public QWidget
{
	Q_OBJECT

public:
	Options(QWidget *parent = 0);

private slots:
	void lcd_fix();
	void save();
	void poweroff();
	void reboot();
};
