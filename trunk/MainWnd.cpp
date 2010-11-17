#include "MainWnd.h"
#include "Connection.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QMenu>
#include <QNetworkInterface>

MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags)
	: QDialog(parent, flags)
{
	ui.setupUi(this);
	createTray();
	updateLocalAddresses();

	setting = UserSetting::getInstance();
	setCurrentLocalAddress(setting->getIPAddress());
	ui.sbPort->setValue(setting->getPort());
	onPortChanged(setting->getPort());

	connect(ui.btClose,    SIGNAL(clicked()),   this, SLOT(onClose()));
	connect(ui.btShutdown, SIGNAL(clicked()),   this, SLOT(onShutdown()));
	connect(ui.sbPort,     SIGNAL(valueChanged(int)), this, SLOT(onPortChanged(int)));
	connect(&server, SIGNAL(newConnection(Connection*)), this, SLOT(newConnection(Connection*)));
}

MainWnd::~MainWnd()
{

}

void MainWnd::createTray()
{
	QMenu* trayMenu = new QMenu(this);
	trayMenu->addAction(ui.actionShow);
	trayMenu->addAction(ui.actionShutdown);

	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setIcon(QIcon(":/MainWnd/Images/Server.png"));
	trayIcon->setContextMenu(trayMenu);

	connect(ui.actionShow,     SIGNAL(triggered()), this, SLOT(onShow()));
	connect(ui.actionShutdown, SIGNAL(triggered()), this, SLOT(onShutdown()));
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(onTrayActivated(QSystemTrayIcon::ActivationReason)));
}

void MainWnd::closeEvent(QCloseEvent* event)
{
	onClose();
	event->ignore();
}

void MainWnd::onClose()
{
	trayIcon->show();
	hide();
}

void MainWnd::onShutdown()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Are you sure to shutdown the server?"), 
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::Yes)
	{
		trayIcon->hide();
		setting->setIPAddress(getCurrentLocalAddress());
		setting->setPort(ui.sbPort->value());
		UserSetting::destroySettingManager();
		qApp->quit();
	}
}

void MainWnd::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::DoubleClick)
		onShow();
}

void MainWnd::onShow()
{
	show();
	trayIcon->hide();
}

void MainWnd::newConnection(Connection* connection)
{
//	connection->setGreetingMessage(peerManager->userName());

	connect(connection, SIGNAL(error(QAbstractSocket::SocketError)),
			this, SLOT(connectionError(QAbstractSocket::SocketError)));
	connect(connection, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(connection, SIGNAL(readyForUse()), this, SLOT(readyForUse()));
}

void MainWnd::connectionError(QAbstractSocket::SocketError socketError)
{
	if (Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::disconnected()
{
	if (Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::readyForUse()
{
	Connection* connection = qobject_cast<Connection*>(sender());
	if (!connection || 
		hasConnection(connection->peerAddress(), connection->peerPort()))
		return;

	//connect(connection, SIGNAL(newMessage(QString, QString)),
	//		this, SIGNAL(newMessage(QString, QString)));

	peers.insert(connection->peerAddress(), connection);
	//QString nick = connection->name();
	//if (!nick.isEmpty())
	//	emit newParticipant(nick);
}

void MainWnd::removeConnection(Connection *connection)
{
	if (peers.contains(connection->peerAddress()))
	{
		peers.remove(connection->peerAddress());
		//QString nick = connection->name();
		//if (!nick.isEmpty())
		//	emit participantLeft(nick);
	}
	connection->deleteLater();
}

bool MainWnd::hasConnection(const QHostAddress& senderIp, int senderPort) const
{
	if (senderPort == -1)  // no such port
		return peers.contains(senderIp);

	if (!peers.contains(senderIp))  // no such ip
		return false;

	// check ip:port
	QList<Connection*> connections = peers.values(senderIp);
	foreach (Connection* connection, connections) {
		if (connection->peerPort() == senderPort)
			return true;
	}

	return false;
}

void MainWnd::updateLocalAddresses()
{
	localAddresses.clear();
	ui.cbLocalAddresses->clear();
	foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces())
		foreach(QNetworkAddressEntry entry, interface.addressEntries())
		{
			QHostAddress address = entry.ip();
			if(address != QHostAddress::LocalHost && 
			   address.protocol() == QAbstractSocket::IPv4Protocol)   // exclude 127.0.0.1
			{
				localAddresses << address;
				ui.cbLocalAddresses->addItem(address.toString());
			}
		}
}

QString MainWnd::getCurrentLocalAddress() const {
	return ui.cbLocalAddresses->currentText();
}

void MainWnd::setCurrentLocalAddress(const QString& address) {
	ui.cbLocalAddresses->setCurrentIndex(
		ui.cbLocalAddresses->findText(address));
}

void MainWnd::onPortChanged(int port)
{
	server.close();
	server.listen(QHostAddress::Any, port);
}


//////////////////////////////////////////////////////////////////////////
// Setting
UserSetting::UserSetting(const QString& fileName) : MySetting<UserSetting>(fileName)
{
	if(QFile(this->fileName).size() == 0)   // no setting
		loadDefaults();
}

void UserSetting::loadDefaults()
{
	setIPAddress("0.0.0.0");
	setPort(12345);
}

QString UserSetting::getIPAddress() const {
	return value("IPAddress").toString();
}

quint16 UserSetting::getPort() const {
	return value("Port").toInt();
}

void UserSetting::setIPAddress(const QString& address) {
	setValue("IPAddress", address);
}

void UserSetting::setPort(quint16 port) {
	setValue("Port", port);
}