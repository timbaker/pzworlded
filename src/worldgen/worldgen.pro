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
    worldgenwindow.h \
    worldgenview.h

SOURCES += \
    worldgenwindow.cpp \
    worldgenview.cpp

FORMS += \
    worldgenwindow.ui

