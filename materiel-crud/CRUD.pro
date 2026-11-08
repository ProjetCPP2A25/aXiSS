QT += core gui widgets sql
CONFIG += c++17

TEMPLATE = app
TARGET = CRUD

SOURCES += \
    gaxis.cpp \
    connection.cpp \
    materiel.cpp

HEADERS += \
    gaxis.h \
    connection.h \
    materiel.h

FORMS += \
    gaxis.ui
