#ifndef CONNECTION_H
#define CONNECTION_H

// One Connection for each client

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>

// Parses the message header & body from Connection
// Accepts 4 types of header: GREETING, PHOTO_RESPONSE, USERLIST_RESPONSE, EVENT
// Format of packet: header#size#body
// Format of body:
//		GREETING: [OK, CONNECTED]/[WRONG_USER]
//		PHOTO_RESPONSE: [filename#binary photo data]/[empty]
//		USERLIST_RESPONSE: username1#username2#...
//		EVENT: event#parameters
//			Format of parameters: parameter1#parameter2#...

class Connection;
class Receiver : public QObject
{
	Q_OBJECT

public:
	typedef enum {
		Undefined,
		Greeting,
		Event,
		RegisterPhoto,
		RequestPhoto,
		RequestUserList
	} DataType;

public:
	Receiver(Connection* c);
	DataType guessDataType(const QByteArray& header);
	void processData(Receiver::DataType dataType, const QByteArray& buffer);

signals:
	void newMessage(const QString& from, const QByteArray& message);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void requestPhoto (const QByteArray& targetUser);
	void requestUserList();

private:
	Connection* connection;
};

// A TCP socket connected to the server
// NOT a singleton: there is one connection from the server to each client
class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	typedef enum {
		WaitingForGreeting,
		ReadingGreeting,
		ReadyForUse
	} ConnectionState;

public:
	Connection(QObject* parent = 0);
	QString getUserName() const { return userName; }
	void send(const QByteArray& header, const QByteArray& body = QByteArray("P"));
	void send(const QByteArray& header, const QList<QByteArray>& bodies);
	Receiver* getReceiver() const { return receiver; }

protected:
	void timerEvent(QTimerEvent* timerEvent);

signals:
	void readyForUse();
	void newMessage(const QString& from, const QByteArray& message);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void requestPhoto (const QByteArray& targetUser);
	void requestUserList();

private slots:
	void onReadyRead();    // data coming
	void onDisconnected();

private:
	bool readHeader();
	int  readDataIntoBuffer(int maxSize = MaxBufferSize);
	int  getDataLength();
	bool hasEnoughData();
	void processData();

public:
	static const int  MaxBufferSize   = 1024 * 1024;   // 1KB
	static const int  TransferTimeout = 30 * 1000;

private:
	ConnectionState state;
	Receiver::DataType dataType;
	QByteArray      buffer;
	int             numBytes;
	int             transferTimerID;
	QString         userName;
	Receiver*       receiver;

	static QSet<QString> userNames;
};

#endif // CONNECTION_H
