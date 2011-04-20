#pnd_info.pro - The qmake project file for pnd_info
TEMPLATE = app
QT -= core gui
TARGET = pnd_info

INCLUDEPATH = ../pnd/include
LIBS += -L../pnd -lpnd

SOURCES += pnd_info.c
