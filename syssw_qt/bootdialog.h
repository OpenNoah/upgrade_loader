#pragma once

#include <qt.h>

class BootDialog: public QDialog
{
	Q_OBJECT
public:
	BootDialog(QString cmd, unsigned long offset, bool ro, QWidget *parent);
	static bool BootCheck(QString cmd, unsigned long offset = 0);
	static void init();

private slots:
	void confirm();
	void cancel();

private:
	static QStringList inits;

	QListBox *info;
	QPushButton *pbCancel, *pbClose;
	QTimer *timer;
};
