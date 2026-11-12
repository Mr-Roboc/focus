QT += core gui qml quick quickcontrols2 network testlib
CONFIG += c++17 testcase
TARGET = focus_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/apps.h ../src/backend.h ../src/daemon.h ../src/enforcer.h ../src/store.h \
           ../src/theme.h ../src/translator.h
SOURCES += focus_tests.cpp ../src/apps.cpp ../src/backend.cpp ../src/daemon.cpp ../src/enforcer.cpp \
           ../src/store.cpp ../src/theme.cpp ../src/translator.cpp
