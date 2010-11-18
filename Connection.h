#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>

static const int MaxBufferSize = 1024 * 1024;

class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	Connection(QObject *parent);
	~Connection();

signals:
	void readyForUse();
	void newMessage(const QString &from, const QString &message);

private slots:
	void processReadyRead();
	void sendPing();
	void sendGreetingMessage();

private:
	bool readHeader();
	int  readDataIntoBuffer(int maxSize = MaxBufferSize);
	int  getDataLength();
	bool readProtocolHeader();
	bool hasEnoughData();
	void processData();

public:
	typedef enum {
		WaitingForGreeting,
		ReadingGreeting,
		ReadyForUse
	} ConnectionState;
	
	typedef enum {
		PlainText,
		Ping,
		Pong,
		Greeting,
		Undefined
	} DataType;

private:
	QTimer pingTimer;
	QTime  pongTime;
	ConnectionState state;
	DataType        dataType;
	QByteArray buffer;
	int numBytes;
	int timerId;
	bool isGreetingSent;
};

#endif // CONNECTION_H
