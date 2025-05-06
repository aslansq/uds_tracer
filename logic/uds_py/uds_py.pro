QT -= gui
QT += xml
QT += xmlpatterns
CONFIG += c++11 console
CONFIG -= app_bundle

message("Using Qt version $$[QT_VERSION]")

!equals(QT_MAJOR_VERSION, 5) {
    error("Qt version 5 is not detected. Exiting qmake.")
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        output/uds_def.cpp \
	output/main.cpp

INCLUDEPATH += $$PWD/output

HEADERS += \
        output/uds_def.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
