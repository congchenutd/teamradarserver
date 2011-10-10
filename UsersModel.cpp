#include "UsersModel.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSet>

UsersModel::UsersModel(QObject* parent)
	: QSqlTableModel(parent) {}

bool UsersModel::openDB(const QString& name)
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

void UsersModel::createTables()
{
	QSqlQuery query;
	query.exec("create table Logs( \
				ID int primary key, \
				Time time, \
				Client varchar, \
				Event varchar, \
				Parameters varchar \
				)");
	query.exec("create table Users( \
				Username varchar primary key, \
				Online bool, \
				Color varchar, \
				Image varchar, \
				Project varchar \
				)");
}

void UsersModel::makeAllOffline()
{
	QSqlQuery query;
	query.exec(tr("update Users set Online = \"false\""));
}

void UsersModel::makeOffline(const QString& name)
{
	QSqlQuery query;
	query.exec(tr("update Users set Online = \"false\" where Username = \"%1\"").arg(name));
}

void UsersModel::makeOnline(const QString& name)
{
	QSqlQuery query;
	query.exec(tr("update Users set Online = \"true\" where Username = \"%1\"").arg(name));
}

void UsersModel::addUser(const QString& name)
{
	QSqlQuery query;
	query.exec(tr("insert into Users values (\"%1\", \"true\", \"#000000\", \"\", \"\")").arg(name));
}

void UsersModel::setImage(const QString& name, const QString& imagePath)
{
	QSqlQuery query;
	query.exec(tr("update Users set Image = \"%1\" where Username = \"%2\"").arg(imagePath).arg(name));
}

bool UsersModel::select()
{
	bool result = QSqlTableModel::select();
	emit selected();
	return result;
}

void UsersModel::setColor(const QString& name, const QString& color)
{
	QSqlQuery query;
	query.exec(tr("update Users set Color = \"%1\" where Username = \"%2\"").arg(color).arg(name));
}

QString UsersModel::getColor(const QString& name)
{
	QSqlQuery query;
	query.exec(tr("select Color from Users where Username = \"%1\"").arg(name));
	return query.next() ? query.value(0).toString() : "#000000";
}

void UsersModel::setProject(const QString& name, const QString& project)
{
	QSqlQuery query;
	query.exec(tr("update Users set Project = \"%1\" where Username = \"%2\"").arg(project).arg(name));
}

QList<QByteArray> UsersModel::getProjects()
{
	QSet<QByteArray> result;
	QSqlQuery query;
	query.exec(tr("select Project from Users"));
	while(query.next())
	{
		QString project = query.value(0).toString();
		if(!project.isEmpty())
			result << project.toUtf8();
	}
	return result.toList();
}

QString UsersModel::getProject(const QString& name)
{
	QSqlQuery query;
	query.exec(tr("select Project from Users where Username = \"%1\"").arg(name));
	return query.next() ? query.value(0).toString() : QString();
}

QList<QByteArray> UsersModel::getProjectMembers(const QString& project)
{
	QList<QByteArray> result;
	QSqlQuery query;
	query.exec(tr("select Username from Users where Username <> \"\" \
				  and Project = \"%1\"").arg(project));
	while(query.next())
		result << query.value(0).toString().toUtf8();
	return result;
}