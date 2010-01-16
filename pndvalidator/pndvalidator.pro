#pndvalidator.pro - The qmake project file for pndvalidator
TEMPLATE = app
QT -= core gui
TARGET = pndvalidator

INCLUDEPATH = ../pnd/include
LIBS += -L../pnd -lpnd

# Input
SOURCES += pndvalidator.c
