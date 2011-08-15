#include "Connection.h"
#include <QHostAddress>
#include <QTimerEvent>

Connection::Connection(QObject *parent)
	: QTcpSocket(parent)
{
	state = WaitingForGreeting;
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
	if (timerEvent->timerId() == transferTimerID) {
		abort();
		killTimer(transferTimerID);
		transferTimerID = 0;
	}
}

void Connection::onReadyRead()
{
	if(state == WaitingForGreeting)
	{
		if(!readHeader())  // get data type and length
			return;
		if(dataType != Receiver::Greeting)
		{
			abort();
			return;
		}
		state = ReadingGreeting;
	}

	if(state == ReadingGreeting)
	{
		if(!hasEnoughData())   // read numBytes bytes of data
			return;

		buffer = read(numBytes);
		if(buffer.size() != numBytes)
		{
			abort();
			return;
		}

		userName = buffer;
		dataType = Receiver::Undefined;
		numBytes = 0;
		buffer.clear();

		if(!isValid())   // don't know why
		{
			abort();
			return;
		}

		if(!userNames.contains(userName))  // check user name
		{
			send("GREETING", "OK, CONNECTED");
			userNames.insert(userName);
		}
		else
		{
			send("GREETING", "WRONG_USER");
			return;
		}

		state = ReadyForUse;
		emit readyForUse();
	}

	do
	{
		if(dataType == Receiver::Undefined && !readHeader())  // read header, size
			return;
		if(!hasEnoughData())                        // wait data
			return;
		processData();
	} while(bytesAvailable() > 0);
}

// read data type and data length
bool Connection::readHeader()
{
	// reset timer
	if(transferTimerID)
	{
		killTimer(transferTimerID);
		transferTimerID = 0;
	}

	// read header data
	if(readDataIntoBuffer() <= 0)
	{
		transferTimerID = startTimer(TransferTimeout);
		return false;
	}

	dataType = receiver->guessDataType(buffer);
	if(dataType == Receiver::Undefined)   // ignore unknown
	{
		buffer.clear();
		return false;
	}

	buffer.clear();
	numBytes = getDataLength();
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
	// reset timer
	if(transferTimerID)
	{
		QObject::killTimer(transferTimerID);
		transferTimerID = 0;
	}

	// get length
	if(numBytes <= 0)
		numBytes = getDataLength();

	// wait for data
	if(bytesAvailable() < numBytes || numBytes <= 0)
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

void Connection::onDisconnected() {
	userNames.remove(userName);
}

QSet<QString> Connection::userNames;

void Connection::send(const QByteArray& header, const QByteArray& body)
{
	QByteArray message(header);
	if(!header.endsWith("#"))
		message.append("#");
	message.append(QByteArray::number(body.length()) + '#' + body);
	write(message);
}

void Connection::send(const QByteArray& header, const QList<QByteArray>& bodies)
{
	QByteArray joined;
	foreach(QByteArray body, bodies)
		joined.append(body + "#");
	joined.chop(1);
	send(header, joined);
}

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
	return Undefined;
}

void Receiver::processData(Receiver::DataType dataType, const QByteArray& buffer)
{
	QString userName = connection->getUserName();
	switch(dataType)
	{
	case Event:
		emit newMessage(userName, buffer);
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
	default:
		break;
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

void Sender::sendEvent(const QString& userName, const QString& event, const QString& parameters) {
	if(connection->getState() == Connection::ReadyForUse)
		connection->send("EVENT", userName.toUtf8() + "#" + event.toUtf8() + "#" + parameters.toUtf8());
}

void Sender::sendPhotoResponse(const QString& fileName, const QByteArray& photoData) {
	if(connection->getState() == Connection::ReadyForUse)
		connection->send("PHOTO_RESPONSE", QList<QByteArray>() << fileName.toUtf8() << photoData);
}

void Sender::sendUserListResponse(const QList<QByteArray>& userList) {
	if(connection->getState() == Connection::ReadyForUse)
		connection->send("USERLIST_RESPONSE", userList);
}