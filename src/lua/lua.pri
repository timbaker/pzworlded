INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src
LIBS += -L$$top_builddir/lib -llua
win32-msvc*:PRE_TARGETDEPS += $$top_builddir/lib/lua.lib
*g++*:PRE_TARGETDEPS += $$top_builddir/lib/liblua.a

