#include "MainWnd.h"
#include "Connection.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	if(!UsersModel::openDB("TeamRadarServer.db"))
		return 1;
	UsersModel::createTables();
	Receiver::init();

	QApplication app(argc, argv);
	app.setQuitOnLastWindowClosed(false);
	MainWnd wnd;
	wnd.show();

	return app.exec();		
}
