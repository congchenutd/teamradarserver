#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>

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
		RegisterPhoto,
		RequestPhoto,
		RequestUserList
	} DataType;

public:
	Connection(QObject *parent);
	~Connection() {}
	QString getUserName() const { return userName; }
	void sendPhoto(const QString& targetUser);
	void send(const QString& header, const QString& body = QString("P"));
	void send(const QString& header, const QStringList& bodies);

signals:
	void readyForUse();
	void newMessage(const QString &from, const QString &message);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void requestPhoto (const QString& targetUser);
	void requestUserList();

private slots:
	void onReadyRead();
	void sendPing();
	void onDisconnected();

private:
	bool readHeader();
	int  readDataIntoBuffer(int maxSize = MaxBufferSize);
	int  getDataLength();
	bool hasEnoughData();
	void processData();
	DataType guessDataType(const QByteArray& header);

public:
	static const int  MaxBufferSize   = 1024 * 1024;   // 1KB
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
	QString         userName;

	static QSet<QString> userNames;
};

#endif // CONNECTION_H
