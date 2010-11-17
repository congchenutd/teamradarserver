#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QDialog>
#include <QSystemTrayIcon>
#include <QMultiHash>
#include <QHostAddress>
#include <QList>
#include "ui_MainWnd.h"
#include "Server.h"
#include "../MySetting/MySetting.h"

class UserSetting;

class MainWnd : public QDialog
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();

protected:
	void closeEvent(QCloseEvent* event);

private slots:
	void onClose();
	void onShutdown();
	void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
	void onShow();
	void newConnection(Connection* connection);
	void connectionError(QAbstractSocket::SocketError socketError);
	void disconnected();
	void readyForUse();
	void onPortChanged(int port);

private:
	void createTray();
	void updateLocalAddresses();
	void removeConnection(Connection* connection);
	bool hasConnection(const QHostAddress& senderIp, int senderPort) const;
	QString getCurrentLocalAddress() const;
	void    setCurrentLocalAddress(const QString& address);

private:
	Ui::MainWndClass ui;
	UserSetting* setting;
	QSystemTrayIcon* trayIcon;
	Server server;
	QMultiHash<QHostAddress, Connection*> peers;
	QList<QHostAddress> localAddresses;
};

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

#endif // MAINWND_H
