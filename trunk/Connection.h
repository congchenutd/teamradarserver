#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>

static const int MaxBufferSize = 1024;   // 1KB

class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	typedef enum {
		WaitingForGreeting,
		ReadingGreeting,
		ReadyForUse
	} ConnectionState;

	typedef enum {
		Undefined,
		Greeting,
		Ping,
		Pong,
		Event,
	} DataType;

public:
	Connection(QObject *parent);
	~Connection();
	QString getUserName() const { return userName; }

signals:
	void readyForUse();
	void newMessage(const QString &from, const QString &message);

private slots:
	void onReadyRead();
	void sendPing();
	void sendGreeting();

private:
	bool readHeader();
	int  readDataIntoBuffer(int maxSize = MaxBufferSize);
	int  getDataLength();
	bool hasEnoughData();
	void processData();
	DataType guessDataType(const QByteArray& header);

public:
	static const int  TransferTimeout = 30 * 1000;
	static const int  PongTimeout     = 60 * 1000;
	static const int  PingInterval    = 10 * 1000;

private:
	QTimer          pingTimer;
	QTime           pongTime;
	ConnectionState state;
	DataType        dataType;
	QByteArray      buffer;
	int             numBytes;
	int             timerId;
	bool            isGreetingSent;
	QString         userName;
};

#endif // CONNECTION_H
