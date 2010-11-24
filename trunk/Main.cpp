#include "MainWnd.h"
#include <QtGui/QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMessageBox>

bool openDB(const QString& name)
{
	QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE");
	database.setDatabaseName(name);
	if(!database.open())
	{
		QMessageBox::critical(0, "Error", "Can not open database");
		return false;
	}
	return true;
}

void createTables()
{
	QSqlQuery query;
	query.exec("create table Logs( \
				ID int primary key, \
				Time time, \
				Client varchar, \
				Event varchar, \
				Parameters varchar \
				)");
	query.exec("create table Connections(Username varchar primary key)");
}

int main(int argc, char *argv[])
{
	if(!openDB("TeamRadarServer.db"))
		return 1;
	createTables();

	QApplication app(argc, argv);
	MainWnd wnd;
	wnd.show();
	return app.exec();
}
