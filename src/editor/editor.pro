include(../../PZWorldEd.pri)
include(../libtiled/libtiled.pri)
include(../lua/lua.pri)
include(../worldgen/worldgen.pri)
include(../osm/osm.pri)
include(../qtlockedfile/qtlockedfile.pri)

QT       += core gui xml
contains(QT_CONFIG, opengl): QT += opengl

TARGET = PZWorldEd
TEMPLATE = app
target.path = $${PREFIX}/bin
INSTALLS += target
win32 {
    DESTDIR = ../..
} else {
    DESTDIR = ../../bin
}

DEFINES += QT_NO_CAST_FROM_ASCII \
    QT_NO_CAST_TO_ASCII

macx {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../bin/PZWorldEd.app/Contents/Frameworks
    LIBS += -framework Foundation
} else:win32 {
    LIBS += -L$$OUT_PWD/../../lib
} else {
    QMAKE_LIBDIR_FLAGS += -L$$OUT_PWD/../../lib
}

# Make sure the Tiled executable can find libtiled
!win32:!macx {
    QMAKE_RPATHDIR += \$\$ORIGIN/../lib

    # It is not possible to use ORIGIN in QMAKE_RPATHDIR, so a bit manually
    QMAKE_LFLAGS += -Wl,-z,origin \'-Wl,-rpath,$$join(QMAKE_RPATHDIR, ":")\'
    QMAKE_RPATHDIR =
}

MOC_DIR = .moc
UI_DIR = .uic
RCC_DIR = .rcc
OBJECTS_DIR = .obj

SOURCES += main.cpp\
        mainwindow.cpp \
    worldview.cpp \
    worldscene.cpp \
    world.cpp \
    worlddocument.cpp \
    worldcell.cpp \
    cellview.cpp \
    cellscene.cpp \
    document.cpp \
    documentmanager.cpp \
    celldocument.cpp \
    mapcomposite.cpp \
    mapsdock.cpp \
    preferences.cpp \
    mapimagemanager.cpp \
    undoredo.cpp \
    undodock.cpp \
    mapmanager.cpp \
    basegraphicsview.cpp \
    progress.cpp \
    zoomable.cpp \
    scenetools.cpp \
    worldwriter.cpp \
    worldreader.cpp \
    propertiesdock.cpp \
    propertydefinitionsdialog.cpp \
    templatesdialog.cpp \
    basegraphicsscene.cpp \
    lotsdock.cpp \
    worldcellobject.cpp \
    objectsdock.cpp \
    toolmanager.cpp \
    properties.cpp \
    objecttypesdialog.cpp \
    luatablewriter.cpp \
    luawriter.cpp \
    layersmodel.cpp \
    layersdock.cpp \
    preferencesdialog.cpp \
    objectgroupsdialog.cpp \
    colorbutton.cpp \
    copypastedialog.cpp \
    clipboard.cpp \
    lotfilesmanager.cpp \
    road.cpp \
    roadsdock.cpp \
    simplefile.cpp \
    bmptotmx.cpp \
    bmptotmxdialog.cpp \
    generatelotsdialog.cpp \
    filesystemwatcher.cpp \
    bmptotmxconfirmdialog.cpp \
    resizeworlddialog.cpp \
    newworlddialog.cpp \
    tilemetainfomgr.cpp \
    tilesetmanager.cpp \
    BuildingEditor/furnituregroups.cpp \
    BuildingEditor/buildingtmx.cpp \
    BuildingEditor/buildingtiles.cpp \
    BuildingEditor/buildingobjects.cpp \
    BuildingEditor/buildingmap.cpp \
    BuildingEditor/buildingfloor.cpp \
    BuildingEditor/building.cpp \
    BuildingEditor/buildingwriter.cpp \
    BuildingEditor/buildingreader.cpp \
    BuildingEditor/buildingtemplates.cpp \
    threads.cpp \
    bmpblender.cpp \
    lotpackwindow.cpp \
    chunkmap.cpp \
    fromtodialog.cpp \
    unknowncolorsdialog.cpp \
    path.cpp \
    luaworlded.cpp \
    luatiled.cpp \
    luaconsole.cpp \
    tilepainter.cpp \
    worldlookup.cpp \
    pathdocument.cpp \
    basepathscene.cpp \
    orthopathscene.cpp \
    basepathrenderer.cpp \
    isopathscene.cpp \
    pathview.cpp \
    tilepathscene.cpp \
    worldchunkmap.cpp \
    shadowworld.cpp \
    pathworld.cpp \
    pathtools.cpp

HEADERS  += mainwindow.h \
    worldview.h \
    worldscene.h \
    world.h \
    worlddocument.h \
    worldcell.h \
    cellview.h \
    cellscene.h \
    document.h \
    documentmanager.h \
    celldocument.h \
    mapcomposite.h \
    mapsdock.h \
    preferences.h \
    mapimagemanager.h \
    undoredo.h \
    undodock.h \
    mapmanager.h \
    basegraphicsview.h \
    progress.h \
    zoomable.h \
    scenetools.h \
    worldwriter.h \
    worldreader.h \
    propertiesdock.h \
    propertydefinitionsdialog.h \
    templatesdialog.h \
    basegraphicsscene.h \
    lotsdock.h \
    objectsdock.h \
    toolmanager.h \
    properties.h \
    objecttypesdialog.h \
    luatablewriter.h \
    luawriter.h \
    layersmodel.h \
    layersdock.h \
    preferencesdialog.h \
    objectgroupsdialog.h \
    colorbutton.h \
    copypastedialog.h \
    clipboard.h \
    lotfilesmanager.h \
    road.h \
    roadsdock.h \
    simplefile.h \
    bmptotmx.h \
    bmptotmxdialog.h \
    generatelotsdialog.h \
    filesystemwatcher.h \
    bmptotmxconfirmdialog.h \
    resizeworlddialog.h \
    newworlddialog.h \
    tilemetainfomgr.h \
    tilesetmanager.h \
    BuildingEditor/furnituregroups.h \
    BuildingEditor/buildingtmx.h \
    BuildingEditor/buildingtiles.h \
    BuildingEditor/buildingobjects.h \
    BuildingEditor/buildingmap.h \
    BuildingEditor/buildingfloor.h \
    BuildingEditor/building.h \
    BuildingEditor/buildingwriter.h \
    BuildingEditor/buildingreader.h \
    BuildingEditor/buildingtemplates.h \
    threads.h \
    bmpblender.h \
    lotpackwindow.h \
    chunkmap.h \
    fromtodialog.h \
    unknowncolorsdialog.h \
    path.h \
    luaworlded.h \
    luaconsole.h \
    luatiled.h \
    tilepainter.h \
    worldlookup.h \
    pathdocument.h \
    basepathscene.h \
    orthopathscene.h \
    basepathrenderer.h \
    isopathscene.h \
    pathview.h \
    tilepathscene.h \
    worldchunkmap.h \
    shadowworld.h \
    pathworld.h \
    pathtools.h \
    global.h

FORMS    += mainwindow.ui \
    propertiesview.ui \
    propertiesdialog.ui \
    templatesdialog.ui \
    objecttypesdialog.ui \
    preferencesdialog.ui \
    objectgroupsdialog.ui \
    copypastedialog.ui \
    bmptotmxdialog.ui \
    generatelotsdialog.ui \
    bmptotmxconfirmdialog.ui \
    resizeworlddialog.ui \
    newworlddialog.ui \
    lotpackwindow.ui \
    fromtodialog.ui \
    unknowncolorsdialog.ui \
    luaconsole.ui

OTHER_FILES += luatiled.pkg

RESOURCES += \
    editor.qrc

win32 {
    RC_FILE = worlded.rc
}

win32:INCLUDEPATH += .


include(../tolua/src/lib/tolua.pri)
TOLUA_PKGNAME = tiled
TOLUA_PKG = luatiled.pkg
TOLUA_DEPS = $$PWD/luatiled.h
include(../tolua/src/bin/tolua.pri)

#TOLUA_PKGNAME = worlded
#TOLUA_PKG = luaworlded.pkg
#TOLUA_DEPS = $$PWD/luaworlded.h
#include(../tolua/src/bin/tolua.pri)
