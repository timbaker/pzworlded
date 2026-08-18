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

#include <QApplication>
#include <QDir>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTextStream>

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

QString Preferences::appDirPath() const
{
    return mAppDirPath;
}

QString Preferences::initAppDirPath() {
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_MAC)
    return QCoreApplication::applicationDirPath();
#elif defined(Q_OS_UNIX)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("APPIMAGE"))) {
        qDebug() << "Running as a compressed AppImage file!";
        qDebug() << "Original AppImage location:" << env.value(QStringLiteral("APPIMAGE"));
        qDebug() << "Virtual Mount folder:" << env.value(QStringLiteral("APPDIR"));
        mLinuxAppImage = true;
        const QString appImage = env.value(QStringLiteral("APPIMAGE"));
        return QFileInfo(appImage).absolutePath();
    }
    qDebug() << "Running as a standard unpacked binary or local AppDir.";
    // ../src is where the .o files etc are built, not the source-code directory
    mInBuildDirectory = QFileInfo::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/../src"));
    return QCoreApplication::applicationDirPath();
#else
#error "wtf system is this???"
#endif
}

QString Preferences::initShareDirPath()
{
#if defined(Q_OS_UNIX)
    if (mLinuxAppImage) {
        return appDirPath() + QStringLiteral("/../TileZed/share/tilezed");
    }
    if (mInBuildDirectory) {
        return appDirPath() + QStringLiteral("/../share/tilezed");
    }
    return appDirPath() + QStringLiteral("/../../TileZed/share/tilezed");
#else
    return QString();
#endif
}

bool Preferences::snapToGrid() const
{
    return mSnapToGrid;
}

bool Preferences::showCellBorder() const
{
    return mShowCellBorder;
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
    mAppDirPath = initAppDirPath();
    mShareDirPath = initShareDirPath();

    // Retrieve interface settings
    mSettings->beginGroup(QStringLiteral("Interface"));
    mSnapToGrid = mSettings->value(QStringLiteral("SnapToGrid"), true).toBool();
    mShowCellBorder = mSettings->value(QStringLiteral("ShowCellBorder"), true).toBool();
    mShowCoordinates = mSettings->value(QStringLiteral("ShowCoordinates"), true).toBool();
    mShowWorldGrid = mSettings->value(QStringLiteral("ShowWorldGrid"), true).toBool();
    mShowCellGrid = mSettings->value(QStringLiteral("ShowCellGrid"), false).toBool();
    mGridColor = QColor(mSettings->value(QStringLiteral("GridColor"),
                                         QColor(Qt::black).name()).toString());
    mShowObjects = mSettings->value(QStringLiteral("ShowObjects"), true).toBool();
    mShowObjectNames = mSettings->value(QStringLiteral("ShowObjectNames"), true).toBool();
    mShowBMPs = mSettings->value(QStringLiteral("ShowBMPs"), true).toBool();
    mShowMiniMap = mSettings->value(QStringLiteral("ShowMiniMap"), true).toBool();
    mShowZombieSpawnImage = mSettings->value(QStringLiteral("ShowZombieSpawnImage"), false).toBool();
    mZombieSpawnImageOpacity = mSettings->value(QStringLiteral("ZombieSpawnImageOpacity"), 0.8).toReal();
    mShowZonesInWorldView = mSettings->value(QStringLiteral("ShowZonesInWorldView"), false).toBool();
    mMiniMapWidth = mSettings->value(QStringLiteral("MiniMapWidth"), 256).toInt();
    mHighlightCurrentLevel = mSettings->value(QStringLiteral("HighlightCurrentLevel"),
                                              false).toBool();
    mHighlightRoomUnderPointer = mSettings->value(QStringLiteral("HighlightRoomUnderPointer"),
                                                  false).toBool();
    mHighlightUnlitRooms = mSettings->value(QStringLiteral("HighlightUnlitRooms"), false).toBool();
    mShowLotFloorsOnly = mSettings->value(QStringLiteral("ShowLotFloorsOnly"), false).toBool();
    mShowOtherWorlds = mSettings->value(QStringLiteral("ShowOtherWorlds"), true).toBool();
    mUseOpenGL = mSettings->value(QStringLiteral("OpenGL"), false).toBool();
    mLoadAllWorldThumbnails = mSettings->value(QStringLiteral("LoadAllWorldThumbnails"), false).toBool();
    mShowWorldThumbnails = mSettings->value(QStringLiteral("ShowWorldThumbnails"), true).toBool();
    mShowAdjacentMaps = mSettings->value(QStringLiteral("ShowAdjacentMaps"), true).toBool();
    mShowInvisibleTiles = mSettings->value(QStringLiteral("ShowInvisibleTiles"), true).toBool();
    mTheme = mSettings->value(QStringLiteral("Theme"), QStringLiteral("Default")).toString();
    mSettings->endGroup();

    mSettings->beginGroup(QStringLiteral("MapsDirectory"));
    mMapsDirectory = mSettings->value(QStringLiteral("Current"), QString()).toString();
    mSettings->endGroup();

    // Set the default location of the Tiles Directory to the same value set
    // in TileZed's Tilesets Dialog.
    QSettings settings(QStringLiteral("TheIndieStone"), QStringLiteral("TileZed"));
    QString KEY_TILES_DIR = QStringLiteral("Tilesets/TilesDirectory");
    QString tilesDirectory = settings.value(KEY_TILES_DIR).toString();

    if (tilesDirectory.isEmpty() || !QDir(tilesDirectory).exists()) {
        tilesDirectory = appDirPath() + QStringLiteral("/../Tiles");
        if (!QDir(tilesDirectory).exists())
            tilesDirectory = appDirPath() + QStringLiteral("/../../Tiles");
    }
    if (tilesDirectory.length())
        tilesDirectory = QDir::cleanPath(tilesDirectory);
    if (!QDir(tilesDirectory).exists())
        tilesDirectory.clear();
    mTilesDirectory = mSettings->value(QStringLiteral("TilesDirectory"),
                                       tilesDirectory).toString();

    // Use the same .tiles files as TileZed
    mTilePropertiesFiles = settings.value(QStringLiteral("TilePropertiesFiles")).toStringList();

    mOpenFileDirectory = mSettings->value(QStringLiteral("OpenFileDirectory")).toString();
    mWorldMapXMLFile = mSettings->value(QStringLiteral("WorldMapXMLFile")).toString();

    // Use the same directory as TileZed.
    QString KEY_CONFIG_PATH = QStringLiteral("ConfigDirectory");
    QString configPath = settings.value(KEY_CONFIG_PATH).toString();
    if (configPath.isEmpty())
        configPath = QDir::homePath() + QLatin1Char('/') + QStringLiteral(".TileZed");
    mConfigDirectory = configPath;

    // Use the same directory as TileZed.
    mThumbnailsDirectory = settings.value(QStringLiteral("Thumbnails/Directory")).toString();
}

Preferences::~Preferences()
{
    delete mSettings;
}

QString Preferences::userPath() const
{
    QString userPath = QDir::homePath() + QLatin1Char('/') + QStringLiteral(".TileZed");
    return userPath;
}

QString Preferences::userPath(const QString &fileName) const
{
    return userPath() + QLatin1Char('/') + fileName;
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
    return appDirPath();
#elif defined(Q_OS_MAC)
    return appDirPath() + QStringLiteral("/../Config");
#elif defined(Q_OS_UNIX)
    return mShareDirPath + QStringLiteral("/config");
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
    return appDirPath() + QStringLiteral("/docs");
#elif defined(Q_OS_MAC)
    return appDirPath() + QStringLiteral("/../Docs");
#elif defined(Q_OS_UNIX)
    return mShareDirPath + QStringLiteral("/docs");
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
    return appDirPath() + QStringLiteral("/lua");
#elif defined(Q_OS_MAC)
    return appDirPath() + QStringLiteral("/../Lua");
#elif defined(Q_OS_UNIX)
    return mShareDirPath + QStringLiteral("/lua");
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
    mSettings->setValue(QStringLiteral("Interface/SnapToGrid"), mSnapToGrid);
    emit snapToGridChanged(mSnapToGrid);
}

void Preferences::setShowCellBorder(bool showCellBorder)
{
    if (showCellBorder == mShowCellBorder)
        return;

    mShowCellBorder = showCellBorder;
    mSettings->setValue(QStringLiteral("Interface/ShowCellBorder"), mShowCellBorder);
    emit showCellBorderChanged(mShowCellBorder);
}

void Preferences::setShowCoordinates(bool showCoords)
{
    if (showCoords == mShowCoordinates)
        return;

    mShowCoordinates = showCoords;
    mSettings->setValue(QStringLiteral("Interface/ShowCoordinates"), mShowCoordinates);
    emit showCoordinatesChanged(mShowCoordinates);
}

void Preferences::setShowWorldGrid(bool showGrid)
{
    if (showGrid == mShowWorldGrid)
        return;

    mShowWorldGrid = showGrid;
    mSettings->setValue(QStringLiteral("Interface/ShowWorldGrid"), mShowWorldGrid);
    emit showWorldGridChanged(mShowWorldGrid);
}

void Preferences::setShowCellGrid(bool showGrid)
{
    if (showGrid == mShowCellGrid)
        return;

    mShowCellGrid = showGrid;
    mSettings->setValue(QStringLiteral("Interface/ShowCellGrid"), mShowCellGrid);
    emit showCellGridChanged(mShowCellGrid);
}

void Preferences::setGridColor(const QColor &gridColor)
{
    if (mGridColor == gridColor)
        return;

    mGridColor = gridColor;
    mSettings->setValue(QStringLiteral("Interface/GridColor"), mGridColor.name());
    emit gridColorChanged(mGridColor);
}

void Preferences::setUseOpenGL(bool useOpenGL)
{
    if (mUseOpenGL == useOpenGL)
        return;

    mUseOpenGL = useOpenGL;
    mSettings->setValue(QStringLiteral("Interface/OpenGL"), mUseOpenGL);

    emit useOpenGLChanged(mUseOpenGL);
}

void Preferences::setLoadAllWorldThumbnails(bool thumbs)
{
    if (mLoadAllWorldThumbnails == thumbs)
        return;

    mLoadAllWorldThumbnails = thumbs;
    mSettings->setValue(QStringLiteral("Interface/LoadAllWorldThumbnails"), mLoadAllWorldThumbnails);

    emit loadAllWorldThumbnailsChanged(mLoadAllWorldThumbnails);
}

void Preferences::setShowWorldThumbnails(bool thumbs)
{
    if (mShowWorldThumbnails == thumbs)
        return;

    mShowWorldThumbnails = thumbs;
    mSettings->setValue(QStringLiteral("Interface/ShowWorldThumbnails"), mShowWorldThumbnails);

    emit showWorldThumbnailsChanged(mShowWorldThumbnails);
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
    mSettings->setValue(QStringLiteral("OpenFileDirectory"), mOpenFileDirectory);
}

QString Preferences::worldMapXMLFile() const
{
    return mWorldMapXMLFile;
}

void Preferences::setWorldMapXMLFile(const QString &path)
{
    if (mWorldMapXMLFile == path)
        return;
    mWorldMapXMLFile = path;
    mSettings->setValue(QStringLiteral("WorldMapXMLFile"), mWorldMapXMLFile);
}

void Preferences::setShowAdjacentMaps(bool show)
{
    if (mShowAdjacentMaps == show)
        return;

    mShowAdjacentMaps = show;
    mSettings->setValue(QStringLiteral("Interface/ShowAdjacentMaps"), mShowAdjacentMaps);

    emit showAdjacentMapsChanged(mShowAdjacentMaps);
}

void Preferences::setShowObjects(bool show)
{
    if (mShowObjects == show)
        return;

    mShowObjects = show;
    mSettings->setValue(QStringLiteral("Interface/ShowObjects"), mShowObjects);

    emit showObjectsChanged(mShowObjects);
}

void Preferences::setShowObjectNames(bool show)
{
    if (mShowObjectNames == show)
        return;

    mShowObjectNames = show;
    mSettings->setValue(QStringLiteral("Interface/ShowObjectNames"), mShowObjectNames);

    emit showObjectNamesChanged(mShowObjectNames);
}

void Preferences::setShowBMPs(bool show)
{
    if (mShowBMPs == show)
        return;

    mShowBMPs = show;
    mSettings->setValue(QStringLiteral("Interface/ShowBMPs"), mShowBMPs);

    emit showBMPsChanged(mShowBMPs);
}

void Preferences::setShowZombieSpawnImage(bool show)
{
    if (mShowZombieSpawnImage == show)
        return;

    mShowZombieSpawnImage = show;
    mSettings->setValue(QStringLiteral("Interface/ShowZombieSpawnImage"), mShowZombieSpawnImage);

    emit showZombieSpawnImageChanged(mShowZombieSpawnImage);
}

void Preferences::setZombieSpawnImageOpacity(qreal opacity)
{
    opacity = qMin(opacity, 1.0);
    opacity = qMax(opacity, 0.0);

    if (mZombieSpawnImageOpacity == opacity)
        return;

    mZombieSpawnImageOpacity = opacity;
    mSettings->setValue(QStringLiteral("Interface/ZombieSpawnImageOpacity"), mZombieSpawnImageOpacity);

    emit zombieSpawnImageOpacityChanged(mZombieSpawnImageOpacity);
}

void Preferences::setShowZonesInWorldView(bool show)
{
    if (mShowZonesInWorldView == show)
        return;

    mShowZonesInWorldView = show;
    mSettings->setValue(QStringLiteral("Interface/ShowZonesInWorldView"), mShowZonesInWorldView);

    emit showZonesInWorldViewChanged(mShowZonesInWorldView);
}

void Preferences::setShowMiniMap(bool show)
{
    if (show == mShowMiniMap)
        return;

    mShowMiniMap = show;
    mSettings->setValue(QStringLiteral("Interface/ShowMiniMap"), mShowMiniMap);
    emit showMiniMapChanged(mShowMiniMap);
}

void Preferences::setMiniMapWidth(int width)
{
    width = qMin(width, MINIMAP_WIDTH_MAX);
    width = qMax(width, MINIMAP_WIDTH_MIN);

    if (mMiniMapWidth == width)
        return;
    mMiniMapWidth = width;
    mSettings->setValue(QStringLiteral("Interface/MiniMapWidth"), width);
    emit miniMapWidthChanged(mMiniMapWidth);
}

void Preferences::setHighlightCurrentLevel(bool highlight)
{
    if (highlight == mHighlightCurrentLevel)
        return;

    mHighlightCurrentLevel = highlight;
    mSettings->setValue(QStringLiteral("Interface/HighlightCurrentLevel"), mHighlightCurrentLevel);
    emit highlightCurrentLevelChanged(mHighlightCurrentLevel);
}

void Preferences::setHighlightRoomUnderPointer(bool highlight)
{
    if (highlight == mHighlightRoomUnderPointer)
        return;
    mHighlightRoomUnderPointer = highlight;
    mSettings->setValue(QStringLiteral("Interface/HighlightRoomUnderPointer"),
                        mHighlightRoomUnderPointer);
    emit highlightRoomUnderPointerChanged(mHighlightRoomUnderPointer);
}

void Preferences::setHighlightUnlitRooms(bool highlight)
{
    if (highlight == mHighlightUnlitRooms)
        return;
    mHighlightUnlitRooms = highlight;
    mSettings->setValue(QStringLiteral("Interface/HighlightUnlitRooms"), mHighlightUnlitRooms);
    emit highlightUnlitRoomsChanged(mHighlightUnlitRooms);
}

void Preferences::setShowLotFloorsOnly(bool show)
{
    if (mShowLotFloorsOnly == show)
        return;
    mShowLotFloorsOnly = show;
    mSettings->setValue(QStringLiteral("Interface/ShowLotFloorsOnly"), show);
    emit showLotFloorsOnlyChanged(mShowLotFloorsOnly);
}

void Preferences::setShowOtherWorlds(bool show)
{
    if (show == mShowOtherWorlds)
        return;
    mShowOtherWorlds = show;
    mSettings->setValue(QStringLiteral("Interface/ShowOtherWorlds"),
                        mShowOtherWorlds);
    emit showOtherWorldsChanged(mShowOtherWorlds);
}

void Preferences::setMapsDirectory(const QString &path)
{
    if (mMapsDirectory == path)
        return;
    mMapsDirectory = path;
    mSettings->setValue(QStringLiteral("MapsDirectory/Current"), path);

    // Put this up, otherwise the progress dialog shows and hides for each lot.
    // Since each open document has its own ZLotManager, this shows and hides for each document as well.
//    ZProgressManager::instance()->begin(QStringLiteral("Checking lots..."));

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
    mSettings->setValue(QStringLiteral("TilesDirectory"), path);
    emit tilesDirectoryChanged();
}

QString Preferences::tiles2xDirectory() const
{
    if (mTilesDirectory.isEmpty())
        return QString();
    return mTilesDirectory + QLatin1Char('/') + QStringLiteral("2x");
}

QString Preferences::texturesDirectory() const
{
    return QDir(mTilesDirectory).filePath(QStringLiteral("Textures"));
}

void Preferences::setShowInvisibleTiles(bool show)
{
    if (mShowInvisibleTiles == show)
        return;

    mShowInvisibleTiles = show;
    mSettings->setValue(QStringLiteral("Interface/ShowInvisibleTiles"), mShowInvisibleTiles);

    emit showInvisibleTilesChanged(mShowInvisibleTiles);
}

void Preferences::setTheme(const QString &theme)
{
    if (mTheme == theme) {
        return;
    }
    mTheme = theme;
    applyTheme();
}

void Preferences::applyTheme() const
{
    mSettings->setValue(QStringLiteral("Interface/Theme"), mTheme);
    if (mTheme == QStringLiteral("Default")) {
        qApp->setStyleSheet(QString());
        return;
    }
    QString resource;
    if (mTheme == QStringLiteral("Breeze (Dark)")) {
        resource = QStringLiteral(":breeze/dark/stylesheet.qss");
    } else if (mTheme == QStringLiteral("QDarkStyle (Dark)")) {
        resource = QStringLiteral(":qdarkstyle/dark/darkstyle.qss");
    } else if (mTheme == QStringLiteral("QDarkStyle (Light)")) {
        resource = QStringLiteral(":qdarkstyle/light/lightstyle.qss");
    } else {
        return;
    }
    QFile theme_file(resource);
    theme_file.open(QFile::ReadOnly | QFile::Text);
    if(theme_file.isOpen()) {
        QTextStream ts(&theme_file);
        qApp->setStyleSheet(ts.readAll());        //set the theme here!
        theme_file.close();
    }
}
