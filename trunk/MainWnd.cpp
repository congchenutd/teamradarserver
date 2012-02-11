#include "MainWnd.h"
#include "Connection.h"
#include "../ImageColorBoolModel/ImageColorBoolProxy.h"
#include "../ImageColorBoolModel/ImageColorBoolDelegate.h"
#include "PhaseDivider.h"
#include "Setting.h"
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
	setting = Setting::getInstance();
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
	modelUsers.setSort(modelUsers.ONLINE, Qt::DescendingOrder);   // "true" before "false"

	ImageColorBoolProxy* proxy = new ImageColorBoolProxy(this);
	proxy->setSourceModel(&modelUsers);
	proxy->setColumnType(modelUsers.USERNAME, proxy->NameColumn);
	proxy->setColumnType(modelUsers.COLOR,    proxy->ColorColumn);
	proxy->setColumnType(modelUsers.IMAGE,    proxy->ImageColumn);
	proxy->setColumnType(modelUsers.ONLINE,   proxy->BoolColumn);
	proxy->setGrayImageBy(modelUsers.ONLINE);
	proxy->setImageColumn(modelUsers.IMAGE);

	ui.tvUsers->setModel(proxy);
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
	Setting::destroySettingManager();
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
	onDisconnected();
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
	connect(receiver, SIGNAL(requestUserList()), this, SLOT(onRequestUserList()));
	connect(receiver, SIGNAL(requestAllUsers()), this, SLOT(onRequestAllUsers()));
	connect(receiver, SIGNAL(requestTimeSpan()), this, SLOT(onRequestTimeSpan()));
	connect(receiver, SIGNAL(requestProjects()), this, SLOT(onRequestProjects()));
	connect(receiver, SIGNAL(newEvent(QString, QByteArray)), this, SLOT(onNewEvent(QString, QByteArray)));
	connect(receiver, SIGNAL(registerPhoto(QString, QByteArray)), this, SLOT(onRegisterPhoto(QString, QByteArray)));
	connect(receiver, SIGNAL(registerColor(QString, QByteArray)), this, SLOT(onRegisterColor(QString, QByteArray)));
	connect(receiver, SIGNAL(requestPhoto(QString)), this, SLOT(onRequestPhoto(QString)));
	connect(receiver, SIGNAL(requestColor(QString)), this, SLOT(onRequestColor(QString)));
	connect(receiver, SIGNAL(requestEvents(QStringList, QStringList, QDateTime, QDateTime, QStringList, int)),
			this,     SLOT(onRequestEvents(QStringList, QStringList, QDateTime, QDateTime, QStringList, int)));
	connect(receiver, SIGNAL(chatMessage(QList<QByteArray>, QByteArray)), this, SLOT(onChat(QList<QByteArray>, QByteArray)));
	connect(receiver, SIGNAL(joinProject(QString)), this, SLOT(onJointProject(QString)));
	connect(receiver, SIGNAL(requestRecent(int)), this, SLOT(onRequestRecent(int)));

	// new client
	connections.insert(connection->getUserName(), connection);
	log(TeamRadarEvent(connection->getUserName(), "Connected"));
	
	// refresh the user table
	UsersModel::addUser   (connection->getUserName());
	UsersModel::makeOnline(connection->getUserName());
	modelUsers.select();
}

void MainWnd::removeConnection(Connection* connection)
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

// bind again
void MainWnd::onPortChanged(int port)
{
	server.close();
	server.listen(QHostAddress::Any, port);
}

void MainWnd::onNewEvent(const QString& user, const QByteArray& message)
{
	QByteArray event      = message.split(Connection::Delimiter1).at(0);
	QByteArray parameters = message.split(Connection::Delimiter1).at(1);
	broadcast(TeamRadarEvent(user, event, parameters));
}

void MainWnd::log(const TeamRadarEvent& event)
{
	int lastRow = modelLogs.rowCount();
	modelLogs.insertRow(lastRow);
	modelLogs.setData(modelLogs.index(lastRow, ID),         getNextID("Logs", "ID"));
	modelLogs.setData(modelLogs.index(lastRow, TIME),       event.time.toString(dateTimeFormat));
	modelLogs.setData(modelLogs.index(lastRow, CLIENT),     event.userName);
	modelLogs.setData(modelLogs.index(lastRow, EVENT),      event.eventType);
	modelLogs.setData(modelLogs.index(lastRow, PARAMETERS), event.parameters);
	modelLogs.submitAll();
	ui.tvLogs->resizeColumnsToContents();
}

void MainWnd::broadcast(const QString& source, const QByteArray& packet) {
	broadcast(source, getCoworkers(source), packet);   // broadcast in the group
}

void MainWnd::broadcast(const TeamRadarEvent& event)
{
	broadcast(event.userName, Sender::makeEventPacket(event));
	log(event);
}

void MainWnd::broadcast(const QString& source, const QList<QByteArray>& recipients, const QByteArray& packet)
{
	foreach(QString recipient, recipients)    // broadcast to the recipients
		if(connections.contains(recipient))   // the recipient if online
		{
			Connection* connection = connections[recipient];
			if(connection->getUserName() != source)      // skip the source
				connection->getSender()->send(packet);
		}
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
		os << modelLogs.data(modelLogs.index(i, TIME))  .toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, CLIENT)).toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, EVENT)) .toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, PARAMETERS)).toString() << "\r\n";
	modelLogs.sort(TIME, Qt::DescendingOrder);
}

void MainWnd::onRequestUserList()
{
	if(Sender* sender = getSender())
	{
		QList<QByteArray> peers = getCoworkers(getSourceDeveloperName());
		QList<QByteArray> onlinePeers;         // pick online users
		foreach(QString peer, peers)
			if(connections.contains(peer))
				onlinePeers << peer.toUtf8();

		sender->send(Sender::makeUserListResponse(onlinePeers));
		log(TeamRadarEvent(sender->getUserName(), "Request online users"));
	}
}

void MainWnd::onRequestAllUsers()
{
	if(Sender* sender = getSender())
	{
		QList<QByteArray> peers = getCoworkers(getSourceDeveloperName());
		sender->send(Sender::makeAllUsersResponse(peers));
		log(TeamRadarEvent(sender->getUserName(), "Request all users"));
	}
}

void MainWnd::onRequestTimeSpan()
{
	QSqlQuery query;
	query.exec("select min(Time), max(Time) from Logs");
	if(query.next())
	{
		QByteArray start = query.value(0).toString().toUtf8();
		QByteArray end   = query.value(1).toString().toUtf8();
		Sender* sender = getSender();
		if(sender != 0)
			sender->send(Sender::makeTimeSpanResponse(start, end));
	}
}

void MainWnd::onRequestProjects()
{
	Sender* sender = getSender();
	if(sender != 0)
		sender->send(Sender::makeProjectsResponse(UsersModel::getProjects()));
}

void MainWnd::onRegisterPhoto(const QString& user, const QByteArray& photoData)
{
	int seperator = photoData.indexOf(Connection::Delimiter1);
	if(seperator == -1)
		return;

	QByteArray suffix   = photoData.left(seperator);
	QByteArray fileData = photoData.right(photoData.length() - seperator - 1);
	QString fileName(setting->getPhotoDir() + "/" + user + "." + suffix);
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
	QString fileName = setting->getPhotoDir() + "/" + targetUser + ".png";
	QFile file(fileName);
	Sender* sender = getSender();
	if(sender == 0)
		return;
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
	QByteArray color = UsersModel::getColor(targetUser).toUtf8();
	Sender* sender = getSender();
	if(sender != 0)
	{
		sender->send(Sender::makeColorResponse(targetUser, color));
		log(TeamRadarEvent(sender->getUserName(), "Request color of", targetUser));
	}
}

void MainWnd::resizeUserTable()
{
	ui.tvUsers->resizeRowsToContents();
	ui.tvUsers->resizeColumnsToContents();
}

Events MainWnd::queryEvents(int count, const QStringList &users, const QStringList &eventTypes, const QDateTime &startTime, const QDateTime &endTime)
{
	// query without phases
	QString userClause  = users     .isEmpty() ? "1" : tr("Client in (\"%1\")").arg(users     .join("\", \""));
	QString eventClause = eventTypes.isEmpty() ? "1" : tr("Event  in (\"%1\")").arg(eventTypes.join("\", \""));
	QString timeClause  = startTime.isNull() || endTime.isNull() ? "1"
						: tr("Time between \"%1\" and \"%2\"").arg(startTime.toString(dateTimeFormat))
															  .arg(endTime  .toString(dateTimeFormat));
	QString countClause = tr("LIMIT %1").arg(count);
	QSqlQuery query;
	query.exec(tr("select Client, Event, Parameters, Time from Logs \
				  where %1 and %2 and %3 %4")
			   .arg(userClause)
			   .arg(eventClause)
			   .arg(timeClause)
			   .arg(countClause));

	Events result;
	while(query.next())
		result << TeamRadarEvent(query.value(0).toString(),
								 query.value(1).toString(),
								 query.value(2).toString(),
								 query.value(3).toString());
	return result;
}

void MainWnd::onRequestEvents(const QStringList& users, const QStringList& eventTypes,
							  const QDateTime& startTime, const QDateTime& endTime,
							  const QStringList& phases, int fuzziness)
{
	// query without phases
	Events events = queryEvents(-1, users, eventTypes, startTime, endTime);

	// filter by phases
	PhaseDivider divider(events, fuzziness);
	Events dividedEvents = divider.getEvents(phases);

	// send
	Sender* sender = getSender();
	if(sender != 0)
		foreach(TeamRadarEvent event, dividedEvents)
			sender->send(Sender::makeEventsResponse(event));
}

void MainWnd::onChat(const QList<QByteArray>& recipients, const QByteArray& content)
{
	QString sourceName = getSourceDeveloperName();
	broadcast(sourceName, recipients, Sender::makeChatPacket(sourceName, content));
}

void MainWnd::onJointProject(const QString& projectName)
{
	// remove the developer from the old group
	QString developer = getSourceDeveloperName();
	QString oldProject = UsersModel::getProject(developer);
	if(!oldProject.isEmpty() && oldProject != projectName)
		broadcast(TeamRadarEvent(developer, "DISCONNECTED", oldProject));
	
	UsersModel::setProject(developer, projectName);
	modelUsers.select();
	broadcast(TeamRadarEvent(developer, "JOINED", projectName));
}

void MainWnd::onRequestRecent(int count)
{
	Events events = queryEvents(count);

	// send
	Sender* sender = getSender();
	if(sender != 0)
		foreach(TeamRadarEvent event, events)
			sender->send(Sender::makeRecentEventsResponse(event));
}

Sender* MainWnd::getSender() const
{
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	return receiver != 0 ? receiver->getSender() : 0;
}

QString MainWnd::getSourceDeveloperName() const
{
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	return receiver != 0 ? receiver->getUserName() : QString();
}

QList<QByteArray> MainWnd::getCoworkers(const QString& developer)
{
	QString project = UsersModel::getProject(developer);
	return UsersModel::getProjectMembers(project);
}

void MainWnd::contextMenuEvent(QContextMenuEvent* event)
{
	QModelIndexList idxes = ui.tvUsers->selectionModel()->selectedRows();
	if(idxes.isEmpty())
		return;

	QMenu menu(this);
	QAction* actionDelete = new QAction("Delete", this);
	connect(actionDelete, SIGNAL(triggered()), this, SLOT(onDelete()));
	menu.addAction(actionDelete);
	menu.exec(event->globalPos());
}

void MainWnd::onDelete()
{
	QModelIndexList idxes = ui.tvUsers->selectionModel()->selectedRows();
	if(idxes.isEmpty())
		return;

	if(QMessageBox::warning(this, tr("Warning"), tr("Really delete this entry?"), 
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::No)
		return;

	modelUsers.removeRow(idxes.front().row());
}

const QString MainWnd::dateTimeFormat = "yyyy-MM-dd HH:mm:ss";

int getNextID(const QString& tableName, const QString& sectionName)
{
	QSqlQuery query;
	query.exec(QObject::tr("select max(%1) from %2").arg(sectionName).arg(tableName));
	return query.next() ? query.value(0).toInt() + 1 : 0;
}
