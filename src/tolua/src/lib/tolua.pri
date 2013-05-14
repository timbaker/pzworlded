INCLUDEPATH += $$PWD/../../include
DEPENDPATH += $$PWD/../../include
LIBS += -L$$top_builddir/lib -ltolua
win32-msvc*:PRE_TARGETDEPS += $$top_builddir/lib/tolua.lib
*g++*:PRE_TARGETDEPS += $$top_builddir/lib/libtolua.a

