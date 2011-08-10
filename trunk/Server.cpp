#include "Server.h"
#include "Connection.h"

Server::Server(QObject* parent) : QTcpServer(parent)
{}

void Server::incomingConnection(int socketDescriptor)
{
	Connection *connection = new Connection(this);
	connection->setSocketDescriptor(socketDescriptor);
	emit newConnection(connection);
}