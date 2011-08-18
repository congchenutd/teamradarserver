#include "MainWnd.h"
#include "UsersModel.h"
#include <QtGui/QApplication>

int main(int argc, char *argv[])
{
	if(!UsersModel::openDB("TeamRadarServer.db"))
		return 1;
	UsersModel::createTables();

	QApplication app(argc, argv);
	app.setQuitOnLastWindowClosed(false);
	MainWnd wnd;
	wnd.show();
	return app.exec();
}
