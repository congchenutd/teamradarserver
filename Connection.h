#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>
#include "MainWnd.h"

// Parses the message header & body from Connection
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
//		REQUEST_EVENTS: user list#time span#event types
//			user list: name1;name2;...
//			event types: type1;type2;...
//			time span: start time;end time
//		CHAT: recipients#content
//			recipients = name1;name2;...
//		REQUEST_TIMESPAN: [empty]

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
		RequestColor,
		RequestEvents,
		Chat,
		RequestTimeSpan
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
	void requestEvents(const QStringList& users, const QStringList& eventTypes,
					   const QDateTime& startTime, const QDateTime& endTime);
	void chatMessage(const QStringList& recipients, const QByteArray& content);
	void requestTimeSpan();

private:
	void parseGreeting(const QByteArray& buffer);
	void parseEvents  (const QByteArray& buffer);
	void parseChat    (const QByteArray& buffer);

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
	static const char Delimiter1 = '#';
	static const char Delimiter2 = ';';
	static const char Delimiter3 = ',';

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
// Format of packet: header#size#body
// Format of body:
//		PHOTO_RESPONSE: [filename#binary photo data]/[empty]
//		USERLIST_RESPONSE: username1#username2#...
//		EVENT: user#event#[parameters]#time
//			Format of parameters: parameter1#parameter2#...
//		COLOR_RESPONSE: targetUser#color
//		EVENT_RESPONSE: same as event
//		CHAT: peerName#content
//		TIMESPAN_RESPONSE: start#end

//	Formatting (makeXXX) and sending (send) are separated for flexibility

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
	static QByteArray makeEventsResponse(const TeamRadarEvent& event);
	static QByteArray makeChatPacket(const QString& user, const QByteArray& content);
	static QByteArray makeTimeSpanResponse(const QByteArray& start, const QByteArray& end);

private:
	Connection* connection;
};

struct TeamRadarEvent
{
	TeamRadarEvent::TeamRadarEvent(const QString& name, 
								   const QString& event, 
								   const QString& para = QString(), 
								   const QString& t = QString())
		: userName(name), eventType(event), parameters(para) {
			time = t.isEmpty() ? QDateTime::currentDateTime() 
							   : QDateTime::fromString(t, MainWnd::dateTimeFormat);
	}
	QString   userName;
	QString   eventType;
	QString   parameters;
	QDateTime time;
};

#endif // CONNECTION_H
