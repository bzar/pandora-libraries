#pnd_run.pro - The qmake project file for pnd_run
TEMPLATE = app
QT -= core gui
TARGET = pnd_run

INCLUDEPATH = ../pnd/include
LIBS += -L../pnd -lpnd

SOURCES += pnd_run.c
