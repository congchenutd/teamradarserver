#include "MainWnd.h"
#include "Connection.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QMenu>
#include <QNetworkInterface>
#include <QSqlQuery>
#include <QFileDialog>

MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags)
	: QDialog(parent, flags)
{
	ui.setupUi(this);
	createTray();
	updateLocalAddresses();

	// load setting
	setting = UserSetting::getInstance();
	setCurrentLocalAddress(setting->getIPAddress());
	ui.sbPort->setValue(setting->getPort());
	onPortChanged(setting->getPort());  // listen

	// database
	modelLogs.setTable("Logs");
	modelLogs.select();
	ui.tableView->setModel(&modelLogs);
	ui.tableView->hideColumn(ID);
	ui.tableView->sortByColumn(TIME, Qt::DescendingOrder);

	QSqlQuery query;
	query.exec(tr("delete from Connections"));

	modelConnections.setTable("Connections");
	modelConnections.select();
	ui.listView->setModel(&modelConnections);
	ui.listView->setModelColumn(0);

	connect(ui.sbPort, SIGNAL(valueChanged(int)), this, SLOT(onPortChanged(int)));
	connect(&server, SIGNAL(newConnection(Connection*)), this, SLOT(onNewConnection(Connection*)));
	connect(ui.btClear,  SIGNAL(clicked()), this, SLOT(onClear()));
	connect(ui.btExport, SIGNAL(clicked()), this, SLOT(onExport()));
}

void MainWnd::createTray()
{
	QMenu* trayMenu = new QMenu(this);
	trayMenu->addAction(ui.actionAbout);
	trayMenu->addAction(ui.actionShutdown);

	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setIcon(QIcon(":/MainWnd/Images/Server.png"));
	trayIcon->setContextMenu(trayMenu);
	trayIcon->show();

	connect(ui.actionAbout,    SIGNAL(triggered()), this, SLOT(onAbout()));
	connect(ui.actionShutdown, SIGNAL(triggered()), this, SLOT(onShutdown()));
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(onTrayActivated(QSystemTrayIcon::ActivationReason)));
}

void MainWnd::closeEvent(QCloseEvent* event)
{
	hide();
	event->ignore();
}

void MainWnd::onShutdown()
{
	trayIcon->hide();
	setting->setIPAddress(getCurrentLocalAddress());  // save setting
	setting->setPort(ui.sbPort->value());
	UserSetting::destroySettingManager();
	qApp->quit();
}

void MainWnd::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
	if(reason == QSystemTrayIcon::DoubleClick)
		show();
}

void MainWnd::onNewConnection(Connection* connection)
{
	connect(connection, SIGNAL(error(QAbstractSocket::SocketError)),
			this, SLOT(onConnectionError()));
	connect(connection, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	connect(connection, SIGNAL(readyForUse()),  this, SLOT(onReadyForUse()));
}

void MainWnd::onConnectionError() {
	if(Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::onDisconnected() {
	if(Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::onReadyForUse()
{
	Connection* connection = qobject_cast<Connection*>(sender());
	if(!connection || connectionExists(connection))
		return;

	connect(connection, SIGNAL(newMessage(QString, QByteArray)),
			this, SLOT(onNewMessage(QString, QByteArray)));
	connect(connection, SIGNAL(registerPhoto(QByteArray, QByteArray)),
			this, SLOT(onRegisterPhoto(QByteArray, QByteArray)));
	connect(connection, SIGNAL(requestUserList()),     this, SLOT(onRequestUserList()));
	connect(connection, SIGNAL(requestPhoto(QByteArray)), this, SLOT(onRequestPhoto(QByteArray)));

	// new client
	clients.insert(Address(connection->peerAddress().toString(), connection->peerPort()), connection);
	broadcast(connection->getUserName().toUtf8(), "CONNECTED", "");
	
	QSqlQuery query;
	query.exec(tr("insert into Connections values (\"%1\")").arg(connection->getUserName()));
	modelConnections.select();
}

void MainWnd::removeConnection(Connection *connection)
{
	if(connectionExists(connection))
	{
		clients.remove(Address(connection->peerAddress().toString(), 
							   connection->peerPort()));
		broadcast(connection->getUserName(), "DISCONNECTED", "");

		QSqlQuery query;
		query.exec(tr("delete from Connections where Username = \"%1\"").arg(connection->getUserName()));
		modelConnections.select();
	}

	connection->deleteLater();
}

bool MainWnd::connectionExists(const Connection* connection) const {
	return clients.contains(
		Address(connection->peerAddress().toString(), connection->peerPort()));
}

// find all local IP addresses
void MainWnd::updateLocalAddresses()
{
	ui.cbLocalAddresses->clear();
	foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces())
		foreach(QNetworkAddressEntry entry, interface.addressEntries())
		{
			QHostAddress address = entry.ip();
			if(address != QHostAddress::LocalHost && 
			   address.protocol() == QAbstractSocket::IPv4Protocol)   // exclude 127.0.0.1
				ui.cbLocalAddresses->addItem(address.toString());
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

void MainWnd::onNewMessage(const QString& user, const QByteArray& message)
{
	QByteArray event      = message.split('#').at(0);
	QByteArray parameters = message.split('#').at(1);
	broadcast(user, event, parameters);
}

void MainWnd::log(const QString& user, const QByteArray& event, const QString& parameters)
{
	int lastRow = modelLogs.rowCount();
	modelLogs.insertRow(lastRow);
	modelLogs.setData(modelLogs.index(lastRow, ID),         getNextID("Logs", "ID"));
	modelLogs.setData(modelLogs.index(lastRow, TIME),       QDateTime::currentDateTime().toString(Qt::ISODate));
	modelLogs.setData(modelLogs.index(lastRow, CLIENT),     user);
	modelLogs.setData(modelLogs.index(lastRow, EVENT),      event);
	modelLogs.setData(modelLogs.index(lastRow, PARAMETERS), parameters);
	modelLogs.submitAll();
}

void MainWnd::broadcast(const QString& user, const QByteArray& event, const QByteArray& parameters)
{
	for(Clients::iterator it = clients.begin(); it != clients.end(); ++it)
	{
		Connection* connection = it.value();
		if(connection->getUserName() == user)  // skip the source
			continue;
		QByteArray userName = user.split("@").front().toUtf8();
		connection->send("EVENT", QList<QByteArray>() << userName << event << parameters);
	}
	log(user, event, parameters);
}

void MainWnd::onClear()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Really clear the log?"), 
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::Yes)
	{
		QSqlQuery query;
		query.exec("delete from Logs");
		modelLogs.select();
	}
}

void MainWnd::onAbout() {
	QMessageBox::about(this, tr("About"), 
		tr("<H3>TeamRadar Server</H3>"
		   "<P>Cong Chen</P>"
		   "<P>2011.8.10</P>"
		   "<P><a href=mailto:CongChenUTD@Gmail.com>CongChenUTD@Gmail.com</a></P>"));
}

void MainWnd::onExport()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Export"), "Export.txt", 
											"Text files (*.txt);;All files (*.*)");
	if(fileName.isEmpty())
		return;
	QFile file(fileName);
	if(!file.open(QFile::WriteOnly | QFile::Truncate))
		return;
	QTextStream os(&file);
	modelLogs.sort(TIME, Qt::AscendingOrder);
	for(int i=0; i<modelLogs.rowCount(); ++i)
		os << modelLogs.data(modelLogs.index(i, TIME)).toString() << "#"
		   << modelLogs.data(modelLogs.index(i, CLIENT)).toString().split('@').front() << "#"
		   << modelLogs.data(modelLogs.index(i, EVENT)).toString() << "#"
		   << modelLogs.data(modelLogs.index(i, PARAMETERS)).toString() << "\r\n";
	modelLogs.sort(TIME, Qt::DescendingOrder);
}

void MainWnd::onRegisterPhoto(const QString& user, const QByteArray& photoData)
{
	int seperator = photoData.indexOf('#');
	if(seperator == -1)
		return;

	QByteArray suffix   = photoData.left(seperator);
	QByteArray fileData = photoData.right(photoData.length() - seperator - 1);
	QFile file(user + "." + suffix);
	if(file.open(QFile::WriteOnly | QFile::Truncate))
	{
		file.write(fileData);
		log(user, "RegisterPhoto");
	}
}

void MainWnd::onRequestPhoto(const QByteArray& targetUser)
{
	QString fileName = targetUser + ".png";
	QFile file(fileName);
	Connection* connection = qobject_cast<Connection*>(sender());
	if(file.open(QFile::ReadOnly))
	{
		QByteArray photoData = file.readAll();
		connection->send("PHOTO_RESPONSE", QList<QByteArray>() << fileName.toUtf8() << photoData);
		log(connection->getUserName(), "Request photo of " + targetUser);
	}
	else
	{
		connection->send("PHOTO_RESPONSE");
		log(connection->getUserName(), "Failed: Request photo of " + targetUser);
	}
}

void MainWnd::onRequestUserList()
{
	QList<QByteArray> users;
	foreach(Connection* connection, clients)
		users << connection->getUserName().toUtf8();

	Connection* connection = qobject_cast<Connection*>(sender());
	connection->send("USERLIST_RESPONSE", users);
	log(connection->getUserName(), "Request user list");
}

int getNextID(const QString& tableName, const QString& sectionName)
{
	QSqlQuery query;
	query.exec(QObject::tr("select max(%1) from %2").arg(sectionName).arg(tableName));
	return query.next() ? query.value(0).toInt() + 1 : 0;
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
	setIPAddress("127.0.0.1");
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