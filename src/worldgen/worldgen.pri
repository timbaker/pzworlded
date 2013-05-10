INCLUDEPATH += $$PWD $$PWD/..
DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -lworldgen
win32-msvc:PRE_TARGETDEPS += $$top_builddir/lib/worldgen.lib
win32-g++:PRE_TARGETDEPS += $$top_builddir/lib/libworldgen.a
