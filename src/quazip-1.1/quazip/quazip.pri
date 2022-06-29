INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD
DEFINES += QUAZIP_STATIC
LIBS += -L$$top_builddir/lib -lquazip
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}
