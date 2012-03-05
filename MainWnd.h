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
#include "ConnectionPool.h"

struct TeamRadarEvent;
class Setting;
class Sender;

class MainWnd : public QDialog
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);

protected:
	void closeEvent(QCloseEvent* event);   // hide, instead of close
	void contextMenuEvent(QContextMenuEvent*);

private slots:
	// UI
	void onShutdown();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onAbout();
	void onExport();
	void onClearLog();
	void onDelUser();
	void onDelLogs();
	void resizeUserTable();

	// Connection
	void onPortChanged(int port);
	void onNewConnection(Connection* connection);
	void onDisconnected();
	void onReadyForUse();
	void onChangeName(const QString& oldName, const QString& newName);

	// events
	void onNewEvent(const QString& user, const QByteArray& message);
	void onRegPhoto(const QString& user, const QByteArray& photoData);
	void onRegColor(const QString& user, const QByteArray& color);
	void onChat(const QList<QByteArray>& recipients, const QByteArray& content);
	void onJointProject(const QString& projectName);
	void onReqTeamMembers();
	void onReqTimeSpan();
	void onReqProjects();
	void onReqOnline  (const QString& targetUser);
	void onReqPhoto   (const QString& targetUser);
	void onReqColor   (const QString& targetUser);
	void onReqLocation(const QString& targetUser);
	void onReqEvents  (const QStringList& users, const QStringList& eventTypes,
					   const QDateTime& startTime, const QDateTime& endTime,
					   const QStringList& phases, int fuzziness);

private:
	void createTray();
	void updateLocalAddresses();        // find local IPs
	Sender* getSender() const;          // sender of the connection responsible for the current signal
	QString getSourceUserName() const;  // user name of the connection responsible for the current signal
	QList<QByteArray> getTeamMembers(const QString& user) const;   // all members on the same project

	void broadcast(const QString& source, const QList<QByteArray>& recipients,
				   const QByteArray& packet);                          // to specific recipients (when chatting)
	void broadcast(const QString& source, const QByteArray& packet);   // to the group
	void broadcast(const TeamRadarEvent& event);                       // for convenience, to the group
	void log      (const TeamRadarEvent& event);
	Events queryEvents(const QStringList& users      = QStringList(),
					   const QStringList& eventTypes = QStringList(),
					   const QDateTime&   startTime  = QDateTime(),
					   const QDateTime&   endTime    = QDateTime());
	void contextMenuLogs (const QPoint& mousePosition);
	void contextMenuUsers(const QPoint& mousePosition);

public:
	enum {LOG_ID, LOG_TIME, LOG_CLIENT, LOG_EVENT, LOG_PARAMETERS};  // for the log model
	static const QString dateTimeFormat;

private:
	Ui::MainWndClass ui;

	Setting*         setting;
	QSystemTrayIcon* trayIcon;
	Server           server;
	ConnectionPool   connectionPool;
	QSqlTableModel   modelLogs;
	UsersModel       modelUsers;

};

int getNextID(const QString& tableName, const QString& sectionName);

#endif // MAINWND_H
