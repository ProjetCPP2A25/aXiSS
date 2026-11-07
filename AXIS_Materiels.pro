QT += core gui widgets network sql
CONFIG += c++17

TARGET = AXIS_Materiels
TEMPLATE = app

SOURCES += \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    materielpage.cpp \
    database.cpp \
    charts.cpp \
    employe.cpp \
    helpers.cpp \
    intervention.cpp \
    interventiondialogs.cpp \
    interventionform.cpp \
    interventionpage.cpp \
    interventiontable.cpp \
    mapview.cpp \
    riskzones.cpp \
    statspanel.cpp \
    vehicule.cpp \
    vehiculedonut.cpp \
    vehiculepage.cpp

HEADERS += \
    axis.h \
    connexion.h \
    materielpage.h \
    database.h \
    charts.h \
    style.h \
    employe.h \
    helpers.h \
    xlsxwriter.h \
    intervention.h \
    interventiondialogs.h \
    interventionform.h \
    interventionpage.h \
    interventiontable.h \
    mapview.h \
    riskzones.h \
    statspanel.h \
    vehicule.h \
    vehiculedonut.h \
    vehiculepage.h

FORMS += \
    axis.ui \
    materielpage.ui \
    vehiculepage.ui \
    interventionpage.ui

RESOURCES += \
    ressources.qrc

DISTFILES += \
    style.qss
