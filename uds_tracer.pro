QT       += core gui
QT += xml
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

message("Using Qt version $$[QT_VERSION]")

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    cantabform/canfdform.cpp \
    cantabform/canreplayform.cpp \
    cantabform/canstdform.cpp \
    cantabform/cantabform.cpp \
    logic/replaycan.cpp \
    logic/traceuds.cpp

SOURCES += \
    tracertabform/tracertabform.cpp

SOURCES += \
    logic/cmd/cmdcancfg.cpp \
    logic/cmd/cmd.cpp \
    logic/cmd/cmddef.cpp \
    logic/cmd/cmdfileop.cpp

SOURCES += \
    logic/cobs/cobs.c

SOURCES += \
    logic/isotp/isotp.cpp

SOURCES += \
    logic/peak/peakbasiccan.cpp \
    logic/peak/peakfdcan.cpp \
    logic/peak/peakstdcan.cpp

SOURCES += \
    logic/uds/uds.cpp \
    logic/uds/gen/uds_def.cpp

SOURCES += \
    logic/can.cpp \
    logic/cli.cpp \
    logic/config.cpp \
    logic/util.cpp


SOURCES += \
    mainwindow.cpp \
    main.cpp

INCLUDEPATH += $$PWD/cantabform
win32 {
    INCLUDEPATH += $$PWD/drivers/peak-win-V4.10.1.968
}
INCLUDEPATH += $$PWD/logic/cmd
INCLUDEPATH += $$PWD/logic/cobs
INCLUDEPATH += $$PWD/logic/isotp
INCLUDEPATH += $$PWD/logic/peak
INCLUDEPATH += $$PWD/logic/uds
INCLUDEPATH += $$PWD/logic/uds/gen
INCLUDEPATH += $$PWD/logic/
INCLUDEPATH += $$PWD/tracertabform

HEADERS += \
    cantabform/canfdform.h \
    cantabform/canreplayform.h \
    cantabform/canstdform.h \
    cantabform/cantabform.h \
    logic/replaycan.h \
    logic/traceuds.h

win32 {
    HEADERS += \
        drivers/peak-win-V4.10.1.968/PCANBasic.h \
        drivers/peak-win-V4.10.1.968/stdafx.h
}

HEADERS += \
    logic/cmd/cmddef.h \
    logic/cmd/cmd.h

HEADERS += \
    logic/cobs/cobs.h

HEADERS += \
    logic/isotp/isotp_config.hpp \
    logic/isotp/isotp_defines.hpp \
    logic/isotp/isotp.hpp

HEADERS += \
    logic/peak/peakbasiccan.h \
    logic/peak/peakfdcan.h \
    logic/peak/peakstdcan.h

HEADERS += \
    logic/uds/uds.h \
    logic/uds/gen/uds_def.h

HEADERS += \
    logic/can.h \
    logic/config.h \
    logic/cli.h \
    logic/util.h

HEADERS += \
    tracertabform/tracertabform.h

HEADERS += \
    hexvalidator.h  \
    mainwindow.h

FORMS += \
    cantabform/canfdform.ui \
    cantabform/canreplayform.ui \
    cantabform/canstdform.ui \
    cantabform/cantabform.ui

FORMS += \
    tracertabform/tracertabform.ui \
    mainwindow.ui

LIBS += -lpcanbasic 
win32 {
    LIBS += -L$$PWD/drivers/peak-win-V4.10.1.968
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
