/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "preferences.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

Preferences *Preferences::mInstance = 0;

Preferences *Preferences::instance()
{
    if (!mInstance)
        mInstance = new Preferences;
    return mInstance;
}

void Preferences::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

bool Preferences::snapToGrid() const
{
    return mSnapToGrid;
}

bool Preferences::showCoordinates() const
{
    return mShowCoordinates;
}

bool Preferences::showWorldGrid() const
{
    return mShowWorldGrid;
}

bool Preferences::showCellGrid() const
{
    return mShowCellGrid;
}

bool Preferences::showMiniMap() const
{
    return mShowMiniMap;
}

int Preferences::miniMapWidth() const
{
    return mMiniMapWidth;
}

bool Preferences::highlightCurrentLevel() const
{
    return mHighlightCurrentLevel;
}

Preferences::Preferences()
    : QObject()
    , mSettings(new QSettings)
{
    // Retrieve interface settings
    mSettings->beginGroup(QLatin1String("Interface"));
    mSnapToGrid = mSettings->value(QLatin1String("SnapToGrid"), true).toBool();
    mShowCoordinates = mSettings->value(QLatin1String("ShowCoordinates"), true).toBool();
    mShowWorldGrid = mSettings->value(QLatin1String("ShowWorldGrid"), true).toBool();
    mShowCellGrid = mSettings->value(QLatin1String("ShowCellGrid"), false).toBool();
    mGridColor = QColor(mSettings->value(QLatin1String("GridColor"),
                                         QColor(Qt::black).name()).toString());
    mShowObjectNames = mSettings->value(QLatin1String("ShowObjectNames"), true).toBool();
    mShowBMPs = mSettings->value(QLatin1String("ShowBMPs"), true).toBool();
    mShowMiniMap = mSettings->value(QLatin1String("ShowMiniMap"), true).toBool();
    mMiniMapWidth = mSettings->value(QLatin1String("MiniMapWidth"), 256).toInt();
    mHighlightCurrentLevel = mSettings->value(QLatin1String("HighlightCurrentLevel"),
                                              false).toBool();
    mHighlightRoomUnderPointer = mSettings->value(QLatin1String("HighlightRoomUnderPointer"),
                                                  false).toBool();
    mUseOpenGL = mSettings->value(QLatin1String("OpenGL"), false).toBool();
    mWorldThumbnails = mSettings->value(QLatin1String("WorldThumbnails"), false).toBool();
    mShowAdjacentMaps = mSettings->value(QLatin1String("ShowAdjacentMaps"), true).toBool();
    QString hmStyle = mSettings->value(QLatin1String("HeightMapDisplayStyle"),
                                       QLatin1String("mesh")).toString();
    mHeightMapDisplayStyle = (hmStyle == QLatin1String("mesh")) ? 0 : 1;
    mSettings->endGroup();

    mSettings->beginGroup(QLatin1String("MapsDirectory"));
    mMapsDirectory = mSettings->value(QLatin1String("Current"), QString()).toString();
    mSettings->endGroup();

    // Set the default location of the Tiles Directory to the same value set
    // in TileZed's Tilesets Dialog.
    QSettings settings(QLatin1String("mapeditor.org"), QLatin1String("Tiled"));
    QString KEY_TILES_DIR = QLatin1String("Tilesets/TilesDirectory");
    QString tilesDirectory = settings.value(KEY_TILES_DIR).toString();

    if (tilesDirectory.isEmpty() || !QDir(tilesDirectory).exists()) {
        tilesDirectory = QCoreApplication::applicationDirPath() +
                QLatin1Char('/') + QLatin1String("../Tiles");
        if (!QDir(tilesDirectory).exists())
            tilesDirectory = QCoreApplication::applicationDirPath() +
                    QLatin1Char('/') + QLatin1String("../../Tiles");
    }
    if (tilesDirectory.length())
        tilesDirectory = QDir::cleanPath(tilesDirectory);
    if (!QDir(tilesDirectory).exists())
        tilesDirectory.clear();
    mTilesDirectory = mSettings->value(QLatin1String("TilesDirectory"),
                                       tilesDirectory).toString();
    mTiles2xDirectory = mSettings->value(QLatin1String("Tiles2xDirectory")).toString();

    mOpenFileDirectory = mSettings->value(QLatin1String("OpenFileDirectory")).toString();

    // Use the same directory as TileZed.
    QString KEY_CONFIG_PATH = QLatin1String("ConfigDirectory");
    QString configPath = settings.value(KEY_CONFIG_PATH).toString();
    if (configPath.isEmpty())
        configPath = QDir::homePath() + QLatin1Char('/') + QLatin1String(".TileZed");
    mConfigDirectory = configPath;

    // Use the 'use virtual tilesets' setting from TileZed.
    mUseVirtualTilesets = settings.value(QLatin1String("Interface/UseVirtualTilesets"), false).toBool();
}

Preferences::~Preferences()
{
    delete mSettings;
}

QString Preferences::configPath() const
{
    return mConfigDirectory;
}

QString Preferences::configPath(const QString &fileName) const
{
    return configPath() + QLatin1Char('/') + fileName;
}

QString Preferences::appConfigPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/config");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Config");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::appConfigPath(const QString &fileName) const
{
    return appConfigPath() + QLatin1Char('/') + fileName;
}

QString Preferences::docsPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QLatin1String("/docs");
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/docs");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Docs");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::docsPath(const QString &fileName) const
{
    return docsPath() + QLatin1Char('/') + fileName;
}

QString Preferences::luaPath() const
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + QLatin1String("/lua");
#elif defined(Q_OS_UNIX)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../share/tilezed/lua");
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath() + QLatin1String("/../Lua");
#else
#error "wtf system is this???"
#endif
}

QString Preferences::luaPath(const QString &fileName) const
{
    return luaPath() + QLatin1Char('/') + fileName;
}

QString Preferences::mapsDirectory() const
{
    return mMapsDirectory;
}

void Preferences::setSnapToGrid(bool snapToGrid)
{
    if (snapToGrid == mSnapToGrid)
        return;

    mSnapToGrid = snapToGrid;
    mSettings->setValue(QLatin1String("Interface/SnapToGrid"), mSnapToGrid);
    emit snapToGridChanged(mSnapToGrid);
}

void Preferences::setShowCoordinates(bool showCoords)
{
    if (showCoords == mShowCoordinates)
        return;

    mShowCoordinates = showCoords;
    mSettings->setValue(QLatin1String("Interface/ShowCoordinates"), mShowCoordinates);
    emit showCoordinatesChanged(mShowCoordinates);
}

void Preferences::setShowWorldGrid(bool showGrid)
{
    if (showGrid == mShowWorldGrid)
        return;

    mShowWorldGrid = showGrid;
    mSettings->setValue(QLatin1String("Interface/ShowWorldGrid"), mShowWorldGrid);
    emit showWorldGridChanged(mShowWorldGrid);
}

void Preferences::setShowCellGrid(bool showGrid)
{
    if (showGrid == mShowCellGrid)
        return;

    mShowCellGrid = showGrid;
    mSettings->setValue(QLatin1String("Interface/ShowCellGrid"), mShowCellGrid);
    emit showCellGridChanged(mShowCellGrid);
}

void Preferences::setGridColor(const QColor &gridColor)
{
    if (mGridColor == gridColor)
        return;

    mGridColor = gridColor;
    mSettings->setValue(QLatin1String("Interface/GridColor"), mGridColor.name());
    emit gridColorChanged(mGridColor);
}

void Preferences::setUseOpenGL(bool useOpenGL)
{
    if (mUseOpenGL == useOpenGL)
        return;

    mUseOpenGL = useOpenGL;
    mSettings->setValue(QLatin1String("Interface/OpenGL"), mUseOpenGL);

    emit useOpenGLChanged(mUseOpenGL);
}

void Preferences::setWorldThumbnails(bool thumbs)
{
    if (mWorldThumbnails == thumbs)
        return;

    mWorldThumbnails = thumbs;
    mSettings->setValue(QLatin1String("Interface/WorldThumbnails"), mWorldThumbnails);

    emit worldThumbnailsChanged(mWorldThumbnails);
}

QString Preferences::openFileDirectory() const
{
    return mOpenFileDirectory;
}

void Preferences::setOpenFileDirectory(const QString &path)
{
    if (mOpenFileDirectory == path)
        return;
    mOpenFileDirectory = path;
    mSettings->setValue(QLatin1String("OpenFileDirectory"), mOpenFileDirectory);
}

void Preferences::setShowAdjacentMaps(bool show)
{
    if (mShowAdjacentMaps == show)
        return;

    mShowAdjacentMaps = show;
    mSettings->setValue(QLatin1String("Interface/ShowAdjacentMaps"), mShowAdjacentMaps);

    emit showAdjacentMapsChanged(mShowAdjacentMaps);
}

void Preferences::setShowObjectNames(bool show)
{
    if (mShowObjectNames == show)
        return;

    mShowObjectNames = show;
    mSettings->setValue(QLatin1String("Interface/ShowObjectNames"), mShowObjectNames);

    emit showObjectNamesChanged(mShowObjectNames);
}

void Preferences::setShowBMPs(bool show)
{
    if (mShowBMPs == show)
        return;

    mShowBMPs = show;
    mSettings->setValue(QLatin1String("Interface/ShowBMPs"), mShowBMPs);

    emit showBMPsChanged(mShowBMPs);
}

void Preferences::setShowMiniMap(bool show)
{
    if (show == mShowMiniMap)
        return;

    mShowMiniMap = show;
    mSettings->setValue(QLatin1String("Interface/ShowMiniMap"), mShowMiniMap);
    emit showMiniMapChanged(mShowMiniMap);
}

void Preferences::setMiniMapWidth(int width)
{
    width = qMin(width, MINIMAP_WIDTH_MAX);
    width = qMax(width, MINIMAP_WIDTH_MIN);

    if (mMiniMapWidth == width)
        return;
    mMiniMapWidth = width;
    mSettings->setValue(QLatin1String("Interface/MiniMapWidth"), width);
    emit miniMapWidthChanged(mMiniMapWidth);
}

void Preferences::setHighlightCurrentLevel(bool highlight)
{
    if (highlight == mHighlightCurrentLevel)
        return;

    mHighlightCurrentLevel = highlight;
    mSettings->setValue(QLatin1String("Interface/HighlightCurrentLevel"), mHighlightCurrentLevel);
    emit highlightCurrentLevelChanged(mHighlightCurrentLevel);
}

void Preferences::setHighlightRoomUnderPointer(bool highlight)
{
    if (highlight == mHighlightRoomUnderPointer)
        return;
    mHighlightRoomUnderPointer = highlight;
    mSettings->setValue(QLatin1String("Interface/HighlightRoomUnderPointer"),
                        mHighlightRoomUnderPointer);
    emit highlightRoomUnderPointerChanged(mHighlightRoomUnderPointer);
}

void Preferences::setHeightMapDisplayStyle(int style)
{
    if (style == mHeightMapDisplayStyle)
        return;

    mHeightMapDisplayStyle = style;
    mSettings->setValue(QLatin1String("Interface/HeightMapDisplayStyle"),
                        QLatin1String(style ? "flat" : "mesh"));
    emit heightMapDisplayStyleChanged(mHeightMapDisplayStyle);
}


void Preferences::setMapsDirectory(const QString &path)
{
    if (mMapsDirectory == path)
        return;
    mMapsDirectory = path;
    mSettings->setValue(QLatin1String("MapsDirectory/Current"), path);

    // Put this up, otherwise the progress dialog shows and hides for each lot.
    // Since each open document has its own ZLotManager, this shows and hides for each document as well.
//    ZProgressManager::instance()->begin(QLatin1String("Checking lots..."));

    emit mapsDirectoryChanged();
}

QString Preferences::tilesDirectory() const
{
    return mTilesDirectory;
}

void Preferences::setTilesDirectory(const QString &path)
{
    if (mTilesDirectory == path)
        return;
    mTilesDirectory = path;
    mSettings->setValue(QLatin1String("TilesDirectory"), path);
    emit tilesDirectoryChanged();
}

QString Preferences::tiles2xDirectory() const
{
    return mTiles2xDirectory;
}

void Preferences::setTiles2xDirectory(const QString &path)
{
    mTiles2xDirectory = path;
    mSettings->setValue(QLatin1String("Tiles2xDirectory"), mTiles2xDirectory);
    emit tilesDirectoryChanged();
}

QString Preferences::texturesDirectory() const
{
    return QDir(mTilesDirectory).filePath(QLatin1String("Textures"));
}
