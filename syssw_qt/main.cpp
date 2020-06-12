#include <qt.h>
#include "mainw.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	// Set default configurations
	a.setFont(QFont("noah", 16));
	a.setDefaultCodec(QTextCodec::codecForName("UTF8"));

	MainW mw;
	a.setMainWidget(&mw);
	mw.showFullScreen();
	return a.exec();
};
