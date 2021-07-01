#pragma once

#include <qt.h>

class Boot: public QWidget
{
	Q_OBJECT
public:
	Boot(QWidget *parent);

private slots:
	void boot(int id);
	void bootList();

private:
	static void codec(void *p, unsigned long size);
	static unsigned long offset(QString path);

	QListBox *lb;
	QStringList names;
	QStringList mounts;
	QValueList<unsigned long> offsets;
};
