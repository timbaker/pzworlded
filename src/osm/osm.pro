include(../../PZWorldEd.pri)

INCLUDEPATH += ../editor

TARGET = osm
TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle

DESTDIR = $$top_builddir/lib

DEFINES += \
    QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

SOURCES += \
    osmfile.cpp \
    osmlookup.cpp \
    osmrulefile.cpp

HEADERS += \
    coordinates.h \
    osmfile.h \
    osmlookup.h \
    osmrulefile.h

