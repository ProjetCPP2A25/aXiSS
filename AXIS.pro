QT += core gui widgets sql
CONFIG += c++17

TARGET = AXIS
TEMPLATE = app

SOURCES += \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    employe.cpp

HEADERS += \
    axis.h \
    connexion.h \
    employe.h

FORMS += axis.ui

RESOURCES += ressources.qrc

DISTFILES += style.qss
