#include <qt.h>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	// Set default configurations
	a.setFont(QFont("noah", 16));
	a.setDefaultCodec(QTextCodec::codecForName("UTF8"));

	QPushButton pb(QObject::tr("Hello, world!\n中文测试\n点击退出..."), 0);
	a.setMainWidget(&pb);

	QObject::connect(&pb, SIGNAL(clicked()), &a, SLOT(quit()));

	pb.showFullScreen();
	return a.exec();
};
