# http://qt-project.org/wiki/QMake-top-level-srcdir-and-builddir

HACK_INITVARS_PWD_QT5=$$PWD
TEMPLATE=subdirs
SUBDIRS= # don't build anything, we're just generating the .qmake.cache file
QMAKE_SUBSTITUTES += .qmake.cache.in

