#include "Connection.h"
#include <QHostAddress>

Connection::Connection(QObject *parent)
	: QTcpSocket(parent)
{
	state = WaitingForGreeting;
	dataType = Undefined;
	numBytes = -1;
	timerId = 0;
	pingTimer.setInterval(PingInterval);
	userName = tr("Unknown");

	connect(this,       SIGNAL(readyRead()),    this, SLOT(onReadyRead()));
	connect(this,       SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	connect(&pingTimer, SIGNAL(timeout()),      this, SLOT(sendPing()));
}

void Connection::onReadyRead()
{
	if(state == WaitingForGreeting)
	{
		if(!readHeader())  // get data type and length
			return;
		if(dataType != Greeting)
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
		dataType = Undefined;
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

		//pingTimer.start();                // start heart beating
		//pongTime.start();
		state = ReadyForUse;
		emit readyForUse();
	}

	do
	{
		if(dataType == Undefined && !readHeader())
			return;
		if(!hasEnoughData())
			return;
		processData();
	} while(bytesAvailable() > 0);
}

// read data type and data length
bool Connection::readHeader()
{
	// new timerid
	if(timerId)
	{
		killTimer(timerId);
		timerId = 0;
	}

	// read header data
	if(readDataIntoBuffer() <= 0)
	{
		timerId = startTimer(TransferTimeout);
		return false;
	}

	dataType = guessDataType(buffer);
	if(dataType == Undefined)   // ignore unknown
		return false;

	buffer.clear();
	numBytes = getDataLength();
	return true;
}

// read maxSize bytes into buffer, return # of bytes read
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

void Connection::sendPing()
{
	//if(pongTime.elapsed() > PongTimeout)
	//{
	//	abort();   // peer dead
	//	return;
	//}

	send("PING");
}

bool Connection::hasEnoughData()
{
	// new timerid
	if(timerId)
	{
		QObject::killTimer(timerId);
		timerId = 0;
	}

	// get length
	if(numBytes <= 0)
		numBytes = getDataLength();

	// wait for data
	if(bytesAvailable() < numBytes || numBytes <= 0)
	{
		timerId = startTimer(TransferTimeout);
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

	switch(dataType)
	{
	case Ping:
		send("PONG");
		break;
	case Pong:
		pongTime.restart();
		break;
	case Event:
		emit newMessage(userName, QString::fromUtf8(buffer));
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

	dataType = Undefined;
	numBytes = 0;
	buffer.clear();
}

Connection::DataType Connection::guessDataType(const QByteArray& header)
{
	if(header.startsWith("GREETING"))
		return Greeting;
	if(header.startsWith("PING"))
		return Ping;
	if(header.startsWith("PONG"))
		return Pong;
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

void Connection::onDisconnected()
{
	pingTimer.stop();
	userNames.remove(userName);
}

QSet<QString> Connection::userNames;

void Connection::send(const QString& header, const QString& body) {
	write(header.toUtf8() + '#' + 
		QByteArray::number(body.length()) + '#' + 
		body.toUtf8());
}

void Connection::send(const QString& header, const QStringList& bodies) {
	send(header, bodies.join("#"));
}