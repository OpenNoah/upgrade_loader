#pragma once

#include <qt.h>

class BootDialog: public QDialog
{
	Q_OBJECT
public:
	BootDialog(QString cmd, QWidget *parent);

private slots:
	void confirm();
	void cancel();

private:
	QListBox *info;
	QPushButton *pbCancel, *pbClose;
	QTimer *timer;
};
