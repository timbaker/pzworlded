/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#include "mapinfomanager.h"

#include "idletasks.h"
#include "mapcomposite.h"
#include "mapinfo.h"
#include "preferences.h"
#include "progress.h"
#include "tilesetmanager.h"

#include "assettask.h"
#include "assettaskmanager.h"

#include "map.h"
#include "mapreader.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include "qtlockedfile.h"
using namespace SharedTools;

#include "BuildingEditor/building.h"
#include "BuildingEditor/buildingreader.h"
#include "BuildingEditor/buildingmap.h"
#include "BuildingEditor/buildingobjects.h"
#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

namespace {
#ifdef QT_NO_DEBUG
inline QNoDebug noise() { return QNoDebug(); }
#else
inline QDebug noise() { return QDebug(QtDebugMsg); }
#endif
}

using namespace Tiled;
using namespace Tiled::Internal;
using namespace BuildingEditor;

SINGLETON_IMPL(MapInfoManager)

MapInfoManager::MapInfoManager() :
    mFileSystemWatcher(new FileSystemWatcher(this)),
    mDeferralDepth(0),
    mWaitingForMapInfo(nullptr)
{
    connect(mFileSystemWatcher, &FileSystemWatcher::fileChanged,
            this, &MapInfoManager::fileChanged);

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, &QTimer::timeout,
            this, &MapInfoManager::fileChangedTimeout);

    qRegisterMetaType<MapInfo*>("MapInfo*");

    connect(IdleTasks::instancePtr(), &IdleTasks::idleTime, this, &MapInfoManager::processDeferrals);
}

MapInfoManager::~MapInfoManager()
{
    TilesetManager *tilesetManager = TilesetManager::instancePtr();

    for (Asset* asset : m_assets) {
        MapInfo *mapInfo = static_cast<MapInfo*>(asset);
    }
}

class MapInfoReader
{
    Q_DECLARE_TR_FUNCTIONS(MapInfoReader)

public:
    bool openFile(QtLockedFile *file)
    {
        if (!file->exists()) {
            mError = tr("File not found: %1").arg(file->fileName());
            return false;
        }
        if (!file->open(QFile::ReadOnly | QFile::Text)) {
            mError = tr("Unable to read file: %1").arg(file->fileName());
            return false;
        }
        if (!file->lock(QtLockedFile::ReadLock)) {
            mError = tr("Unable to lock file for reading: %1").arg(file->fileName());
            return false;
        }

        return true;
    }

    MapInfo *readMap(const QString &mapFilePath)
    {
        QtLockedFile file(mapFilePath);
        if (!openFile(&file))
            return nullptr;

        if (mapFilePath.endsWith(QLatin1String(".tbx")))
            return readBuilding(&file, QFileInfo(mapFilePath).absolutePath());

        return readMap(&file, QFileInfo(mapFilePath).absolutePath());
    }

    MapInfo *readMap(QIODevice *device, const QString &path)
    {
        Q_UNUSED(path)

        mError.clear();
        mMapInfo = nullptr;

#if 1
        QByteArray data = device->read(1024);
        xml.addData(data);
#else
        xml.setDevice(device);
#endif

        if (xml.readNextStartElement() && xml.name() == QLatin1String("map")) {
            mMapInfo = readMap();
        } else {
            xml.raiseError(tr("Not a map file."));
        }

        return mMapInfo;
    }

    MapInfo *readMap()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("map"));

        const QXmlStreamAttributes atts = xml.attributes();
        const int mapWidth =
                atts.value(QLatin1String("width")).toString().toInt();
        const int mapHeight =
                atts.value(QLatin1String("height")).toString().toInt();
        const int tileWidth =
                atts.value(QLatin1String("tilewidth")).toString().toInt();
        const int tileHeight =
                atts.value(QLatin1String("tileheight")).toString().toInt();

        const QString orientationString =
                atts.value(QLatin1String("orientation")).toString();
        const Map::Orientation orientation =
                orientationFromString(orientationString);
        if (orientation == Map::Unknown) {
            xml.raiseError(tr("Unsupported map orientation: \"%1\"")
                           .arg(orientationString));
        }

        mMapInfo = new MapInfo(orientation, mapWidth, mapHeight, tileWidth, tileHeight);

        return mMapInfo;
    }

    MapInfo *readBuilding(QIODevice *device, const QString &path)
    {
        Q_UNUSED(path)

        mError.clear();
        mMapInfo = nullptr;

#if 1
        QByteArray data = device->read(1024);
        xml.addData(data);
#else
        xml.setDevice(device);
#endif

        if (xml.readNextStartElement() && xml.name() == QLatin1String("building")) {
            mMapInfo = readBuilding();
        } else {
            xml.raiseError(tr("Not a building file."));
        }

        return mMapInfo;
    }

    MapInfo *readBuilding()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("building"));

        const QXmlStreamAttributes atts = xml.attributes();
        const int mapWidth =
                atts.value(QLatin1String("width")).toString().toInt();
        const int mapHeight =
                atts.value(QLatin1String("height")).toString().toInt();
        const int tileWidth = 64;
        const int tileHeight = 32;

        const Map::Orientation orient = static_cast<Map::Orientation>(BuildingEditor::BuildingMap::defaultOrientation());

#if 1
        // FIXME: If Map::Isometric orientation is used then we must know the number
        // of floors to determine the correct map size.  That would require parsing
        // the whole .tbx file.
        Q_ASSERT(orient == Map::LevelIsometric);
        int extra = 1;
#else
        int maxLevel = building->floorCount() - 1;
        int extraForWalls = 1;
        int extra = (orient == Map::LevelIsometric)
                ? extraForWalls : maxLevel * 3 + extraForWalls;
#endif

        mMapInfo = new MapInfo(orient, mapWidth + extra, mapHeight + extra,
                               tileWidth, tileHeight);

        return mMapInfo;
    }

    QXmlStreamReader xml;
    MapInfo *mMapInfo;
    QString mError;
};

MapInfo *MapInfoManager::mapInfo(const QString &mapFilePath)
{
    // Caller must ensure mapFilePath is the canonical path as returned by MapManager::pathForMap()
#if 1
    if (mapFilePath.isEmpty())
    {
        qDebug() << "MapInfoManager::mapInfo EMPTY path given";
    }
    MapInfo* mapInfo = static_cast<MapInfo*>(get(mapFilePath));
    if (mapInfo == nullptr)
    {
        mapInfo = static_cast<MapInfo*>(load(mapFilePath));
    }
    if (mapInfo->isFailure())
    {
        return nullptr;
    }
    return mapInfo;
#elif 0
    if (mapInfo == nullptr)
    {
        return mapInfo;
    }
    if (mapInfo->isFailure())
    {
        return nullptr;
    }
    // FIXME
    IdleTasks::instance().blockTasks(true);
    while (mapInfo->isEmpty())
    {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        AssetTaskManager::instance().updateAsyncTransactions();
        Sleep::msleep(10);
    }
    IdleTasks::instance().blockTasks(false);
    if (mapInfo->isFailure())
    {
        return nullptr;
    }
    return mapInfo;
#else
    if (mMapInfo.contains(mapFilePath))
       return mMapInfo[mapFilePath];

    QFileInfo fileInfo(mapFilePath);
    if (!fileInfo.exists())
        return nullptr;

    MapInfoReader reader;
    MapInfo *mapInfo = reader.readMap(mapFilePath);
    if (!mapInfo)
        return nullptr;

    noise() << "read map info for" << mapFilePath;
    mapInfo->setFilePath(mapFilePath);

    mMapInfo[mapFilePath] = mapInfo;
    mFileSystemWatcher->addPath(mapFilePath);

    return mapInfo;
#endif
}

void MapInfoManager::mapParametersChanged(MapInfo *mapInfo)
{
#if 0
    Map *map = mapInfo->map();
    Q_ASSERT(map);
    mapInfo->mHeight = map->height();
    mapInfo->mWidth = map->width();
    mapInfo->mTileWidth = map->tileWidth();
    mapInfo->mTileHeight = map->tileHeight();
#endif
}

#ifdef WORLDED

void MapInfoManager::newMapFileCreated(const QString &path)
{
    for (Asset* asset : m_assets) {
        MapInfo* mapInfo = static_cast<MapInfo*>(asset);
    }

    emit mapFileCreated(path);
}
#endif // WORLDED

void MapInfoManager::fileChanged(const QString &path)
{
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void MapInfoManager::fileChangedTimeout()
{
#if 0
    PROGRESS progress(tr("Examining changed maps..."));
#endif

    foreach (const QString &path, mChangedFiles) {
        qDebug() << "MapInfoManager::fileChangedTimeOut" << path << "exists=" << QFileInfo(path).exists();
        if (MapInfo* mapInfo = static_cast<MapInfo*>(get(path))) {
            noise() << "MapInfoManager::fileChangedTimeout" << path;
            mFileSystemWatcher->removePath(path);
            QFileInfo info(path);
            if (info.exists()) {
                reload(mapInfo);
//                mFileSystemWatcher->addPath(path);
//                emit mapFileChanged(mapInfo); // FIXME
            }
        }
    }

    mChangedFiles.clear();
}

void MapInfoManager::mapLoadedByThread(MapInfo *mapInfo)
{
    if (mapInfo != mWaitingForMapInfo && mDeferralDepth > 0) {
        noise() << "MAPINFO LOADED BY THREAD - DEFERR" << mapInfo->path();
        mDeferredMaps += MapDeferral(mapInfo);
        return;
    }

    noise() << "MAPINFO LOADED BY THREAD" << mapInfo->path();

    MapInfoManagerDeferral deferral;

    bool replace = mapInfo->orientation() != Tiled::Map::Orientation::Unknown;
    if (replace) {
        Q_ASSERT(!mapInfo->isBeingEdited());
        emit mapAboutToChange(mapInfo);
    }

    mapInfo->setFrom(mapInfo->mLoaded);
    delete mapInfo->mLoaded;
    mapInfo->mLoaded = nullptr;

    onLoadingSucceeded(mapInfo);

    if (replace)
        emit mapChanged(mapInfo);

    emit mapLoaded(mapInfo);
}

void MapInfoManager::failedToLoadByThread(const QString error, MapInfo *mapInfo)
{
//    mapInfo->mLoading = false;
    mError = error;
    emit mapFailedToLoad(mapInfo);
}

void MapInfoManager::deferThreadResults(bool defer)
{
    if (defer) {
        ++mDeferralDepth;
        noise() << "MapInfoManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
        --mDeferralDepth;
        noise() << "MapInfoManager::deferThreadResults depth-- =" << mDeferralDepth;
    }
}

void MapInfoManager::processDeferrals()
{
    if (mDeferralDepth > 0) {
        noise() << "MapInfoManager::processDeferrals deferred - FML";
        return;
    }
    QList<MapDeferral> deferrals = mDeferredMaps;
    mDeferredMaps.clear();
    for (MapDeferral md : deferrals) {
        mapLoadedByThread(md.mapInfo);
    }
}

Asset *MapInfoManager::createAsset(AssetPath path, AssetParams *params)
{
    return new MapInfo(path, this/*, params*/);
}

void MapInfoManager::destroyAsset(Asset *asset)
{
    Q_UNUSED(asset);
}

class AssetTask_LoadMapInfo : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadMapInfo(MapInfo* asset)
        : BaseAsyncAssetTask(asset)
    {

    }

    void run() override
    {
        QString mapFilePath = m_asset->getPath().getString();

        MapInfoReader reader;
        mMapInfo = reader.readMap(mapFilePath);
        if (mMapInfo == nullptr)
            return;

        noise() << "read map info for" << mapFilePath;
        mMapInfo->setFilePath(mapFilePath);

        bLoaded = true;
    }

    void handleResult() override
    {
        MapInfoManager::instance().loadMapInfoTaskFinished(this);
    }

    void release() override
    {
        delete mMapInfo;
        delete this;
    }

    MapInfo* mMapInfo = nullptr;
    bool bLoaded = false;
};

void MapInfoManager::startLoading(Asset *asset)
{
    MapInfo* mapInfo = static_cast<MapInfo*>(asset);
    mapInfo->setFilePath(asset->getPath().getString()); // canonical, hopefully

    AssetTask_LoadMapInfo* assetTask = new AssetTask_LoadMapInfo(mapInfo);
    setTask(asset, assetTask);
    AssetTaskManager::instance().submit(assetTask);
}

void MapInfoManager::loadMapInfoTaskFinished(AssetTask_LoadMapInfo *task)
{
    if (task->bLoaded)
    {
        MapInfo* mapInfo = static_cast<MapInfo*>(task->m_asset);
        mapInfo->mLoaded = task->mMapInfo;
        task->mMapInfo = nullptr;
        mFileSystemWatcher->addPath(task->m_asset->getPath().getString());
        mapLoadedByThread(mapInfo);
        // Could be deferred
        // onLoadingSucceeded(mapInfo);
    }
    else
    {
        onLoadingFailed(task->m_asset);
    }
}
