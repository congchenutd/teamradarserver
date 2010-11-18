#include "Connection.h"

static const int TransferTimeout = 30 * 1000;
static const int PongTimeout = 60 * 1000;
static const int PingInterval = 5 * 1000;
static const char SeparatorToken = '#';

Connection::Connection(QObject *parent)
	: QTcpSocket(parent)
{
	state = WaitingForGreeting;
	dataType = Undefined;
	numBytes = -1;
	timerId = 0;
	isGreetingSent = false;
	pingTimer.setInterval(PingInterval);

	connect(this, SIGNAL(readyRead()), this, SLOT(processReadyRead()));
	connect(this, SIGNAL(connected()), this, SLOT(sendGreetingMessage()));
	connect(this, SIGNAL(disconnected()), &pingTimer, SLOT(stop()));
	connect(&pingTimer, SIGNAL(timeout()), this, SLOT(sendPing()));
}

Connection::~Connection()
{

}

void Connection::processReadyRead()
{
	if(state == WaitingForGreeting)
	{
		if (!readHeader())
			return;
		if (dataType != Greeting) {
			abort();
			return;
		}
		state = ReadingGreeting;
	}

	if (state == ReadingGreeting) {
		if (!hasEnoughData())
			return;

		buffer = read(numBytes);
		if (buffer.size() != numBytes) {
			abort();
			return;
		}

		//username = QString(buffer) + '@' + peerAddress().toString() + ':'
		//	+ QString::number(peerPort());
		dataType = Undefined;
		numBytes = 0;
		buffer.clear();

		if (!isValid()) {
			abort();
			return;
		}

		if (!isGreetingSent)
			sendGreetingMessage();

		pingTimer.start();
		pongTime.start();
		state = ReadyForUse;
		emit readyForUse();
	}

	do {
		if (dataType == Undefined) {
			if (!readHeader())
				return;
		}
		if (!hasEnoughData())
			return;
		processData();
	} while (bytesAvailable() > 0);
}

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

	if(buffer.startsWith("PING")) {
		dataType = Ping;
	} else if (buffer.startsWith("PONG")) {
		dataType = Pong;
	} else if (buffer.startsWith("MESSAGE")) {
		dataType = PlainText;
	} else if (buffer.startsWith("GREETING")) {
		dataType = Greeting;
	} else {
		dataType = Undefined;
//		abort();
		return false;
	}

	buffer.clear();
	numBytes = getDataLength();
	return true;
}

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

	while(bytesAvailable() > 0 && buffer.size() < maxSize)
	{
		buffer.append(read(1));
		if(buffer.endsWith(SeparatorToken))
			break;
	}

	return buffer.size() - numBytesBeforeRead;
}

int Connection::getDataLength()
{
	if (bytesAvailable() <= 0 || readDataIntoBuffer() <= 0 || !buffer.endsWith(SeparatorToken))
		return 0;

	buffer.chop(1);  // chop separator
	int number = buffer.toInt();
	buffer.clear();
	return number;
}

void Connection::sendPing()
{

}

void Connection::sendGreetingMessage()
{
	QByteArray greeting = "Hello from server";
	QByteArray data = "GREETING " + QByteArray::number(greeting.size()) + ' ' + greeting;
	if(write(data) == data.size())
		isGreetingSent = true;
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
	if (buffer.size() != numBytes) {
		abort();
		return;
	}

	switch (dataType)
	{
	case PlainText:
//		emit newMessage(username, QString::fromUtf8(buffer));
		break;
	case Ping:
		write("PONG");
		break;
	case Pong:
		pongTime.restart();
		break;
	default:
		break;
	}

	dataType = Undefined;
	numBytes = 0;
	buffer.clear();
}