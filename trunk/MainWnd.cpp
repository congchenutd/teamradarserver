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

	modelConnections.setTable("Connections");
	modelConnections.select();
	ui.listView->setModel(&modelConnections);
	ui.listView->setModelColumn(0);

	QSqlQuery query;
	query.exec(tr("delete from Connections"));

	connect(ui.sbPort, SIGNAL(valueChanged(int)), this, SLOT(onPortChanged(int)));
	connect(&server, SIGNAL(newConnection(Connection*)), this, SLOT(newConnection(Connection*)));
	connect(ui.btClear,  SIGNAL(clicked()), this, SLOT(onClear()));
	connect(ui.btExport, SIGNAL(clicked()), this, SLOT(onExport()));
}

MainWnd::~MainWnd()
{
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
		show();
}

void MainWnd::newConnection(Connection* connection)
{
	connect(connection, SIGNAL(error(QAbstractSocket::SocketError)),
			this, SLOT(connectionError()));
	connect(connection, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(connection, SIGNAL(readyForUse()),  this, SLOT(readyForUse()));
}

void MainWnd::connectionError() {
	if(Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::disconnected() {
	if(Connection* connection = qobject_cast<Connection*>(sender()))
		removeConnection(connection);
}

void MainWnd::readyForUse()
{
	Connection* connection = qobject_cast<Connection*>(sender());
	if (!connection || hasConnection(connection))
		return;

	connect(connection, SIGNAL(newMessage(QString, QString)),
			this, SLOT(onNewMessage(QString, QString)));

	// new client
	clients.insert(Address(connection->peerAddress().toString(), connection->peerPort()), connection);
	broadcast(connection->getUserName(), "CONNECTED", "");
	
	QSqlQuery query;
	query.exec(tr("insert into Connections values (\"%1\")").arg(connection->getUserName()));
	modelConnections.select();
}

void MainWnd::removeConnection(Connection *connection)
{
	if(hasConnection(connection))
	{
		clients.remove(
			Address(connection->peerAddress().toString(), connection->peerPort()));
		broadcast(connection->getUserName(), "DISCONNECTED", "");

		QSqlQuery query;
		query.exec(tr("delete from Connections where Username = \"%1\"").arg(connection->getUserName()));
		modelConnections.select();
	}

	connection->deleteLater();
}

bool MainWnd::hasConnection(const Connection* connection) const
{
	return clients.contains(
		Address(connection->peerAddress().toString(), connection->peerPort()));
}

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

void MainWnd::onNewMessage(const QString& user, const QString& message)
{
	QStringList msgs   = message.split('#');
	QString event      = msgs.at(0);
	QString parameters = msgs.at(1);
	broadcast(user, event, parameters);
}

void MainWnd::log(const QString& user, const QString& event, const QString& parameters)
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

void MainWnd::broadcast(const QString& user, const QString& event, const QString& parameters)
{
	for(Clients::iterator it = clients.begin(); it != clients.end(); ++it)
	{
		if(isFrom(user, it.value()))  // skip the source
			continue;
		QString userName = user.split("@").front();
		QByteArray data = userName.toUtf8() + '#' +	event.toUtf8() + '#' + parameters.toUtf8();
		it.value()->write("EVENT#" + QByteArray::number(data.size()) + '#' + data);
	}
	log(user, event, parameters);
}

void MainWnd::onClear()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Are you sure to clear the log?"), 
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::Yes)
	{
		QSqlQuery query;
		query.exec("delete from Logs");
		modelLogs.select();
	}
}

bool MainWnd::isFrom(const QString& user, const Connection* connection)
{
	QString ipPort = user.split("@").at(1);
	QString ip   = ipPort.split(":").at(0);
	int     port = ipPort.split(":").at(1).toInt();
	return connection->peerAddress().toString() == ip && connection->peerPort() == port;
}

void MainWnd::onAbout() {
	QMessageBox::about(this, tr("About"), 
		tr("<H3>TeamRadar Server</H3>"
		   "<P>Cong Chen</P>"
		   "<P>2010.11.19</P>"
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