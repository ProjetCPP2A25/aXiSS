QT += core gui widgets sql network
CONFIG += c++17

TARGET = AXIS_Vehicules
TEMPLATE = app

SOURCES += \
    main.cpp \
    connexion.cpp \
    vehicule.cpp \
    entretien.cpp \
    entretiendialog.cpp \
    gaxis.cpp

HEADERS += \
    connexion.h \
    vehicule.h \
    entretien.h \
    entretiendialog.h \
    gaxis.h

FORMS += \
    gaxis.ui
