include(../../PZWorldEd.pri)
include(../libtiled/libtiled.pri)

TARGET = worldgen
TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle

DESTDIR = $$top_builddir/lib

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

HEADERS += \
    mainwindow.h \
    worldgenview.h

SOURCES += \
    mainwindow.cpp \
    worldgenview.cpp

FORMS += \
    mainwindow.ui

