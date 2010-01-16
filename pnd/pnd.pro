#pnd.pro - The qmake project file for pnd

TARGET = pnd
TEMPLATE = lib
CONFIG += staticlib
VERSION = 1.0.1

INCLUDEPATH += include/

HEADERS += include/pnd_apps.h \
           include/pnd_conf.h \
           include/pnd_container.h \
           include/pnd_desktop.h \
           include/pnd_device.h \
           include/pnd_discovery.h \
           include/pnd_io_gpio.h \
           include/pnd_locate.h \
           include/pnd_logger.h \
           include/pnd_notify.h \
           include/pnd_pathiter.h \
           include/pnd_pndfiles.h \
           include/pnd_pxml.h \
           include/pnd_pxml_names.h \
           include/pnd_tinyxml.h \
           include/pnd_utility.h \
           include/pnd_version.h \
           src/pnd_keytype.h \
           src/tinyxml/tinystr.h \
           src/tinyxml/tinyxml.h
SOURCES += src/pnd_apps.c \
           src/pnd_conf.c \
           src/pnd_container.c \
           src/pnd_desktop.c \
           src/pnd_device.c \
           src/pnd_discovery.c \
           src/pnd_io_gpio.c \
           src/pnd_locate.c \
           src/pnd_logger.c \
           src/pnd_notify.c \
           src/pnd_pndfiles.c \
           src/pnd_pxml.c \
           src/pnd_tinyxml.cpp \
           src/pnd_utility.c \
           src/tinyxml/tinystr.cpp \
           src/tinyxml/tinyxml.cpp \
           src/tinyxml/tinyxmlerror.cpp \
           src/tinyxml/tinyxmlparser.cpp
