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
    materielpage.cpp \
    charts.cpp \
    database.cpp \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    employe.cpp \
    riskzones.cpp \
    statspanel.cpp \
    vehicule.cpp \
    vehiculedonut.cpp \
    vehiculepage.cpp

HEADERS += \
    helpers.h \
    xlsxwriter.h \
    intervention.h \
    interventiondialogs.h \
    interventionform.h \
    interventionpage.h \
    interventiontable.h \
    mapview.h \
    materielpage.h \
    axis.h \
    charts.h \
    connexion.h \
    database.h \
    employe.h \
    riskzones.h \
    statspanel.h \
    style.h \
    vehicule.h \
    vehiculedonut.h \
    vehiculepage.h

FORMS += \
    axis.ui \
    interventionpage.ui \
    materielpage.ui \
    vehiculepage.ui

RESOURCES += \
    ressources.qrc

DISTFILES += \
    style.qss