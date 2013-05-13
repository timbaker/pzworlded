INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -losm
win32-msvc*:PRE_TARGETDEPS += $$top_builddir/lib/osm.lib
win32-g++*:PRE_TARGETDEPS += $$top_builddir/lib/libosm.a

