#pragma once

#include <qt.h>

class Boot: public QWidget
{
	Q_OBJECT
public:
	Boot(QWidget *parent);

private slots:
	void boot(int id);

private:
	static void codec(void *p, unsigned long size);
	static unsigned long offset(QString path);

	QStringList names;
	QStringList mounts;
	QValueList<unsigned long> offsets;
};
