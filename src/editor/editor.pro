include(../../PZWorldEd.pri)
include(../libtiled/libtiled.pri)

QT       += core gui
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
    objectgroupsdialog.cpp

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
    objectgroupsdialog.h

FORMS    += mainwindow.ui \
    propertiesview.ui \
    propertiesdialog.ui \
    templatesdialog.ui \
    objecttypesdialog.ui \
    preferencesdialog.ui \
    objectgroupsdialog.ui

OTHER_FILES +=

RESOURCES += \
    editor.qrc

win32:INCLUDEPATH += .
