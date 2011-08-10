#ifndef CONNECTION_H
#define CONNECTION_H

// One Connection for each client

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
	QString getUserName() const { return userName; }
	void send(const QByteArray& header, const QByteArray& body = QByteArray("P"));
	void send(const QByteArray& header, const QList<QByteArray>& bodies);

protected:
	void timerEvent(QTimerEvent* timerEvent);

signals:
	void readyForUse();
	void newMessage(const QString& from, const QByteArray& message);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void requestPhoto (const QByteArray& targetUser);
	void requestUserList();

private slots:
	void sendPing();
	void onReadyRead();    // data coming
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
	int             transferTimerID;
	QString         userName;

	static QSet<QString> userNames;
};

#endif // CONNECTION_H
