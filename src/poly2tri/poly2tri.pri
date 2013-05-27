INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -lpoly2tri
win32-msvc*:PRE_TARGETDEPS += $$top_builddir/lib/poly2tri.lib
*g++*:PRE_TARGETDEPS += $$top_builddir/lib/libpoly2tri.a

