#pandora-libraries.pro - The qmake project file for pandora-libraries

TARGET = pnd
TEMPLATE = lib
CONFIG += dll
QT -= core gui
VERSION = 1.0.1

INCLUDEPATH += include/

HEADERS += \
    include/pnd_io_evdev.h \
    include/pnd_version.h \
    include/pnd_device.h \
    include/pnd_io_ioctl.h \
    include/pnd_notify.h \
    include/pnd_pxml_names.h \
    include/pnd_apps.h \
    include/pnd_conf.h \
    include/pnd_pndfiles.h \
    include/pnd_utility.h \
    include/pnd_tinyxml.h \
    include/pnd_container.h \
    include/pnd_locate.h \
    include/pnd_desktop.h \
    include/pnd_pxml.h \
    include/pnd_io_gpio.h \
    include/pnd_dbusnotify.h \
    include/pnd_discovery.h \
    include/pnd_logger.h \
    lib/tinyxml/tinystr.h \
    lib/tinyxml/tinyxml.h \
    lib/pnd_pathiter.h \
    lib/pnd_keytype.h

SOURCES += \
    lib/pnd_io_evdev.c \
    lib/pnd_locate.c \
    lib/pnd_dbusnotify.c \
    lib/pnd_utility.c \
    lib/pnd_container.c \
    lib/pnd_conf.c \
    lib/pnd_device.c \
    lib/pnd_notify.c \
    lib/pnd_io_gpio.c \
    lib/pnd_desktop.c \
    lib/pnd_pxml.c \
    lib/pnd_io_ioctl.c \
    lib/pnd_pndfiles.c \
    lib/pnd_discovery.c \
    lib/tinyxml/tinystr.cpp \
    lib/tinyxml/tinyxmlparser.cpp \
    lib/tinyxml/tinyxml.cpp \
    lib/tinyxml/tinyxmlerror.cpp \
    lib/pnd_apps.c \
    lib/pnd_logger.c \
    lib/pnd_tinyxml.cpp
