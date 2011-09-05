######################################################################
# Automatically generated by qmake (2.01a) Fri Sep 2 17:08:55 2011
######################################################################

TEMPLATE = app
TARGET = 
DEPENDPATH += . Debug GeneratedFiles
INCLUDEPATH += .

QT += sql
QT += network
RC_FILE = TeamRadarServer.rc
RESOURCES += MainWnd.qrc

# Input
HEADERS += Connection.h \
           MainWnd.h \
           PhaseDivider.h \
           Server.h \
           TeamRadarEvent.h \
           UsersModel.h \
           ../MySetting/MySetting.h \
           ../ImageColorBoolModel/ImageColorBoolModel.h \
           ../ImageColorBoolModel/ImageColorBoolDelegate.h
FORMS += MainWnd.ui
SOURCES += Connection.cpp \
           Main.cpp \
           MainWnd.cpp \
           PhaseDivider.cpp \
           Server.cpp \
           TeamRadarEvent.cpp \
           UsersModel.cpp \
           ../ImageColorBoolModel/ImageColorBoolModel.cpp \
           ../ImageColorBoolModel/ImageColorBoolDelegate.cpp
