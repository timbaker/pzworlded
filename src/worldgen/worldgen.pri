INCLUDEPATH += $$PWD $$PWD/..
DEPENDPATH += $$PWD
LIBS += -L$$top_builddir/lib -lworldgen
PRE_TARGETDEPS += $$top_builddir/lib/worldgen.lib
