#include "Connection.h"
#include "MainWnd.h"
#include "TeamRadarEvent.h"
#include <QHostAddress>
#include <QTimerEvent>
#include <QColor>

Connection::Connection(QObject *parent) : QTcpSocket(parent)
{
	ready = false;
	dataType = Receiver::Undefined;
	numBytes = -1;
	transferTimerID = 0;
	userName = tr("Unknown");
	receiver = new Receiver(this);
	sender   = new Sender  (this);

	connect(this, SIGNAL(readyRead()),    this, SLOT(onReadyRead()));
	connect(this, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
}

// transfer time out
void Connection::timerEvent(QTimerEvent* timerEvent)
{
	if(timerEvent->timerId() == transferTimerID)  // the event is for this connection
	{
		abort();
		killTimer(transferTimerID);
		transferTimerID = 0;
	}
}

// new data incoming
void Connection::onReadyRead()
{
	do {
		if(dataType == Receiver::Undefined && !readHeader())  // read header
			return;
		if(!hasEnoughData())                                  // read length, and wait for data
			return;
		processData();
	} while(bytesAvailable() > 0);
}

// read data type
bool Connection::readHeader()
{
	if(transferTimerID)   // reset timer
	{
		killTimer(transferTimerID);
		transferTimerID = 0;
	}

	if(readDataIntoBuffer() <= 0)         // read header data
	{
		transferTimerID = startTimer(TransferTimeout);
		return false;
	}

	dataType = receiver->guessDataType(buffer);    // read header type
	buffer.clear();

	if(dataType == Receiver::Undefined)   // ignore unknown
	{
		qDebug() << "Unknown dataType: " << buffer;
		return false;
	}

	return true;
}

// read until '#' into buffer, return the number of bytes read
int Connection::readDataIntoBuffer(int maxSize)
{
	if (maxSize > MaxBufferSize)
		return 0;

	int numBytesBeforeRead = buffer.size();
	if(numBytesBeforeRead == MaxBufferSize)  // buffer full
	{
		abort();
		return 0;
	}

	// read until separator
	while(bytesAvailable() > 0 && buffer.size() < maxSize)
	{
		buffer.append(read(1));
		if(buffer.endsWith(Delimiter1))
			break;
	}

	return buffer.size() - numBytesBeforeRead;
}

int Connection::getDataLength()
{
	if(bytesAvailable() <= 0 || readDataIntoBuffer() <= 0 || !buffer.endsWith(Delimiter1))
		return 0;

	buffer.chop(1);    // chop delimiter
	int number = buffer.toInt();
	buffer.clear();
	return number;
}

// wait for the data
bool Connection::hasEnoughData()
{
	if(transferTimerID)   // reset timer
	{
		QObject::killTimer(transferTimerID);
		transferTimerID = 0;
	}

	if(numBytes < 0)	  // get length
		numBytes = getDataLength();

	if(bytesAvailable() < numBytes)	// start timer and wait for data
	{
		transferTimerID = startTimer(TransferTimeout);
		return false;
	}

	return true;
}

// parse
void Connection::processData()
{
	buffer = read(numBytes);
	if(buffer.size() != numBytes)
		return abort();

	receiver->processData(dataType, buffer);

	dataType = Receiver::Undefined;   // reset status
	numBytes = -1;
	buffer.clear();
}

void Connection::onDisconnected()
{
	userNames.remove(userName);
	ready = false;
}

void Connection::setReadyForUse()
{
	userNames.insert(userName);  // add itself to online name list
	ready = true;
	emit readyForUse();
}

bool Connection::userExists(const QString& userName) {
	return userNames.contains(userName);
}

QSet<QString> Connection::userNames;


//////////////////////////////////////////////////////////////////////////
Receiver::Receiver(Connection* c) {
	connection = c;
}
Sender* Receiver::getSender() const {
	return connection->getSender();
}
QString Receiver::getUserName() const {
	return connection->getUserName();
}

Receiver::DataType Receiver::guessDataType(const QByteArray& h)
{
	QByteArray header = h;
	if(header.endsWith('#'))
		header.chop(1);
	return dataTypes.contains(header) ? dataTypes[header] : Undefined;
}

void Receiver::processData(Receiver::DataType dataType, const QByteArray& buffer) {
	if(dataType != Undefined)
		(this->*parsers[dataType])(buffer);   // call specific parser
}

void Receiver::parseGreeting(const QByteArray& userName)
{
	connection->setUserName(userName);
	if(userName.isEmpty() || connection->userExists(userName))  // check user name
	{
		connection->write(Sender::makePacket("GREETING", "WRONG_USER"));
		connection->abort();
	}
	else
	{
		connection->write(Sender::makePacket("GREETING", "OK, CONNECTED"));
		connection->setReadyForUse();
	}
}

// offline events request
void Receiver::parseReqEvents(const QByteArray& buffer)
{
	QList<QByteArray> sections = buffer.split(Connection::Delimiter1);
	if(sections.size() != 5)
		return;

	QStringList users  = QString(sections[0]).split(Connection::Delimiter2);
	QStringList events = QString(sections[1]).split(Connection::Delimiter2);
	QString startTime  = sections[2].split(Connection::Delimiter2).at(0);
	QString endTime    = sections[2].split(Connection::Delimiter2).at(1);
	QStringList phases = QString(sections[3]).split(Connection::Delimiter2);
	int fuzziness      = sections[4].toInt();
	emit reqEvents(users, events, QDateTime::fromString(startTime, MainWnd::dateTimeFormat),
								  QDateTime::fromString(endTime,   MainWnd::dateTimeFormat),
				   phases, fuzziness);
}

void Receiver::parseChat(const QByteArray& buffer)
{
	QList<QByteArray> sections = buffer.split(Connection::Delimiter1);
	if(sections.size() == 2) {
		QList<QByteArray> recipients = sections[0].split(Connection::Delimiter2);
		emit chatMessage(recipients, sections[1]);
	}
}

void Receiver::parseEvent(const QByteArray& buffer) {
	emit newEvent(connection->getUserName(), buffer);
}
void Receiver::parseRegPhoto(const QByteArray& buffer) {
	emit regPhoto(connection->getUserName(), buffer);
}
void Receiver::parseRegColor(const QByteArray& buffer) {
	emit regColor(connection->getUserName(), buffer);
}
void Receiver::parseReqOnline(const QByteArray& buffer) {
	emit reqOnline(buffer);
}
void Receiver::parseReqPhoto(const QByteArray& buffer) {
	emit reqPhoto(buffer);
}
void Receiver::parseReqTeamMembers(const QByteArray&) {
	emit reqTeamMembers();
}
void Receiver::parseReqColor(const QByteArray& buffer) {
	emit reqColor(buffer);
}
void Receiver::parseReqTimeSpan(const QByteArray&) {
	emit reqTimeSpan();
}
void Receiver::parseReqProjects(const QByteArray&) {
	emit reqProjects();
}
void Receiver::parseJoinProject(const QByteArray& buffer) {
	emit joinProject(buffer);
}
void Receiver::parseReqLocation(const QByteArray& buffer) {
	emit reqLocation(buffer);
}

void Receiver::init()
{
	// header -> date type
	dataTypes.insert("GREETING", Greeting);
	dataTypes.insert("EVENT",    Event);
	dataTypes.insert("CHAT",     Chat);

	dataTypes.insert("REG_PHOTO",    RegPhoto);
	dataTypes.insert("REG_COLOR",    RegColor);
	dataTypes.insert("JOIN_PROJECT", JoinProject);

	dataTypes.insert("REQ_ONLINE",      ReqOnline);
	dataTypes.insert("REQ_PHOTO",       ReqPhoto);
	dataTypes.insert("REQ_COLOR",       ReqColor);
	dataTypes.insert("REQ_EVENTS",      ReqEvents);
	dataTypes.insert("REQ_TIMESPAN",    ReqTimeSpan);
	dataTypes.insert("REQ_PROJECTS",    ReqProjects);
	dataTypes.insert("REQ_TEAMMEMBERS", ReqTeamMembers);
	dataTypes.insert("REQ_LOCATION",    ReqLocation);

	// data type -> parser
	parsers.insert(Greeting,       &Receiver::parseGreeting);
	parsers.insert(Event,          &Receiver::parseEvent);
	parsers.insert(Chat,           &Receiver::parseChat);
	parsers.insert(ReqEvents,      &Receiver::parseReqEvents);
	parsers.insert(JoinProject,    &Receiver::parseJoinProject);
	parsers.insert(RegPhoto,       &Receiver::parseRegPhoto);
	parsers.insert(RegColor,       &Receiver::parseRegColor);
	parsers.insert(ReqTeamMembers, &Receiver::parseReqTeamMembers);
	parsers.insert(ReqOnline,      &Receiver::parseReqOnline);
	parsers.insert(ReqPhoto,       &Receiver::parseReqPhoto);
	parsers.insert(ReqColor,       &Receiver::parseReqColor);
	parsers.insert(ReqTimeSpan,    &Receiver::parseReqTimeSpan);
	parsers.insert(ReqProjects,    &Receiver::parseReqProjects);
	parsers.insert(ReqLocation,    &Receiver::parseReqLocation);
}

QMap<QString, Receiver::DataType>          Receiver::dataTypes;
QMap<Receiver::DataType, Receiver::Parser> Receiver::parsers;

//////////////////////////////////////////////////////////////////////////
Sender::Sender(Connection* c) {
	connection = c;
}
QString Sender::getUserName() const {
	return connection->getUserName();
}

void Sender::send(const QByteArray& packet) {
	if(connection->isReadyForUse())
		connection->write(packet);
}

// make a packet from header and body
// add length and delimiters
QByteArray Sender::makePacket(const QByteArray& header, const QByteArray& body)
{
	QByteArray packet(header);
	if(!header.endsWith(Connection::Delimiter1))
		packet.append(Connection::Delimiter1);
	packet.append(QByteArray::number(body.length()) + Connection::Delimiter1 + body);
	return packet;
}

// join multiple bodies
QByteArray Sender::makePacket(const QByteArray& header, const QList<QByteArray>& bodies)
{
	QByteArray joined;
	foreach(QByteArray body, bodies)
		joined.append(body + Connection::Delimiter1);
	joined.chop(1);    // chop the last '#'
	return makePacket(header, joined);
}

// header can be customized (EVENT | EVENT_REPLY, they share the same body format)
QByteArray Sender::makeEventPacket(const QByteArray& header, const TeamRadarEvent& event) {
	return makePacket(header, QList<QByteArray>() << event.userName.toUtf8()
												  << event.eventType.toUtf8()
												  << event.parameters.toUtf8()
												  << event.time.toString(MainWnd::dateTimeFormat).toUtf8());
}

QByteArray Sender::makeEventPacket(const TeamRadarEvent& event) {
	return makeEventPacket("EVENT", event);
}

// respond to offline events request
// one request results in multiple reply, one reply for one event
QByteArray Sender::makeEventsReply(const TeamRadarEvent& event) {
	return makeEventPacket("EVENTS_REPLY", event);
}

QByteArray Sender::makeTeamMembersReply(const QList<QByteArray>& userList) {
	return makePacket("TEAMMEMBERS_REPLY", userList);
}
QByteArray Sender::makeOnlineReply(const QString& targetUser, bool online)
{
	QByteArray value = online ? "TRUE" : "FALSE";
	return makePacket("ONLINE_REPLY", QList<QByteArray>() << targetUser.toUtf8() << value);
}
QByteArray Sender::makePhotoReply(const QString& fileName, const QByteArray& photoData) {
	return makePacket("PHOTO_REPLY", QList<QByteArray>() << fileName.toUtf8() << photoData);
}
QByteArray Sender::makeColorReply(const QString& targetUser, const QByteArray& color) {
	return makePacket("COLOR_REPLY", QList<QByteArray>() << targetUser.toUtf8() << color);
}
QByteArray Sender::makeChatPacket(const QString& user, const QByteArray& content) {
	return makePacket("CHAT", QList<QByteArray>() << user.toUtf8() << content);
}
QByteArray Sender::makeTimeSpanReply(const QByteArray& start, const QByteArray& end) {
	return makePacket("TIMESPAN_REPLY", QList<QByteArray>() << start << end);
}
QByteArray Sender::makeProjectsReply(const QList<QByteArray>& projects) {
	return makePacket("PROJECTS_REPLY", projects);
}
QByteArray Sender::makeLocationReply(const QString& targetUser, const QString& location) {
	return makePacket("LOCATION_REPLY", QList<QByteArray>() << targetUser.toUtf8()
															   << location.toUtf8());
}


