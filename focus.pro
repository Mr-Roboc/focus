QT += core gui qml quick quickcontrols2 network

CONFIG += c++17 release
TARGET = focus
TEMPLATE = app

HEADERS += \
    src/apps.h \
    src/backend.h \
    src/cli.h \
    src/daemon.h \
    src/enforcer.h \
    src/store.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/apps.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/daemon.cpp \
    src/enforcer.cpp \
    src/store.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
