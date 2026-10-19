QT += core gui widgets network sql
CONFIG += c++17

TARGET = AXIS
TEMPLATE = app

SOURCES += \
    helpers.cpp \
    intervention.cpp \
    interventiondialogs.cpp \
    interventionform.cpp \
    interventionpage.cpp \
    interventiontable.cpp \
    mapview.cpp \
    charts.cpp \
    database.cpp \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    employe.cpp \
    conge.cpp \
    riskzones.cpp \
    sidebar.cpp \
    smtp.cpp \
    statspanel.cpp

HEADERS += \
    helpers.h \
    xlsxwriter.h \
    intervention.h \
    interventiondialogs.h \
    interventionform.h \
    interventionpage.h \
    interventiontable.h \
    mapview.h \
    axis.h \
    charts.h \
    connexion.h \
    database.h \
    employe.h \
    conge.h \
    riskzones.h \
    sidebar.h \
    smtp.h \
    statspanel.h \
    style.h

FORMS += \
    axis.ui \
    interventionpage.ui

RESOURCES += \
    ressources.qrc

DISTFILES += \
    style.qss