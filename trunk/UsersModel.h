#ifndef USERSMODEL_H
#define USERSMODEL_H

#include "../ImageColorBoolModel/ImageColorBoolModel.h"

class UsersModel : public ImageColorBoolModel
{
	Q_OBJECT

public:
	UsersModel(QObject* parent = 0);
	
	static bool openDB(const QString& name);
	static void createTables();
	static void makeAllOffline();
	static void makeOffline(const QString& name);
	static void makeOnline (const QString& name);
	static void addUser(const QString& name);
	static void setImage(const QString& name, const QString& imagePath);
	static void setColor(const QString& name, const QString& color);
	static QString getColor(const QString& name);

	bool select();

signals:
	void selected();

public:
	enum {USERNAME, ONLINE, COLOR, IMAGE};
};

#endif // USERSMODEL_H
