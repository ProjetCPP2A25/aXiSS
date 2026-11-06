# Projet A.X.I.S (Advanced eXpert Intervention System) - RedFlame Protection
# Module : Gestion des interventions
QT += core gui widgets sql

CONFIG   += c++17

TARGET    = CRUD
TEMPLATE  = app

SOURCES += \
    connection.cpp \
    gaxis.cpp \
    intervention.cpp \
    main.cpp

HEADERS += \
    connection.h \
    gaxis.h \
    intervention.h

FORMS += \
    gaxis.ui
