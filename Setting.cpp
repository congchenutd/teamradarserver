#include "Setting.h"
#include <QDir>

Setting::Setting(const QString& fileName) : MySetting<Setting>(fileName)
{
	if(QFile(this->fileName).size() == 0)   // no setting
		loadDefaults();
}

void Setting::loadDefaults()
{
	setIPAddress("127.0.0.1");
	setPort(12345);

	QDir::current().mkdir("Photos");
	setValue("PhotoPath", "./Photos");
}

QString Setting::getIPAddress() const {
	return value("IPAddress").toString();
}

quint16 Setting::getPort() const {
	return value("Port").toInt();
}

void Setting::setIPAddress(const QString& address) {
	setValue("IPAddress", address);
}

void Setting::setPort(quint16 port) {
	setValue("Port", port);
}

QString Setting::getPhotoDir() const {
	return value("PhotoPath").toString();
}