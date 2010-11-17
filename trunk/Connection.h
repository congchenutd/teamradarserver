#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>

class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	Connection(QObject *parent);
	~Connection();

signals:
	void readyForUse();
	void newMessage(const QString &from, const QString &message);

private:
	
};

#endif // CONNECTION_H
