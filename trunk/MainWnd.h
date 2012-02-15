#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QDialog>
#include <QSystemTrayIcon>
#include <QMultiHash>
#include <QHostAddress>
#include <QList>
#include <QSqlTableModel>
#include "ui_MainWnd.h"
#include "Server.h"
#include "UsersModel.h"
#include "TeamRadarEvent.h"

class Setting;
struct TeamRadarEvent;
class Sender;

class MainWnd : public QDialog
{
	Q_OBJECT

	typedef QMap<QString, Connection*> Connections;   // <user, connection>

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);

protected:
	void closeEvent(QCloseEvent* event);   // hide, instead of close
	void contextMenuEvent(QContextMenuEvent*);

private slots:
	void onShutdown();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onPortChanged(int port);
	void onNewConnection(Connection* connection);
	void onConnectionError();
	void onDisconnected();
	void onReadyForUse();
	void onClearLog();
	void onAbout();
	void onExport();
	void onDelete();
	void resizeUserTable();
	void onRequestUserList();   // online users for certain project
	void onRequestAllUsers();
	void onRequestTimeSpan();
	void onRequestProjects();
	void onNewEvent(const QString& user, const QByteArray& message);
	void onRegisterPhoto(const QString& user, const QByteArray& photoData);
	void onRegisterColor(const QString& user, const QByteArray& color);
	void onRequestPhoto   (const QString& targetUser);
	void onRequestColor   (const QString& targetUser);
	void onRequestLocation(const QString& targetUser);
	void onRequestEvents(const QStringList& users, const QStringList& eventTypes,
						 const QDateTime& startTime, const QDateTime& endTime,
						 const QStringList& phases, int fuzziness);
	void onChat(const QList<QByteArray>& recipients, const QByteArray& content);
	void onJointProject(const QString& projectName);

private:
	void createTray();
	void updateLocalAddresses();                               // find local IPs
	void removeConnection(Connection* connection);
	bool connectionExists(const Connection* connection) const;
	Sender* getSender() const;                                   // sender associated with the connection
	QString getSourceUserName() const;                           // get the user name of the signal
	QList<QByteArray> getCoworkers(const QString& user) const;   // peers on the same project
	QList<QByteArray> getOnlineUsers() const;

	// to specific recipients (when chatting)
	void broadcast(const QString& source, const QList<QByteArray>& recipients, const QByteArray& packet);
	void broadcast(const QString& source, const QByteArray& packet);   // to the group
	void broadcast(const TeamRadarEvent& event);
	void log      (const TeamRadarEvent& event);
	Events queryEvents(const QStringList& users      = QStringList(),
					   const QStringList& eventTypes = QStringList(),
					   const QDateTime&   startTime  = QDateTime(),
					   const QDateTime&   endTime    = QDateTime());

public:
	enum {ID, TIME, CLIENT, EVENT, PARAMETERS};  // for the log model
	static const QString dateTimeFormat;

private:
	Ui::MainWndClass ui;

	Setting* setting;
	QSystemTrayIcon* trayIcon;
	Server         server;
	Connections    connections;
	QSqlTableModel modelLogs;
	UsersModel     modelUsers;

};

int getNextID(const QString& tableName, const QString& sectionName);

#endif // MAINWND_H
