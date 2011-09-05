#include "TeamRadarEvent.h"
#include "MainWnd.h"

TeamRadarEvent::TeamRadarEvent(const QString& name, const QString& event, const QString& para, const QString& t)
: userName(name), eventType(event), parameters(para)
{
	time = t.isEmpty() ? QDateTime::currentDateTime() 
					   : QDateTime::fromString(t, MainWnd::dateTimeFormat);
}

QString TeamRadarEvent::getPhase() const
{
	if(eventType == "MODE")
	{
		if(parameters == "Projects")
			return "Project";
		if(parameters == "Edit")
			return "Coding";
		if(parameters == "Design")
			return "Prototyping";
		if(parameters == "Debug")
			return "Testing";
	}
	else if(eventType == "SCM_COMMIT")
		return "Deployment";
	return QString();
}