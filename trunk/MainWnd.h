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

class UserSetting;

class MainWnd : public QDialog
{
	Q_OBJECT

	typedef QPair<QString, int> Address;
	typedef QHash<Address, Connection*> Clients;

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();

protected:
	void closeEvent(QCloseEvent* event);

private slots:
	void onShutdown();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onPortChanged(int port);
	void onNewConnection(Connection* connection);
	void onConnectionError();
	void onDisconnected();
	void onReadyForUse();
	void onNewMessage(const QString& user, const QString& message);
	void onClear();
	void onAbout();
	void onExport();
	void onRegisterPhoto(const QString& user, const QByteArray& photoData);
	void onRequestPhoto (const QString& user);

private:
	void createTray();
	void updateLocalAddresses();                              // find local IPs
	void removeConnection(Connection* connection);
	bool connectionExists(const Connection* connection) const;
	QString getCurrentLocalAddress() const;
	void    setCurrentLocalAddress(const QString& address);
	void log      (const QString& user, const QString& event, const QString& parameters);
	void broadcast(const QString& user, const QString& event, const QString& parameters);

public:
	enum {ID, TIME, CLIENT, EVENT, PARAMETERS};

private:
	Ui::MainWndClass ui;
	UserSetting* setting;
	QSystemTrayIcon* trayIcon;
	Server server;
	Clients clients;
	QSqlTableModel modelLogs;
	QSqlTableModel modelConnections;
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
