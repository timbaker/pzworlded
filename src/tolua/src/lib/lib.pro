include($$top_srcdir/PZWorldEd.pri)
include($$top_srcdir/src/lua/lua.pri)

TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt

TARGET = tolua

DESTDIR = $$top_builddir/lib

INCLUDEPATH += \
    ../../include

HEADERS += \
    tolua_event.h \
    ../../include/tolua.h

SOURCES += \
    tolua_to.c \
    tolua_push.c \
    tolua_map.c \
    tolua_is.c \
    tolua_event.c
