#ifndef Setting_h__
#define Setting_h__

#include "../MySetting/MySetting.h"

class Setting : public MySetting<Setting>
{
public:
	Setting(const QString& fileName);

	QString getIPAddress() const;
	quint16 getPort() const;
	QString getPhotoDir() const;
	QString getCompileDate() const;

	void setIPAddress(const QString& address);
	void setPort(quint16 port);

private:
	void loadDefaults();
};

#endif // Setting_h__
