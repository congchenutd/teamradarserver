#ifndef PhaseDivider_h__
#define PhaseDivider_h__

#include "Connection.h"
#include <QList>

typedef QList<TeamRadarEvent> Events;

struct Test
{
	Test(const QString& name, const QString& event, 
		const QString& para = QString(), const QString& t = QString()) {}

	QString   userName;
	QString   eventType;
	QString   parameters;
	QDateTime time;
};

class PhaseDivider
{
public:
	PhaseDivider(const Events& events, int f);
//	Events getResult() const { return resultEvents; }
	void addPhase(const QString& phase);

private:

private:
	int fuzziness;
//	Events originalEvents;
//	Events resultEvents;
	QList<Test> list;
};


#endif // PhaseDivider_h__
