#include "PhaseDivider.h"
#include <QStringList>

PhaseDivider::PhaseDivider(const Events& events, int f)
	: fuzziness(f), originalEvents(events)
{
	qSort(originalEvents);   // sort by time
}

Events PhaseDivider::addPhase(const QString& phase) const
{
	Events result;

	// get all events in this phase
	Events events;
	double sum = 0.0;
	QDateTime start;
	foreach(TeamRadarEvent event, originalEvents)
		if(event.getPhase() == phase)
		{
			if(events.isEmpty())  // first event, the start
				start = event.time;
			else
				sum += start.secsTo(event.time);  // sum up

			events << event;
		}
	if(events.isEmpty())
		return result;

	// get the center, max radius, and radius
	double average = sum / events.size();
	QDateTime center = start.addSecs(average);
	long distanceStart = start.secsTo(center);
	long distanceEnd   = center.secsTo(events.at(events.size() - 1).time);
	long maxRadius = qMax(distanceStart, distanceEnd);
	long radius = fuzziness * maxRadius / 100;

	// produce the cluster
	foreach(TeamRadarEvent event, originalEvents)
		if(event.time < center && event.time.secsTo(center) < radius)
			result << event;
		else if(event.time >= center && center.secsTo(event.time) <= radius)
			result << event;
	return result;
}

Events PhaseDivider::getEvents(const QStringList& phases) const
{
	if(phases.isEmpty())
		return originalEvents;

	Events result;
	foreach(QString phase, phases)
		result << addPhase(phase);
	return result;
}
