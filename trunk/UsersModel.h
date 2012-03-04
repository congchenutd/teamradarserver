#ifndef USERSMODEL_H
#define USERSMODEL_H

#include <QSqlTableModel>

class UsersModel : public QSqlTableModel
{
	Q_OBJECT

public:
	UsersModel(QObject* parent = 0);

	static bool openDB(const QString& name);
	static void createTables();
	static void makeAllOffline();
	static void makeOffline(const QString& name);
	static void makeOnline (const QString& name);
	static bool isOnline(const QString& name);
	static void addUser(const QString& name);
	static void setImage(const QString& name, const QString& imagePath);
	static void setColor(const QString& name, const QString& color);
	static QString getColor(const QString& name);
	static void setProject(const QString& name, const QString& project);
	static QList<QByteArray> getProjects();           // get all active projects
	static QString getProject(const QString& name);   // get the project the developer is working on
	static QList<QByteArray> getProjectMembers(const QString& project);  // all developers working on project

	bool select();

signals:
	void selected();

public:
	enum {USERNAME, ONLINE, COLOR, IMAGE};
};

#endif // USERSMODEL_H
