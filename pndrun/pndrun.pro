#pndrun.pro - The qmake project file for pndrun
TEMPLATE = app
QT -= core gui
TARGET = pndrun

INCLUDEPATH = ../pnd/include
LIBS += -L../pnd -lpnd

SOURCES += pndrun.c
