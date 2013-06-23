include(../../PZWorldEd.pri)
include(../qtlockedfile/qtlockedfile.pri)

TEMPLATE = lib
TARGET = tiled
target.path = $${LIBDIR}
INSTALLS += target
macx {
    DESTDIR = ../../bin/PZWorldEd.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = ../../lib
}
DLLDESTDIR = ../..

win32 {
    # With Qt 4 it was enough to include zlib, since the symbols were available
    # in Qt. Qt 5 no longer exposes zlib symbols, so it needs to be linked.
    # Get the installer at:
    #
    # http://gnuwin32.sourceforge.net/packages/zlib.htm
    #
    greaterThan(QT_MAJOR_VERSION, 4) {
        # If there is an environment variable, take that
        isEmpty(ZLIB_PATH):ZLIB_PATH = "$$PWD/../zlib"

        isEmpty(ZLIB_PATH) {
            error("ZLIB_PATH not defined and could not be auto-detected")
        }

        INCLUDEPATH += $${ZLIB_PATH}
        win32-g++*:LIBS += -L$${ZLIB_PATH} -lz
        win32-msvc*:LIBS += -L$${ZLIB_PATH} zlib.lib
    } else {
        INCLUDEPATH += ../zlib
    }
} else {
    # On other platforms it is necessary to link to zlib explicitly
    LIBS += -lz
}

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII
DEFINES += TILED_LIBRARY

contains(QT_CONFIG, reduce_exports): CONFIG += hide_symbols
OBJECTS_DIR = .obj
SOURCES += compression.cpp \
    imagelayer.cpp \
    isometricrenderer.cpp \
    layer.cpp \
    map.cpp \
    mapobject.cpp \
    mapreader.cpp \
    maprenderer.cpp \
    mapwriter.cpp \
    objectgroup.cpp \
    orthogonalrenderer.cpp \
    properties.cpp \
    staggeredrenderer.cpp \
    tilelayer.cpp \
    tileset.cpp \
    gidmapper.cpp \
    zlevelrenderer.cpp \
    ztilelayergroup.cpp
HEADERS += compression.h \
    imagelayer.h \
    isometricrenderer.h \
    layer.h \
    map.h \
    mapobject.h \
    mapreader.h \
    maprenderer.h \
    mapwriter.h \
    object.h \
    objectgroup.h \
    orthogonalrenderer.h \
    properties.h \
    staggeredrenderer.h \
    tile.h \
    tiled_global.h \
    tilelayer.h \
    tileset.h \
    gidmapper.h \
    zlevelrenderer.h \
    ztilelayergroup.h
macx {
    contains(QT_CONFIG, ppc):CONFIG += x86 \
        ppc
}
