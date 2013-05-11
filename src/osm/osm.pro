include(../../PZWorldEd.pri)

TARGET = osm
TEMPLATE = lib
CONFIG += static
CONFIG -= app_bundle

DESTDIR = $$top_builddir/lib

DEFINES += \
    QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII \
    NOGUI NOPBF

SOURCES += \
    plugins/osmimporter/types.cpp \
    plugins/osmimporter/osmimporter.cpp \
    plugins/osmrenderer/rendererbase.cpp \
    osmfile.cpp \
    osmlookup.cpp

HEADERS += \
    plugins/osmimporter/xmlreader.h \
    plugins/osmimporter/types.h \
    plugins/osmimporter/statickdtree.h \
    plugins/osmimporter/osmimporter.h \
    plugins/osmimporter/ientityreader.h \
    plugins/osmimporter/bz2input.h \
    osmfile.h \
    osmlookup.h

RESOURCES += \
    plugins/osmimporter/speedProfiles.qrc

##############################

INCLUDEPATH += plugins/osmimporter

SOURCES += \
    plugins/osmrenderer/qtilerenderer.cpp \
    plugins/osmrenderer/qtilerendererclient.cpp \
    plugins/osmrenderer/tile-write.cpp

HEADERS += \
    plugins/osmrenderer/qtilerenderer.h \
    plugins/osmrenderer/qtilerendererclient.h \
    plugins/osmrenderer/tile-write.h \
    plugins/osmrenderer/types.h \
    plugins/osmrenderer/quadtile.h \
    plugins/osmrenderer/img_writer.h

RESOURCES += \
    plugins/osmrenderer/rendering_rules.qrc
