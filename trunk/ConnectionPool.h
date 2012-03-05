#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <QMap>
#include <QString>

class Connection;

class ConnectionPool
{
	typedef QMap<QString, Connection*> Pool;

public:
	void insert(Connection* connection);
	void remove(Connection* connection);
	void rename    (const QString& oldName, const QString& newName);
	bool renameSafe(const QString& oldName, const QString& newName) const;
	bool contains(Connection* connection) const;
	bool contains(const QString& userName) const;
	Connection* getConnection(const QString& userName) const;

private:
	Pool pool;
};

#endif // CONNECTIONPOOL_H
