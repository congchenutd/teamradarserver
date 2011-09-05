#ifndef TeamRadarEvent_h__
#define TeamRadarEvent_h__

#include <QString>
#include <QDateTime>
#include <QObject>

struct TeamRadarEvent
{
	TeamRadarEvent(const QString& name, const QString& event, 
				   const QString& para = QString(), const QString& t = QString());
	QString getPhase() const;

	QString   userName;
	QString   eventType;
	QString   parameters;
	QDateTime time;
};

#endif // TeamRadarEvent_h__