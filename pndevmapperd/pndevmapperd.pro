#pndevmapperd.pro - The qmake project file for pndevmapperd
TEMPLATE = app
QT -= core gui
TARGET = pndevmapperd

INCLUDEPATH = ../pnd/include
LIBS += -L../pnd -lpnd

SOURCES += pndevmapperd.c
