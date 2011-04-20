#pndnotifyd.pro - The qmake project file for pndnotifyd
TEMPLATE = app
QT -= core gui
TARGET = pndnotifyd

INCLUDEPATH = ../pnd/include ../pnd

LIBS += -L../pnd -lpnd

SOURCES += pndnotifyd.c
