INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src
contains(CONFIG,Debug):DEFINES += LUA_DEBUG
LIBS += -L$$top_builddir/lib -llua

linux {
DEFINES += LUA_USE_LINUX
LIBS += -ldl
}
