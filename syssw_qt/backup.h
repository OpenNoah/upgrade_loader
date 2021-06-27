#pragma once

#include <stdint.h>
#include <qt.h>

class Backup: public QWidget
{
	Q_OBJECT

public:
	Backup(QWidget *parent = 0);

signals:
	void enabled(bool);

private slots:
	void backup();
	void restore();
	void verify();

private:
	void process(QString op, QString sfin, QString sfout, uint32_t size);
	void nanddump(QString op, QString smtd, QString sfout, uint32_t size);
	bool eraseall(QString op, QString sfin, QString smtd, uint32_t size);
	void nandwrite(QString op, QString sfin, QString smtd, uint32_t size);
	void nandverify(QString op, QString smtd, QString sfile, uint32_t size);

	struct dev_t {
		QString blkdev, name;
		uint32_t size;
		bool mtd;
	};
	QList<dev_t> dev;

	QListBox *lbBlkdev;
	QPushButton *pbRestore;
	QProgressBar *pbProgress;
};
