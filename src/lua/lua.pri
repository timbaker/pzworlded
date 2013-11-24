INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src
contains(CONFIG,Debug):DEFINES += LUA_DEBUG
LIBS += -L$$top_builddir/lib -llua
