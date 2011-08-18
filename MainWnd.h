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
#include "../MySetting/MySetting.h"
#include "UsersModel.h"

class UserSetting;
struct TeamRadarEvent;

class MainWnd : public QDialog
{
	Q_OBJECT

	typedef QMap<QString, Connection*> Connections;

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);

protected:
	void closeEvent(QCloseEvent* event);   // hide, instead of close

private slots:
	void onShutdown();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onPortChanged(int port);
	void onNewConnection(Connection* connection);
	void onConnectionError();
	void onDisconnected();
	void onReadyForUse();
	void onClear();
	void onAbout();
	void onExport();
	void onNewEvent(const QString& user, const QByteArray& message);
	void onRequestUserList();
	void onRegisterPhoto(const QString& user, const QByteArray& photoData);
	void onRegisterColor(const QString& user, const QByteArray& color);
	void onRequestPhoto (const QString& targetUser);
	void onRequestColor (const QString& targetUser);
	void resizeUserTable();

private:
	void createTray();
	void updateLocalAddresses();                              // find local IPs
	void removeConnection(Connection* connection);
	bool connectionExists(const Connection* connection) const;
	QString getCurrentLocalAddress() const;
	void    setCurrentLocalAddress(const QString& address);

	void broadcast(const QString& sourceUser, const QByteArray& packet);
	void broadcast(const TeamRadarEvent& event);
	void log      (const TeamRadarEvent& event);

public:
	enum {ID, TIME, CLIENT, EVENT, PARAMETERS};  // for the log model

private:
	Ui::MainWndClass ui;

	UserSetting* setting;
	QSystemTrayIcon* trayIcon;
	Server         server;
	Connections    connections;
	QSqlTableModel modelLogs;
	UsersModel     modelUsers;
};


//////////////////////////////////////////////////////////////////////////
class UserSetting : public MySetting<UserSetting>
{
public:
	UserSetting(const QString& fileName);

	QString getIPAddress() const;
	quint16 getPort() const;

	void setIPAddress(const QString& address);
	void setPort(quint16 port);

private:
	void loadDefaults();
};

int getNextID(const QString& tableName, const QString& sectionName);

#endif // MAINWND_H
