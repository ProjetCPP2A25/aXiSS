QT += core gui widgets sql
CONFIG += c++17

TARGET = AXIS
TEMPLATE = app

# Une seule interface : axis.ui (Employés, Interventions, Véhicules, Matériels).
# Widgets personnalisés utilisés par axis.ui : BarChartWidget et DonutWidget (charts.h), VehiculeDonutWidget (vehiculedonut.h).
SOURCES += \
    main.cpp \
    axis.cpp \
    connexion.cpp \
    employe.cpp \
    intervention.cpp \
    charts.cpp \
    vehiculedonut.cpp

HEADERS += \
    axis.h \
    connexion.h \
    employe.h \
    intervention.h \
    charts.h \
    vehiculedonut.h

FORMS += \
    axis.ui

RESOURCES += \
    ressources.qrc

DISTFILES += \
    style.qss \
    sql/oracle_employe.sql
