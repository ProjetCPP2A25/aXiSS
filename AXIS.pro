QT += core gui widgets network sql
CONFIG += c++17

TARGET = AXIS
TEMPLATE = app

SOURCES += \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    employe.cpp \
    conge.cpp \
    sidebar.cpp \
    smtp.cpp

HEADERS += \
    axis.h \
    connexion.h \
    employe.h \
    conge.h \
    sidebar.h \
    smtp.h

FORMS += axis.ui

RESOURCES += ressources.qrc

DISTFILES += style.qss
