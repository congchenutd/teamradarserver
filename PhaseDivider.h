#ifndef PhaseDivider_h__
#define PhaseDivider_h__

#include "TeamRadarEvent.h"

class PhaseDivider
{
public:
	PhaseDivider(const Events& events, int f);
	Events getEvents(const QStringList& phases) const;

private:
	Events addPhase(const QString& phase) const;   // add events from this phase

private:
	int fuzziness;
	Events originalEvents;
};

#endif // PhaseDivider_h__
