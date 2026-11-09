QT += core gui widgets sql network
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
    materiel.cpp \
    vehicule.cpp \
    entretien.cpp \
    entretiendialog.cpp \
    charts.cpp \
    vehiculedonut.cpp

HEADERS += \
    axis.h \
    connexion.h \
    employe.h \
    intervention.h \
    materiel.h \
    vehicule.h \
    entretien.h \
    entretiendialog.h \
    charts.h \
    vehiculedonut.h

FORMS += \
    axis.ui

RESOURCES += \
    ressources.qrc

DISTFILES += \
    style.qss \
    sql/oracle_employe.sql \
    sql/oracle_vehicule.sql \
    sql/oracle_vehicule_exemples.sql
