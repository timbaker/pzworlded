include($$top_srcdir/tiled.pri)

TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt

#target.path = $${LIBDIR}
#INSTALLS += target

macx {
    DESTDIR = ../../bin/TileZed.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = ../../lib
}

DLLDESTDIR = ../..

contains(CONFIG,Debug):DEFINES += LUA_DEBUG
#DEFINES += LUA_BUILD_AS_DLL

SOURCES += \
    src/lzio.c \
    src/lvm.c \
    src/lundump.c \
    src/luac.c \
    src/ltm.c \
    src/ltablib.c \
    src/ltable.c \
    src/lstrlib.c \
    src/lstring.c \
    src/lstate.c \
    src/lparser.c \
    src/loslib.c \
    src/lopcodes.c \
    src/lobject.c \
    src/loadlib.c \
    src/lmem.c \
    src/lmathlib.c \
    src/llex.c \
    src/liolib.c \
    src/linit.c \
    src/lgc.c \
    src/lfunc.c \
    src/ldump.c \
    src/ldo.c \
    src/ldebug.c \
    src/ldblib.c \
    src/lctype.c \
    src/lcorolib.c \
    src/lcode.c \
    src/lbitlib.c \
    src/lbaselib.c \
    src/lauxlib.c \
    src/lapi.c

OTHER_FILES += \
    src/lua52.def \
    src/lua_dll.rc

HEADERS += \
    src/lzio.h \
    src/lvm.h \
    src/lundump.h \
    src/lualib.h \
    src/luaconf.h \
    src/lua.h \
    src/ltm.h \
    src/ltable.h \
    src/lstring.h \
    src/lstate.h \
    src/lparser.h \
    src/lopcodes.h \
    src/lobject.h \
    src/lmem.h \
    src/llimits.h \
    src/llex.h \
    src/lgc.h \
    src/lfunc.h \
    src/ldo.h \
    src/ldebug.h \
    src/lctype.h \
    src/lcode.h \
    src/lauxlib.h \
    src/lapi.h

