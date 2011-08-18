#include "MainWnd.h"
#include "Connection.h"
#include "../ImageColorBoolModel/ImageColorBoolDelegate.h"
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
	updateLocalAddresses();   // fetch local IPs

	// load setting
	setting = UserSetting::getInstance();
	setCurrentLocalAddress(setting->getIPAddress());   // ip
	ui.sbPort->setValue(setting->getPort());           // port
	onPortChanged(setting->getPort());                 // listen

	// tables
	modelLogs.setTable("Logs");
	modelLogs.select();
	ui.tvLogs->setModel(&modelLogs);
	ui.tvLogs->hideColumn(ID);
	ui.tvLogs->sortByColumn(TIME, Qt::DescendingOrder);
	ui.tvLogs->resizeColumnsToContents();

	UsersModel::makeAllOffline();
	modelUsers.setTable("Users");
	modelUsers.select();
	modelUsers.setColumnType(modelUsers.USERNAME, modelUsers.NameColumn);
	modelUsers.setColumnType(modelUsers.COLOR,    modelUsers.ColorColumn);
	modelUsers.setColumnType(modelUsers.IMAGE,    modelUsers.ImageColumn);
	modelUsers.setColumnType(modelUsers.ONLINE,   modelUsers.BoolColumn);
	modelUsers.setGrayImageBy(modelUsers.ONLINE);
	ui.tvUsers->setModel(&modelUsers);
	ui.tvUsers->hideColumn(modelUsers.ONLINE);
	ui.tvUsers->hideColumn(modelUsers.IMAGE);
	resizeUserTable();

	connect(&server, SIGNAL(newConnection(Connection*)), this, SLOT(onNewConnection(Connection*)));
	connect(ui.sbPort,   SIGNAL(valueChanged(int)), this, SLOT(onPortChanged(int)));
	connect(&modelUsers, SIGNAL(selected()),        this, SLOT(resizeUserTable()));
	connect(ui.btClear,  SIGNAL(clicked()),         this, SLOT(onClear()));
	connect(ui.btExport, SIGNAL(clicked()),         this, SLOT(onExport()));
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

	Receiver* receiver = connection->getReceiver();
	connect(receiver, SIGNAL(newEvent(QString, QByteArray)), this, SLOT(onNewEvent(QString, QByteArray)));
	connect(receiver, SIGNAL(requestUserList()),             this, SLOT(onRequestUserList()));
	connect(receiver, SIGNAL(registerPhoto(QString, QByteArray)), this, SLOT(onRegisterPhoto(QString, QByteArray)));
	connect(receiver, SIGNAL(registerColor(QString, QByteArray)), this, SLOT(onRegisterColor(QString, QByteArray)));
	connect(receiver, SIGNAL(requestPhoto(QString)), this, SLOT(onRequestPhoto(QString)));
	connect(receiver, SIGNAL(requestColor(QString)), this, SLOT(onRequestColor(QString)));

	// new client
	connections.insert(connection->getUserName(), connection);
	broadcast(TeamRadarEvent(connection->getUserName().toUtf8(), "CONNECTED", ""));
	
	// refresh the user table
	UsersModel::addUser   (connection->getUserName());
	UsersModel::makeOnline(connection->getUserName());
	modelUsers.select();
}

void MainWnd::removeConnection(Connection *connection)
{
	if(connectionExists(connection))
	{
		connections.remove(connection->getUserName());
		broadcast(TeamRadarEvent(connection->getUserName(), "DISCONNECTED", ""));

		UsersModel::makeOffline(connection->getUserName());
		modelUsers.select();
	}

	connection->deleteLater();
}

bool MainWnd::connectionExists(const Connection* connection) const {
	return connections.contains(connection->getUserName());
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

void MainWnd::onNewEvent(const QString& user, const QByteArray& message)
{
	QByteArray event      = message.split('#').at(0);
	QByteArray parameters = message.split('#').at(1);
	broadcast(TeamRadarEvent(user, event, parameters));
}

void MainWnd::log(const TeamRadarEvent& event)
{
	int lastRow = modelLogs.rowCount();
	modelLogs.insertRow(lastRow);
	modelLogs.setData(modelLogs.index(lastRow, ID),         getNextID("Logs", "ID"));
	modelLogs.setData(modelLogs.index(lastRow, TIME),       QDateTime::currentDateTime().toString(Qt::ISODate));
	modelLogs.setData(modelLogs.index(lastRow, CLIENT),     event.userName);
	modelLogs.setData(modelLogs.index(lastRow, EVENT),      event.eventType);
	modelLogs.setData(modelLogs.index(lastRow, PARAMETERS), event.parameter);
	modelLogs.submitAll();
	ui.tvLogs->resizeColumnsToContents();
}

void MainWnd::broadcast(const QString& sourceUser, const QByteArray& packet)
{
	foreach(Connection* connection, connections)
		if(connection->getUserName() != sourceUser)  // skip the source
			connection->getSender()->send(packet);
}

void MainWnd::broadcast(const TeamRadarEvent& event)
{
	broadcast(event.userName, Sender::makeEventPacket(event));
	log(event);
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
		   "<P>2011.8.18</P>"
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

void MainWnd::onRequestUserList()
{
	// make user list
	QList<QByteArray> users;
	foreach(Connection* connection, connections)
		users << connection->getUserName().toUtf8();

	Receiver* receiver = qobject_cast<Receiver*>(sender());
	Sender* sender = receiver->getSender();
	sender->send(sender->makeUserListResponse(users));
	log(TeamRadarEvent(receiver->getUserName(), "Request user list"));
}

void MainWnd::onRegisterPhoto(const QString& user, const QByteArray& photoData)
{
	int seperator = photoData.indexOf('#');
	if(seperator == -1)
		return;

	QByteArray suffix   = photoData.left(seperator);
	QByteArray fileData = photoData.right(photoData.length() - seperator - 1);
	QString fileName(user + "." + suffix);
	QFile file(fileName);
	if(file.open(QFile::WriteOnly | QFile::Truncate))
	{
		file.write(fileData);
		UsersModel::setImage(user, fileName);
		modelUsers.select();
		log(TeamRadarEvent(user, "Register Photo"));

		// update other users
		broadcast(user, Sender::makePhotoResponse(fileName, fileData));
	}
}

void MainWnd::onRegisterColor(const QString& user, const QByteArray& color)
{
	UsersModel::setColor(user, color);
	modelUsers.select();
	log(TeamRadarEvent(user, "Register Color"));

	// update other users
	broadcast(user, Sender::makeColorResponse(user, color));
}

void MainWnd::onRequestPhoto(const QString& targetUser)
{
	QString fileName = targetUser + ".png";
	QFile file(fileName);
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	Sender* sender = receiver->getSender();
	if(file.open(QFile::ReadOnly))
	{
		sender->send(Sender::makePhotoResponse(fileName, file.readAll()));
		log(TeamRadarEvent(sender->getUserName(), "Request photo of", targetUser));
	}
	else {
		log(TeamRadarEvent(sender->getUserName(), "Failed: Request photo of", targetUser));
	}
}

void MainWnd::onRequestColor(const QString& targetUser)
{
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	Sender* sender = receiver->getSender();
	QByteArray color = UsersModel::getColor(targetUser).toUtf8();
	sender->send(Sender::makeColorResponse(targetUser, color));
	log(TeamRadarEvent(sender->getUserName(), "Request color of", targetUser));
}

void MainWnd::resizeUserTable()
{
	ui.tvUsers->resizeRowsToContents();
	ui.tvUsers->resizeColumnsToContents();
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