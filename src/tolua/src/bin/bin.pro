include($$top_srcdir/PZWorldEd.pri)
include(../lib/tolua.pri)
include($$top_srcdir/src/lua/lua.pri)

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

TARGET = tolua

win32 {
    DESTDIR = $$top_builddir
} else {
    DESTDIR = $$top_builddir/bin
}

macx {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../../../bin/TileZed.app/Contents/Frameworks
    LIBS += -framework Foundation
} else:win32 {
    LIBS += -L$$top_builddir/lib
} else {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../../../lib
}

DEFINES += LUA_SOURCE LUA_SOURCE_PATH=\\\"$$PWD/lua/\\\"

SOURCES += \
  tolua.c \
  toluabind.c

INCLUDEPATH += \
    ../../include

#tolua.name = Generate toluabind.c
#tolua.input = tolua.pkg
#tolua.output = toluabind.c
#tolua.commands = tolua -o toluabind.c tolua.pkg
#tolua.CONFIG += no_link
#QMAKE_EXTRA_COMPILERS += tolua

OTHER_FILES += \
    lua/verbatim.lua \
    lua/variable.lua \
    lua/typedef.lua \
    lua/package.lua \
    lua/operator.lua \
    lua/namespace.lua \
    lua/module.lua \
    lua/feature.lua \
    lua/enumerate.lua \
    lua/doit.lua \
    lua/define.lua \
    lua/declaration.lua \
    lua/container.lua \
    lua/compat.lua \
    lua/code.lua \
    lua/clean.lua \
    lua/class.lua \
    lua/basic.lua \
    lua/array.lua \
    lua/all.lua \
    lua/function.lua

HEADERS += \
    lua/function.lua
