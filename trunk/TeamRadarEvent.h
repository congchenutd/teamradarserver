#ifndef TeamRadarEvent_h__
#define TeamRadarEvent_h__

#include <QString>
#include <QDateTime>
#include <QList>

struct TeamRadarEvent
{
	TeamRadarEvent(const QString& name, const QString& event, 
				   const QString& para = QString(), const QString& t = QString());
	QString getPhase() const;
	
	bool operator< (const TeamRadarEvent& other) const;

	QString   userName;
	QString   eventType;
	QString   parameters;
	QDateTime time;
};

typedef QList<TeamRadarEvent> Events;

#endif // TeamRadarEvent_h__
