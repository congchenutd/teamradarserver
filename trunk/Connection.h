#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>

// Parses the message header & body from Connection
// Headers accepted: GREETING, REGISTER_PHOTO, REQUEST_USERLIST, REQUEST_PHOTO, EVENT, 
//					 REGISTER_COLOR, REQUEST_COLOR
// Clients do not send their user names, as they have signed up with GREETING.
// Format of packet: header#size#body
// Format of body:
//		GREETING: [OK, CONNECTED]/[WRONG_USER]
//		REQUEST_USERLIST: [empty], server knows the user name
//		REQEUST_PHOTO: target user name
//		REGISTER_PHOTO: file format#binary photo data
//		EVENT: event type#parameters
//			Format of parameters: parameter1#parameter2#...
//		REGISTER_COLOR: color
//		REQUEST_COLOR: target user name

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
		RequestUserList,
		RegisterPhoto,
		RegisterColor,
		RequestPhoto,
		RequestColor
	} DataType;

public:
	Receiver(Connection* c);
	DataType guessDataType(const QByteArray& header);
	void processData(Receiver::DataType dataType, const QByteArray& buffer);
	Sender* getSender() const;
	QString getUserName() const;

signals:
	void newEvent(const QString& from, const QByteArray& message);
	void requestUserList();
	void requestPhoto(const QString& targetUser);
	void requestColor(const QString& targetUser);
	void registerPhoto(const QString& user, const QByteArray& photo);
	void registerColor(const QString& user, const QByteArray& color);

private:
	void receiveGreeting(const QByteArray& buffer);

private:
	Connection* connection;
};

// A TCP socket connected to the server
// NOT a singleton: one connection for each client

// After the connection is set up, 
// the parsing and composition of messages are handed to Receiver and Sender
class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	Connection(QObject* parent = 0);
	QString         getUserName()   const { return userName; }
	Receiver*       getReceiver()   const { return receiver; }
	Sender*         getSender()     const { return sender;   }
	bool            isReadyForUse() const { return ready;    }
	void setUserName(const QString& name) { userName = name; }
	void setReadyForUse();

	static bool userExists(const QString& userName);

protected:
	void timerEvent(QTimerEvent* timerEvent);   // for transfer timeout

signals:
	void readyForUse();

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
	Receiver::DataType dataType;
	bool       ready;
	QByteArray buffer;
	int        numBytes;
	int        transferTimerID;   // for transfer timeout
	QString    userName;
	Receiver*  receiver;
	Sender*    sender;

	static QSet<QString> userNames;   // detects name duplication
};

// Format and send packets
// Headers: PHOTO_RESPONSE, USERLIST_RESPONSE, EVENT, COLOR_RESPONSE
// Format of packet: header#size#body
// Format of body:
//		PHOTO_RESPONSE: [filename#binary photo data]/[empty]
//		USERLIST_RESPONSE: username1#username2#...
//		EVENT: user#event#[parameters]
//			Format of parameters: parameter1#parameter2#...
//		COLOR_RESPONSE: targetUser#color

//	Formatting and sending are separated for flexibility

struct TeamRadarEvent;
class Sender : public QObject
{
public:
	Sender(Connection* c);
	QString getUserName() const;
	void send(const QByteArray& packet);  // send the formatted packet

	// format the packet
	static QByteArray makePacket(const QByteArray& header, const QByteArray& body = QByteArray("P"));
	static QByteArray makePacket(const QByteArray& header, const QList<QByteArray>& bodies);
	static QByteArray makeEventPacket(const TeamRadarEvent& event);
	static QByteArray makeUserListResponse(const QList<QByteArray>& userList);
	static QByteArray makePhotoResponse(const QString& fileName,   const QByteArray& photoData);
	static QByteArray makeColorResponse(const QString& targetUser, const QByteArray& color);

private:
	Connection* connection;
};

struct TeamRadarEvent
{
	TeamRadarEvent::TeamRadarEvent(const QString& name, const QString& event, const QString& para = QString())
		: userName(name), eventType(event), parameter(para)
	{}

	QString userName;
	QString eventType;
	QString parameter;
};

#endif // CONNECTION_H
