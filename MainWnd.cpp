#include "MainWnd.h"
#include "Connection.h"
#include "ImageColorBoolProxy.h"
#include "ImageColorBoolDelegate.h"
#include "PhaseDivider.h"
#include "Setting.h"
#include <QMessageBox>
#include <QCloseEvent>
#include <QMenu>
#include <QNetworkInterface>
#include <QSqlQuery>
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QtAlgorithms>

MainWnd::MainWnd(QWidget *parent, Qt::WFlags flags)
	: QDialog(parent, flags)
{
	ui.setupUi(this);
	createTray();
	updateLocalAddresses();   // fetch local IPs

	// load setting
	setting = Setting::getInstance();
	ui.cbLocalAddresses->setCurrentIndex(
				ui.cbLocalAddresses->findText(setting->getIPAddress()));
	ui.sbPort->setValue(setting->getPort());
	onPortChanged(setting->getPort());           // listen

	// tables
	modelLogs.setTable("Logs");
	modelLogs.select();
	ui.tvLogs->setModel(&modelLogs);
	ui.tvLogs->hideColumn(LOG_ID);
	ui.tvLogs->sortByColumn(LOG_TIME, Qt::DescendingOrder);
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
	connect(&modelUsers,   SIGNAL(selected()),        this, SLOT(resizeUserTable()));
	connect(ui.sbPort,     SIGNAL(valueChanged(int)), this, SLOT(onPortChanged(int)));
	connect(ui.btClearLog, SIGNAL(clicked()),         this, SLOT(onClearLog()));
	connect(ui.btExport,   SIGNAL(clicked()),         this, SLOT(onExport()));
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
	setting->setIPAddress(ui.cbLocalAddresses->currentText());  // save setting
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
	connect(receiver, SIGNAL(reqTeamMembers()), this, SLOT(onReqTeamMembers()));
	connect(receiver, SIGNAL(reqTimeSpan()),    this, SLOT(onReqTimeSpan()));
	connect(receiver, SIGNAL(reqProjects()),    this, SLOT(onReqProjects()));
	connect(receiver, SIGNAL(newEvent(QString, QByteArray)), this, SLOT(onNewEvent(QString, QByteArray)));
	connect(receiver, SIGNAL(regPhoto(QString, QByteArray)), this, SLOT(onRegPhoto(QString, QByteArray)));
	connect(receiver, SIGNAL(regColor(QString, QByteArray)), this, SLOT(onRegColor(QString, QByteArray)));
	connect(receiver, SIGNAL(reqOnline  (QString)), this, SLOT(onReqOnline  (QString)));
	connect(receiver, SIGNAL(reqPhoto   (QString)), this, SLOT(onReqPhoto   (QString)));
	connect(receiver, SIGNAL(reqColor   (QString)), this, SLOT(onReqColor   (QString)));
	connect(receiver, SIGNAL(reqLocation(QString)), this, SLOT(onReqLocation(QString)));
	connect(receiver, SIGNAL(reqEvents(QStringList, QStringList, QDateTime, QDateTime, QStringList, int)),
			this,     SLOT(onReqEvents(QStringList, QStringList, QDateTime, QDateTime, QStringList, int)));
	connect(receiver, SIGNAL(chatMessage(QList<QByteArray>, QByteArray)),
			this, SLOT(onChat(QList<QByteArray>, QByteArray)));
	connect(receiver, SIGNAL(joinProject(QString)), this, SLOT(onJointProject(QString)));

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
		broadcast(TeamRadarEvent(connection->getUserName(), "DISCONNECTED"));

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
	modelLogs.setData(modelLogs.index(lastRow, LOG_ID),         getNextID("Logs", "ID"));
	modelLogs.setData(modelLogs.index(lastRow, LOG_TIME),       event.time.toString(dateTimeFormat));
	modelLogs.setData(modelLogs.index(lastRow, LOG_CLIENT),     event.userName);
	modelLogs.setData(modelLogs.index(lastRow, LOG_EVENT),      event.eventType);
	modelLogs.setData(modelLogs.index(lastRow, LOG_PARAMETERS), event.parameters);
	modelLogs.submitAll();
	ui.tvLogs->resizeColumnsToContents();
}

// broadcast packet to the group
void MainWnd::broadcast(const QString& source, const QByteArray& packet) {
	broadcast(source, getTeamMembers(source), packet);
}

// broadcast event to the group
void MainWnd::broadcast(const TeamRadarEvent& event)
{
	broadcast(event.userName, Sender::makeEventPacket(event));
	log(event);
}

// broadcast packet from source to recipients
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

void MainWnd::onAbout() {
	QMessageBox::about(this, tr("About"),
		tr("<H3>TeamRadar Server</H3>"
		   "<P>Cong Chen &lt;<a href=mailto:CongChenUTD@Gmail.com>CongChenUTD@Gmail.com</a>&gt;</P>"
		   "<P>Built on %1</P>")
					   .arg(Setting::getInstance()->getCompileDate()));
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
	modelLogs.sort(LOG_TIME, Qt::AscendingOrder);
	for(int i=0; i<modelLogs.rowCount(); ++i)
		os << modelLogs.data(modelLogs.index(i, LOG_TIME))  .toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, LOG_CLIENT)).toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, LOG_EVENT)) .toString() << Connection::Delimiter1
		   << modelLogs.data(modelLogs.index(i, LOG_PARAMETERS)).toString() << "\r\n";
	modelLogs.sort(LOG_TIME, Qt::DescendingOrder);   // restore order
}

void MainWnd::onReqTeamMembers() {
	if(Sender* sender = getSender())
	{
		QList<QByteArray> allPeers = getTeamMembers(getSourceUserName());
		sender->send(Sender::makeTeamMembersReply(allPeers));
		log(TeamRadarEvent(sender->getUserName(), "Request all users"));
	}
}

void MainWnd::onReqTimeSpan()
{
	QSqlQuery query;
	query.exec("select min(Time), max(Time) from Logs");
	if(query.next())
	{
		QByteArray start = query.value(0).toString().toUtf8();
		QByteArray end   = query.value(1).toString().toUtf8();
		if(Sender* sender = getSender())
			sender->send(Sender::makeTimeSpanReply(start, end));
	}
}

void MainWnd::onReqProjects() {
	if(Sender* sender = getSender())
		sender->send(Sender::makeProjectsReply(UsersModel::getProjects()));
}

void MainWnd::onRegPhoto(const QString& user, const QByteArray& photoData)
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
		// save the file to disk and db
		file.write(fileData);
		UsersModel::setImage(user, fileName);
		modelUsers.select();
		log(TeamRadarEvent(user, "Register Photo"));

		// update other users
		broadcast(user, Sender::makePhotoReply(fileName, fileData));
	}
}

void MainWnd::onRegColor(const QString& user, const QByteArray& color)
{
	UsersModel::setColor(user, color);
	modelUsers.select();
	log(TeamRadarEvent(user, "Register Color"));

	// update other users
	broadcast(user, Sender::makeColorReply(user, color));
}

void MainWnd::onReqOnline(const QString& targetUser) {
	if(Sender* sender = getSender())
	{
		sender->send(Sender::makeOnlineReply(targetUser, UsersModel::isOnline(targetUser)));
		log(TeamRadarEvent(sender->getUserName(), "Request online status of ", targetUser));
	}
}

void MainWnd::onReqPhoto(const QString& targetUser)
{
	QString fileName = setting->getPhotoDir() + "/" + targetUser + ".png";
	QFile file(fileName);
	Sender* sender = getSender();
	if(sender == 0)
		return;
	if(file.open(QFile::ReadOnly))
	{
		sender->send(Sender::makePhotoReply(fileName, file.readAll()));
		log(TeamRadarEvent(sender->getUserName(), "Request photo of", targetUser));
	}
	else {
		log(TeamRadarEvent(sender->getUserName(), "Failed: Request photo of", targetUser));
	}
}

void MainWnd::onReqColor(const QString& targetUser)
{
	QByteArray color = UsersModel::getColor(targetUser).toUtf8();
	if(Sender* sender = getSender())
	{
		sender->send(Sender::makeColorReply(targetUser, color));
		log(TeamRadarEvent(sender->getUserName(), "Request color of", targetUser));
	}
}

void MainWnd::resizeUserTable()
{
	ui.tvUsers->resizeRowsToContents();
	ui.tvUsers->resizeColumnsToContents();
}

Events MainWnd::queryEvents(const QStringList& users, const QStringList& eventTypes,
							const QDateTime& startTime, const QDateTime& endTime)
{
	// query without phases
	QString userClause  = users     .isEmpty() ? "1" : tr("Client in (\"%1\")").arg(users     .join("\", \""));
	QString eventClause = eventTypes.isEmpty() ? "1" : tr("Event  in (\"%1\")").arg(eventTypes.join("\", \""));
	QString timeClause  = startTime.isNull() || endTime.isNull() ? "1"
						: tr("Time between \"%1\" and \"%2\"").arg(startTime.toString(dateTimeFormat))
															  .arg(endTime  .toString(dateTimeFormat));
	QSqlQuery query;
	query.exec(tr("select Client, Event, Parameters, Time from Logs \
				  where %1 and %2 and %3 order by Time")
			   .arg(userClause)
			   .arg(eventClause)
			   .arg(timeClause));

	Events result;
	while(query.next())
		result << TeamRadarEvent(query.value(0).toString(),
								 query.value(1).toString(),
								 query.value(2).toString(),
								 query.value(3).toString());
	return result;
}

void MainWnd::onReqEvents(const QStringList& users, const QStringList& eventTypes,
							  const QDateTime& startTime, const QDateTime& endTime,
							  const QStringList& phases, int fuzziness)
{
	// query without phases
	Events events = queryEvents(users, eventTypes, startTime, endTime);

	// filter by phases
	PhaseDivider divider(events, fuzziness);
	Events dividedEvents = divider.getEvents(phases);

	// send
	if(Sender* sender = getSender())
		foreach(TeamRadarEvent event, dividedEvents)
			sender->send(Sender::makeEventsReply(event));
}

void MainWnd::onChat(const QList<QByteArray>& recipients, const QByteArray& content)
{
	QString sourceName = getSourceUserName();
	broadcast(sourceName, recipients, Sender::makeChatPacket(sourceName, content));
}

void MainWnd::onJointProject(const QString& projectName)
{
	// remove the developer from the old group
	QString developer = getSourceUserName();
	QString oldProject = UsersModel::getProject(developer);
	if(!oldProject.isEmpty() && oldProject != projectName)
		broadcast(TeamRadarEvent(developer, "DISCONNECTED", oldProject));

	UsersModel::setProject(developer, projectName);
	modelUsers.select();
	broadcast(TeamRadarEvent(developer, "JOINED", projectName));
}

// send a SAVE event to update the client's display of targetUser's location
void MainWnd::onReqLocation(const QString& targetUser)
{
	// find the last SAVE
	QSqlQuery query;
	query.exec(tr("select Parameters from Logs where Client = \"%1\" \
				  and Event = \"SAVE\" order by Time desc").arg(targetUser));
	if(query.next()) {
		if(Sender* sender = getSender())
		{
			sender->send(Sender::makeEventPacket(
							 TeamRadarEvent(targetUser, "SAVE", query.value(0).toString())));
			log(TeamRadarEvent(sender->getUserName(), "Request location of", targetUser));
		}
	}
}

Sender* MainWnd::getSender() const
{
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	return receiver != 0 ? receiver->getSender() : 0;
}

QString MainWnd::getSourceUserName() const
{
	Receiver* receiver = qobject_cast<Receiver*>(sender());
	return receiver != 0 ? receiver->getUserName() : QString();
}

QList<QByteArray> MainWnd::getTeamMembers(const QString& user) const {
	return UsersModel::getProjectMembers(UsersModel::getProject(user));
}

void MainWnd::contextMenuEvent(QContextMenuEvent* event)
{
	if(ui.tabWidget->currentIndex() == 0)
		contextMenuLogs(event->globalPos());
	else
		contextMenuUsers(event->globalPos());
}

void MainWnd::contextMenuLogs(const QPoint& mousePosition)
{
	// must select one or more
	QModelIndexList idxes = ui.tvLogs->selectionModel()->selectedRows();
	if(idxes.isEmpty())
		return;

	QMenu menu(this);
	QAction* actionDelete = new QAction("Delete", this);
	connect(actionDelete, SIGNAL(triggered()), this, SLOT(onDelLogs()));
	menu.addAction(actionDelete);
	menu.exec(mousePosition);
}

void MainWnd::contextMenuUsers(const QPoint& mousePosition)
{
	// must select one or more
	QModelIndexList idxes = ui.tvUsers->selectionModel()->selectedRows();
	if(idxes.isEmpty())
		return;

	QMenu menu(this);
	QAction* actionDelete = new QAction("Delete", this);
	connect(actionDelete, SIGNAL(triggered()), this, SLOT(onDelUser()));
	menu.addAction(actionDelete);
	menu.exec(mousePosition);
}

void MainWnd::onClearLog()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Really clear the log?"),
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::Yes)
	{
		QSqlQuery query;
		query.exec("delete from Logs");
		modelLogs.select();
	}
}

void MainWnd::onDelUser()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Really delete this entry?"),
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::No)
		return;

	QModelIndexList indexes = ui.tvUsers->selectionModel()->selectedRows();
	qSort(indexes.begin(), indexes.end(), qGreater<QModelIndex>());
	foreach(const QModelIndex& idx, indexes)
		modelUsers.removeRow(idx.row());
}

void MainWnd::onDelLogs()
{
	if(QMessageBox::warning(this, tr("Warning"), tr("Really delete the log?"),
		QMessageBox::Yes | QMessageBox::No)	== QMessageBox::No)
		return;

	QModelIndexList indexes = ui.tvLogs->selectionModel()->selectedRows();
	qSort(indexes.begin(), indexes.end(), qGreater<QModelIndex>());
	foreach(const QModelIndex& idx, indexes)   // delete selected
		modelLogs.removeRow(idx.row());
}

const QString MainWnd::dateTimeFormat = "yyyy-MM-dd HH:mm:ss";

int getNextID(const QString& tableName, const QString& sectionName)
{
	QSqlQuery query;
	query.exec(QObject::tr("select max(%1) from %2").arg(sectionName).arg(tableName));
	return query.next() ? query.value(0).toInt() + 1 : 0;
}
