#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>

// Parses the message header & body from Connection
// Accepts 5 types of header: GREETING, REGISTER_PHOTO, REQUEST_USERLIST, REQUEST_PHOTO, EVENT
// Format of packet: header#size#body
// Format of body:
//		GREETING: [OK, CONNECTED]/[WRONG_USER]
//		REQUEST_USERLIST: [empty], server knows the user name
//		REQEUST_PHOTO: target user name
//		REGISTER_PHOTO: file format#binary photo data
//		EVENT: event type#parameters
//			Format of parameters: parameter1#parameter2#...

class Connection;
class Sender;

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
	Sender* getSender() const;
	QString getUserName() const;

signals:
	void newMessage(const QString& from, const QByteArray& message);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void requestPhoto (const QByteArray& targetUser);
	void requestUserList();

private:
	Connection* connection;
};

// A TCP socket connected to the server
// NOT a singleton: one connection from the server to each client
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
	QString         getUserName() const { return userName; }
	ConnectionState getState()    const { return state;    }
	void send(const QByteArray& header, const QByteArray& body = QByteArray("P"));
	void send(const QByteArray& header, const QList<QByteArray>& bodies);
	Receiver* getReceiver() const { return receiver; }
	Sender*   getSender()   const { return sender;   }

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
	Sender*         sender;

	static QSet<QString> userNames;
};

// Sends 3 types of header: PHOTO_RESPONSE, USERLIST_RESPONSE, EVENT
// Format of packet: header#size#body
// Format of body:
//		PHOTO_RESPONSE: [filename#binary photo data]/[empty]
//		USERLIST_RESPONSE: username1#username2#...
//		EVENT: event#parameters
//			Format of parameters: parameter1#parameter2#...

class Sender : public QObject
{
public:
	Sender(Connection* c);
	void sendEvent(const QString& userName, const QString& event, const QString& parameters);
	void sendPhotoResponse(const QString& fileName, const QByteArray& photoData);
	void sendUserListResponse(const QList<QByteArray>& userList);

private:
	Connection* connection;
};


#endif // CONNECTION_H
