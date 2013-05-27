include($$top_srcdir/PZWorldEd.pri)

TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle
CONFIG -= qt

target.path = $${LIBDIR}
INSTALLS += target

macx {
    DESTDIR = ../../bin/TileZed.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = ../../lib
}

DLLDESTDIR = ../..

#DEFINES += LUA_BUILD_AS_DLL

HEADERS += \
        poly2tri.h \
        common/shapes.h \
        common/utils.h \
        sweep/advancing_front.h \
        sweep/cdt.h \
        sweep/sweep_context.h

SOURCES += \
    common/shapes.cc \
    sweep/advancing_front.cc \
    sweep/cdt.cc \
    sweep/sweep.cc \
    sweep/sweep_context.cc

OTHER_FILES +=

