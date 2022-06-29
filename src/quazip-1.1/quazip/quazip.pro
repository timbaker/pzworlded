include($$top_srcdir/PZWorldEd.pri)
include($$top_srcdir/src/zlib/zlib.pri)

TEMPLATE = lib
TARGET = quazip
CONFIG += static
CONFIG -= app_bundle
#CONFIG -= qt

greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}

DEFINES += QUAZIP_BUILD QUAZIP_STATIC
#win32:DEFINES += ZLIB_DLL

target.path = $${LIBDIR}
INSTALLS += target

macx {
    DESTDIR = ../../bin/TileZed.app/Contents/Frameworks
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@executable_path/../Frameworks/
} else {
    DESTDIR = $$top_builddir/lib
}

DLLDESTDIR = $$top_builddir

SOURCES += \
    unzip.c \
    zip.c \
    JlCompress.cpp \
    qioapi.cpp \
    quaadler32.cpp \
    quachecksum32.cpp \
    quacrc32.cpp \
    quagzipfile.cpp \
    quaziodevice.cpp \
    quazip.cpp \
    quazipdir.cpp \
    quazipfile.cpp \
    quazipfileinfo.cpp \
    quazipnewinfo.cpp

HEADERS += \
    JlCompress.h \
    ioapi.h \
    minizip_crypt.h \
    quaadler32.h \
    quachecksum32.h \
    quacrc32.h \
    quagzipfile.h \
    quaziodevice.h \
    quazip.h \
    quazip_global.h \
    quazip_qt_compat.h \
    quazipdir.h \
    quazipfile.h \
    quazipfileinfo.h \
    quazipnewinfo.h \
    unzip.h \
    zip.h

