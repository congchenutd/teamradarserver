######################################################################
# Automatically generated by qmake (2.01a) Mon Oct 17 18:20:16 2011
######################################################################

TEMPLATE = app
TARGET =
DEPENDPATH += . Debug GeneratedFiles
INCLUDEPATH += .

QT += sql
QT += network
RC_FILE = TeamRadarServer.rc

INCLUDEPATH += ../ImageColorBoolModel

# Input
HEADERS += Connection.h \
		   MainWnd.h \
		   PhaseDivider.h \
		   Server.h \
		   Setting.h \
		   TeamRadarEvent.h \
		   UsersModel.h \
		   ../ImageColorBoolModel/ImageColorBoolProxy.h \
		   ../ImageColorBoolModel/ImageColorBoolDelegate.h \
		   ../MySetting/MySetting.h
FORMS += MainWnd.ui
SOURCES += Connection.cpp \
		   Main.cpp \
		   MainWnd.cpp \
		   PhaseDivider.cpp \
		   Server.cpp \
		   Setting.cpp \
		   TeamRadarEvent.cpp \
		   UsersModel.cpp \
		   ../ImageColorBoolModel/ImageColorBoolProxy.cpp \
		   ../ImageColorBoolModel/ImageColorBoolDelegate.cpp
RESOURCES += MainWnd.qrc
