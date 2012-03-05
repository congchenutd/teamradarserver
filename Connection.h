#ifndef CONNECTION_H
#define CONNECTION_H

#include <QTcpSocket>
#include <QTimer>
#include <QTime>
#include <QStringList>
#include <QSet>
#include "MainWnd.h"

class Connection;
class Sender;

// Parses the message header & body from Connection
// Clients do not send their user names, as they have signed up with GREETING.
// Format of packet: header#size#body
class Receiver : public QObject
{
	Q_OBJECT

public:
	typedef enum {
		Undefined,      // Format of body see below:
		Greeting,       // GREETING: [OK, CONNECTED]/[WRONG_USER]
		ChangeName,     // new name
		Event,          // EVENT: event type#parameters
						// Format of parameters: parameter1#parameter2#...
		RegPhoto,       // REG_PHOTO: file format#binary photo data
		RegColor,       // REG_COLOR: color
		JoinProject,    // JOIN_PROJECT: projectname
		Chat,           // CHAT: recipients#content
						//	recipients = name1;name2;...
		ReqPhoto,       // REQ_PHOTO: target user name
		ReqColor,       // REQ_COLOR: target user name
		ReqEvents,	    // REQ_EVENTS: user list#event types#time span#phases#fuzziness
						//   user list: name1;name2;...
						//   event types: type1;type2;...
						//   time span: start time;end time
						//   phases: phase1;phase2;...
						//   fuzziness: an integer for percentage
		ReqTimeSpan,    // REQ_TIMESPAN: [empty]
		ReqProjects,    // REQ_PROJECTS: [empty]
		ReqTeamMembers, // REQ_ALLUSERS: [empty], server knows the user name
		ReqLocation,    // targetUser
		ReqOnline       // targetUser
	} DataType;

	typedef void(Receiver::*Parser)(const QByteArray& buffer);

public:
	Receiver(Connection* c);
	void processData(Receiver::DataType dataType, const QByteArray& buffer);
	DataType guessDataType(const QByteArray& header);
	Sender*  getSender() const;
	QString  getUserName() const;

	static void init();       // init header - parser associations

	// parsing result
signals:
	void newEvent(const QString& from, const QByteArray& message);
	void chatMessage(const QList<QByteArray>& recipients, const QByteArray& content);
	void joinProject(const QString& projectName);
	void reqTeamMembers();
	void regPhoto (const QString& user, const QByteArray& photo);
	void regColor (const QString& user, const QByteArray& color);
	void reqOnline(const QString& targetUser);
	void reqPhoto (const QString& targetUser);
	void reqColor (const QString& targetUser);
	void reqEvents(const QStringList& users, const QStringList& eventTypes,
					   const QDateTime& startTime, const QDateTime& endTime,
					   const QStringList& phases, int fuzziness);
	void reqTimeSpan();
	void reqProjects();
	void reqLocation(const QString& targetUser);

	// parsers
private:
	void parseGreeting      (const QByteArray& userName);
	void parseChangeName    (const QByteArray& newName);
	void parseEvent         (const QByteArray& buffer);
	void parseReqEvents     (const QByteArray& buffer);
	void parseChat          (const QByteArray& buffer);
	void parseRegPhoto      (const QByteArray& buffer);
	void parseRegColor      (const QByteArray& buffer);
	void parseReqOnline     (const QByteArray& buffer);
	void parseReqPhoto      (const QByteArray& buffer);
	void parseReqTeamMembers(const QByteArray& buffer);
	void parseReqColor      (const QByteArray& buffer);
	void parseReqTimeSpan   (const QByteArray& buffer);
	void parseReqProjects   (const QByteArray& buffer);
	void parseJoinProject   (const QByteArray& buffer);
	void parseReqLocation   (const QByteArray& buffer);

private:
	Connection* connection;
	static QMap<QString, DataType> dataTypes;  // header -> datatype
	static QMap<DataType, Parser>  parsers;    // datatype -> parser
};

// A TCP socket connected to the server
// NOT a singleton: one connection for each client
// After the connection is set up,
// the parsing and composition of messages are handed to Receiver and Sender, respectively
class Connection : public QTcpSocket
{
	Q_OBJECT

public:
	Connection(QObject* parent = 0);
	QString   getUserName()   const { return userName; }
	Receiver* getReceiver()   const { return receiver; }
	Sender*   getSender()     const { return sender;   }
	bool      isReadyForUse() const { return ready;    }
	void setUserName(const QString& name);
	void setReadyForUse();

	static bool userExists(const QString& userName);

protected:
	void timerEvent(QTimerEvent* timerEvent);   // transfer timeout

signals:
	void readyForUse();    // connected
	void changeName(const QString& oldName, const QString& newName);

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


struct TeamRadarEvent;

// Format and send packets
// Formatting (makeXXX) and sending (send) are separated for flexibility
class Sender : public QObject
{
public:
	Sender(Connection* c);
	QString getUserName() const;
	void send(const QByteArray& packet);  // send the formatted packet

	// format the packet
	static QByteArray makePacket(const QByteArray& header, const QByteArray& body = QByteArray());
	static QByteArray makePacket(const QByteArray& header, const QList<QByteArray>& bodies);
	static QByteArray makeEventPacket(const TeamRadarEvent& event);
	static QByteArray makeChatPacket(const QString& user, const QByteArray& content);
	static QByteArray makeTeamMembersReply(const QList<QByteArray>& userList);
	static QByteArray makeOnlineReply(const QString& targetUser, bool online);
	static QByteArray makePhotoReply (const QString& fileName,   const QByteArray& photoData);
	static QByteArray makeColorReply (const QString& targetUser, const QByteArray& color);
	static QByteArray makeEventsReply(const TeamRadarEvent& event);
	static QByteArray makeTimeSpanReply(const QByteArray& start, const QByteArray& end);
	static QByteArray makeProjectsReply(const QList<QByteArray>& projects);
	static QByteArray makeLocationReply(const QString& targetUser, const QString& location);

private:
	static QByteArray makeEventPacket(const QByteArray& header, const TeamRadarEvent& event);

private:
	Connection* connection;
};


#endif // CONNECTION_H
