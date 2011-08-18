#include "Connection.h"
#include <QHostAddress>
#include <QTimerEvent>
#include <QColor>

Connection::Connection(QObject *parent) : QTcpSocket(parent)
{
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
void Connection::timerEvent(QTimerEvent* timerEvent) {
	if(timerEvent->timerId() == transferTimerID) {
		abort();
		killTimer(transferTimerID);
		transferTimerID = 0;
	}
}

// new data incoming
void Connection::onReadyRead()
{
	do {
		if(dataType == Receiver::Undefined && !readHeader())  // read header, size
			return;
		if(!hasEnoughData())                                  // wait data
			return;
		processData();
	} while(bytesAvailable() > 0);
}

// read data type and data length
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

	dataType = receiver->guessDataType(buffer);
	if(dataType == Receiver::Undefined)   // ignore unknown
	{
		qDebug() << "Unknown dataType: " << buffer;
		buffer.clear();
		return false;
	}

	buffer.clear();
	numBytes = getDataLength();   // ready to read the payload
	return true;
}

// read until '#' into buffer, return # of bytes read
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
		if(buffer.endsWith('#'))
			break;
	}

	return buffer.size() - numBytesBeforeRead;
}

int Connection::getDataLength()
{
	if (bytesAvailable() <= 0 || readDataIntoBuffer() <= 0 || !buffer.endsWith('#'))
		return 0;

	buffer.chop(1);    // chop separator
	int number = buffer.toInt();
	buffer.clear();
	return number;
}

bool Connection::hasEnoughData()
{
	if(transferTimerID)   // reset timer
	{
		QObject::killTimer(transferTimerID);
		transferTimerID = 0;
	}

	if(numBytes <= 0)	  // get length
		numBytes = getDataLength();

	if(bytesAvailable() < numBytes || numBytes <= 0)	// wait for data
	{
		transferTimerID = startTimer(TransferTimeout);
		return false;
	}

	return true;
}

void Connection::processData()
{
	buffer = read(numBytes);
	if(buffer.size() != numBytes)
	{
		abort();
		return;
	}

	receiver->processData(dataType, buffer);

	dataType = Receiver::Undefined;
	numBytes = 0;
	buffer.clear();
}

void Connection::onDisconnected()
{
	userNames.remove(userName);
	ready = false;
}

void Connection::setReadyForUse()
{
	userNames.insert(userName);  // add itself
	ready = true;
	emit readyForUse();
}

bool Connection::userExists(const QString& userName) {
	return userNames.contains(userName);
}

QSet<QString> Connection::userNames;

//////////////////////////////////////////////////////////////////////////
Receiver::DataType Receiver::guessDataType(const QByteArray& header)
{
	if(header.startsWith("GREETING"))
		return Greeting;
	if(header.startsWith("EVENT"))
		return Event;
	if(header.startsWith("REGISTER_PHOTO"))
		return RegisterPhoto;
	if(header.startsWith("REQUEST_PHOTO"))
		return RequestPhoto;
	if(header.startsWith("REQUEST_USERLIST"))
		return RequestUserList;
	if(header.startsWith("REGISTER_COLOR"))
		return RegisterColor;
	if(header.startsWith("REQUEST_COLOR"))
		return RequestColor;
	return Undefined;
}

void Receiver::processData(Receiver::DataType dataType, const QByteArray& buffer)
{
	// before connected
	if(dataType == Greeting)
		return receiveGreeting(buffer);

	// after connected
	QString userName = connection->getUserName();
	switch(dataType)
	{
	case Event:
		emit newEvent(userName, buffer);
		break;
	case RegisterPhoto:
		emit registerPhoto(userName, buffer);
		break;
	case RequestPhoto:
		emit requestPhoto(buffer);
		break;
	case RequestUserList:
		emit requestUserList();
		break;
	case RegisterColor:
		emit registerColor(userName, buffer);
		break;
	case RequestColor:
		emit requestColor(buffer);
		break;
	default:
		break;
	}
}

void Receiver::receiveGreeting(const QByteArray& buffer)
{
	QString userName = buffer;
	connection->setUserName(userName);
	if(!connection->userExists(userName))  // check user name
	{
		connection->write(Sender::makePacket("GREETING", "OK, CONNECTED"));
		connection->setReadyForUse();
	}
	else
	{
		connection->write(Sender::makePacket("GREETING", "WRONG_USER"));
		abort();
	}
}

Receiver::Receiver(Connection* c) {
	connection = c;
}

Sender* Receiver::getSender() const {
	return connection->getSender();
}

QString Receiver::getUserName() const {
	return connection->getUserName();
}


//////////////////////////////////////////////////////////////////////////
Sender::Sender(Connection* c) {
	connection = c;
}

QByteArray Sender::makePacket(const QByteArray& header, const QByteArray& body)
{
	QByteArray packet(header);
	if(!header.endsWith("#"))
		packet.append("#");
	packet.append(QByteArray::number(body.length()) + '#' + body);
	return packet;
}

QByteArray Sender::makePacket(const QByteArray& header, const QList<QByteArray>& bodies)
{
	QByteArray joined;
	foreach(QByteArray body, bodies)
		joined.append(body + "#");
	joined.chop(1);    // chop the last '#'
	return makePacket(header, joined);
}

QByteArray Sender::makeEventPacket(const TeamRadarEvent& event) {
	return makePacket("EVENT", QList<QByteArray>() << event.userName.toUtf8()
												   << event.eventType.toUtf8()
												   << event.parameter.toUtf8());
}

QByteArray Sender::makeUserListResponse(const QList<QByteArray>& userList) {
	return makePacket("USERLIST_RESPONSE", userList);
}

QByteArray Sender::makePhotoResponse(const QString& fileName, const QByteArray& photoData) {
	return makePacket("PHOTO_RESPONSE", QList<QByteArray>() << fileName.toUtf8() << photoData);
}

QByteArray Sender::makeColorResponse(const QString& targetUser, const QByteArray& color) {
	return makePacket("COLOR_RESPONSE", QList<QByteArray>() << targetUser.toUtf8() << color);
}

void Sender::send(const QByteArray& packet) {
	if(connection->isReadyForUse())
		connection->write(packet);
}

QString Sender::getUserName() const {
	return connection->getUserName();
}
