#include "ConnectionPool.h"
#include "Connection.h"

void ConnectionPool::insert(Connection* connection) {
	pool.insert(connection->getUserName(), connection);
}

void ConnectionPool::remove(Connection* connection) {
	pool.remove(connection->getUserName());
}

void ConnectionPool::rename(const QString& oldName, const QString& newName) {
	if(renameSafe(oldName, newName))
	{
		Connection* connection = pool[oldName];
		pool.remove(oldName);
		pool.insert(newName, connection);
	}
}

bool ConnectionPool::renameSafe(const QString& oldName, const QString& newName) const {
	return pool.contains(oldName) && !pool.contains(newName);
}

bool ConnectionPool::contains(Connection* connection) const {
	return contains(connection->getUserName());
}

bool ConnectionPool::contains(const QString& userName) const {
	return pool.contains(userName);
}

Connection *ConnectionPool::getConnection(const QString& userName) const {
	return contains(userName) ? pool[userName] : 0;
}

