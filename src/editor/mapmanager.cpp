/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "mapmanager.h"

#include "idletasks.h"
#include "mapassetmanager.h"
#include "mapcomposite.h"
#include "mapinfo.h"
#include "preferences.h"
#include "progress.h"
#include "tilemetainfomgr.h"
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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef QT_NO_DEBUG
inline QNoDebug noise() { return QNoDebug(); }
#else
inline QDebug noise() { return QDebug(QtDebugMsg); }
#endif

using namespace Tiled;
using namespace Tiled::Internal;
using namespace BuildingEditor;

SINGLETON_IMPL(MapManager)

MapManager::MapManager() :
    mFileSystemWatcher(new FileSystemWatcher(this)),
    mDeferralDepth(0),
    mDeferralQueued(false),
    mWaitingForMapInfo(nullptr),
    mNextThreadForJob(0)
#ifdef WORLDED
    , mReferenceEpoch(0)
#endif
{
    connect(mFileSystemWatcher, SIGNAL(fileChanged(QString)),
            SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, SIGNAL(timeout()),
            SLOT(fileChangedTimeout()));

    qRegisterMetaType<MapInfo*>("MapInfo*");
#if 0
    mMapReaderThread.resize(4);
    mMapReaderWorker.resize(mMapReaderThread.size());
    for (int i = 0; i < mMapReaderThread.size(); i++) {
        mMapReaderThread[i] = new InterruptibleThread;
        mMapReaderWorker[i] = new MapReaderWorker(mMapReaderThread[i], i);
        mMapReaderWorker[i]->moveToThread(mMapReaderThread[i]);
        connect(mMapReaderWorker[i], SIGNAL(loaded(Map*,MapInfo*)),
                SLOT(mapLoadedByThread(Map*,MapInfo*)));
        connect(mMapReaderWorker[i], SIGNAL(loaded(Building*,MapInfo*)),
                SLOT(buildingLoadedByThread(Building*,MapInfo*)));
        connect(mMapReaderWorker[i], SIGNAL(failedToLoad(QString,MapInfo*)),
                SLOT(failedToLoadByThread(QString,MapInfo*)));
        mMapReaderThread[i]->start();
    }
#endif
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(metaTilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(metaTilesetRemoved(Tiled::Tileset*)));
}

MapManager::~MapManager()
{
#if 0
    for (int i = 0; i < mMapReaderThread.size(); i++) {
        mMapReaderThread[i]->interrupt(); // stop the long-running task
        mMapReaderThread[i]->quit(); // exit the event loop
        mMapReaderThread[i]->wait(); // wait for thread to terminate
        delete mMapReaderThread[i];
        delete mMapReaderWorker[i];
    }
#endif
    TilesetManager *tilesetManager = TilesetManager::instancePtr();

    const AssetTable::const_iterator end = m_assets.constEnd();
    AssetTable::const_iterator it = m_assets.constBegin();
    while (it != end) {
        MapInfo *mapInfo = static_cast<MapInfo*>(it.value());
        if (Tiled::Map *map = mapInfo->map()) {
            tilesetManager->removeReferences(map->tilesets());
            delete map;
        }
        ++it;
    }
}

// mapName could be "Lot_Foo", "../Lot_Foo", "C:/maptools/Lot_Foo" with/without ".tmx"
QString MapManager::pathForMap(const QString &mapName, const QString &relativeTo)
{
    QString mapFilePath = mapName;

    if (QDir::isRelativePath(mapName)) {
        Q_ASSERT(!relativeTo.isEmpty());
        Q_ASSERT(!QDir::isRelativePath(relativeTo));
        mapFilePath = relativeTo + QLatin1Char('/') + mapName;
    }

    if (!mapFilePath.endsWith(QLatin1String(".tmx")) &&
            !mapFilePath.endsWith(QLatin1String(".tbx")))
        mapFilePath += QLatin1String(".tmx");

    QFileInfo fileInfo(mapFilePath);
    if (fileInfo.exists())
        return fileInfo.canonicalFilePath();

    return QString();
}
#if 0
class EditorMapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath)
    {
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};
#endif
MapInfo *MapManager::loadMap(const QString &mapName, const QString &relativeTo,
                             bool asynch, LoadPriority priority)
{
    // Do not emit mapLoaded() as a result of worker threads finishing
    // loading any maps while we are loading this one.
    // Any time QCoreApplication::processEvents() gets called,
    // a worker-thread's signal to us may be processed.
    MapManagerDeferral deferral;

    QString mapFilePath = pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty()) {
        mError = tr("A map file couldn't be found!\n%1").arg(mapName);
        return nullptr;
    }

    if (MapInfo* mapInfo = static_cast<MapInfo*>(get(mapFilePath))) {
        if (mapInfo->isReady() && mapInfo->mMap != nullptr && mapInfo->mMap->isReady()) {
            return mapInfo;
        }
    }

    QFileInfo fileInfoMap(mapFilePath);

    MapInfo *mapInfo = this->mapInfo(mapFilePath);
    if (mapInfo == nullptr || mapInfo->isFailure())
        return nullptr;

    if (mapInfo->isLoading()) {
#if 0
        foreach (MapReaderWorker *w, mMapReaderWorker)
            QMetaObject::invokeMethod(w, "possiblyRaisePriority",
                                      Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                                      Q_ARG(int,priority));
#endif
        if (!asynch) {
            noise() << "WAITING FOR MAP" << mapName << "with priority" << priority;
            Q_ASSERT(mWaitingForMapInfo == nullptr);
            mWaitingForMapInfo = mapInfo;
            for (int i = 0; i < mDeferredMaps.size(); i++) {
                MapDeferral md = mDeferredMaps[i];
                if (md.mapInfo == mapInfo) {
                    mDeferredMaps.removeAt(i);
                    mapLoadedByThread(md.map, md.mapInfo);
                    break;
                }
            }
            IdleTasks::instance().blockTasks(true);
            while (mapInfo->isLoading()) {
                qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            IdleTasks::instance().blockTasks(false);
            mWaitingForMapInfo = nullptr;
            if (!mapInfo->map())
                return nullptr;
        }
        return mapInfo;
    }
#if 1
    Q_ASSERT(mapInfo->mMap == nullptr);
    mapInfo->mMap = static_cast<MapAsset*>(MapAssetManager::instance().load(mapFilePath));
    mapInfo->addDependency(mapInfo->mMap);
    connect(mapInfo->mMap, &MapAsset::stateChanged, this, [this, mapInfo]{ MapManager::mapAssetStateChanged(mapInfo); });
#else
    mapInfo->mLoading = true;
    QMetaObject::invokeMethod(mMapReaderWorker[mNextThreadForJob], "addJob",
                              Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                              Q_ARG(int,priority));
    mNextThreadForJob = (mNextThreadForJob + 1) % mMapReaderThread.size();
#endif
    if (asynch)
        return mapInfo;

    // Wow.  Had a map *finish loading* after the PROGRESS call below displayed
    // the dialog and started processing events but before the qApp->processEvents()
    // call below, resulting in a hang because mWaitingForMapInfo wasn't set till
    // after the PROGRESS call.
    Q_ASSERT(mWaitingForMapInfo == nullptr);
    mWaitingForMapInfo = mapInfo;

    PROGRESS progress(tr("Reading %1").arg(fileInfoMap.completeBaseName()));
#if 0
    foreach (MapReaderWorker *w, mMapReaderWorker)
        QMetaObject::invokeMethod(w, "possiblyRaisePriority",
                                  Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                                  Q_ARG(int,priority));
#endif
    noise() << "WAITING FOR MAP" << mapName << "with priority" << priority;
    for (int i = 0; i < mDeferredMaps.size(); i++) {
        MapDeferral md = mDeferredMaps[i];
        if (md.mapInfo == mapInfo) {
            mDeferredMaps.removeAt(i);
            mapLoadedByThread(md.map, md.mapInfo);
            break;
        }
    }
    IdleTasks::instance().blockTasks(true);
    while (mapInfo->isLoading()) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    IdleTasks::instance().blockTasks(false);

    mWaitingForMapInfo = nullptr;
    if (mapInfo->map())
        return mapInfo;
    return nullptr;
}


#include <QCoreApplication>
#include <QXmlStreamReader>

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

MapInfo *MapManager::mapInfo(const QString &mapFilePath)
{
#if 1
    MapInfo* mapInfo = (MapInfo*)load(AssetPath(mapFilePath));
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
//        AssetTaskManager::instance().updateAsyncTransactions();
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

MapInfo *MapManager::newFromMap(Tiled::Map *map, const QString &mapFilePath)
{
    MapInfo *mapInfo = new MapInfo(map->orientation(),
                                map->width(), map->height(),
                                map->tileWidth(), map->tileHeight());
    mapInfo->mMap = new MapAsset(map, mapFilePath, MapAssetManager::instancePtr());
    mapInfo->mBeingEdited = true;

    if (!mapFilePath.isEmpty())
    {
        Q_ASSERT(!QFileInfo(mapFilePath).isRelative());
        mapInfo->setFilePath(mapFilePath);
//        mMapInfo[mapFilePath] = info;
    }

    mapInfo->addDependency(mapInfo->mMap);

    // Not adding to the m_assets table because this function is for maps being edited, not lots

    return mapInfo;
}

// FIXME: this map is shared by any CellDocument whose cell has no map specified.
// Adding sub-maps to a cell may add new layers to this shared map.
// If that happens, all CellScenes using this map will need to be updated.
MapInfo *MapManager::getEmptyMap()
{
    QString mapFilePath(QLatin1String("<empty>"));
    if (MapInfo *mapInfo = static_cast<MapInfo*>(get(mapFilePath)))
    {
        return mapInfo;
    }

    MapInfo *mapInfo = new MapInfo(Map::LevelIsometric, 300, 300, 64, 32);
    Map *map = new Map(mapInfo->orientation(),
                       mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

    mapInfo->mMap = new MapAsset(map, mapFilePath, MapAssetManager::instancePtr());
    mapInfo->addDependency(mapInfo->mMap);
    mapInfo->setFilePath(mapFilePath);
    m_assets[mapFilePath] = mapInfo;
#ifdef WORLDED
    addReferenceToMap(mapInfo);
#endif
    return mapInfo;
}

MapInfo *MapManager::getPlaceholderMap(const QString &mapName, int width, int height)
{
    QString mapFilePath(mapName);
    MapInfo *mapInfo = static_cast<MapInfo*>(get(mapFilePath));
    if (mapInfo != nullptr && mapInfo->mMap != nullptr)
    {
         return mapInfo;
    }

    if (width <= 0) width = 32;
    if (height <= 0) height = 32;
    if (mapInfo == nullptr)
    {
        mapInfo = new MapInfo(Map::LevelIsometric, width, height, 64, 32);
        mapInfo->m_manager = this;
    }
    Map *map = new Map(mapInfo->orientation(), mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

    mapInfo->mMap = new MapAsset(map, mapFilePath, MapAssetManager::instancePtr());
    mapInfo->addDependency(mapInfo->mMap);
    mapInfo->setFilePath(mapFilePath);
    mapInfo->mPlaceholder = true;
    m_assets[mapFilePath] = mapInfo;

    return mapInfo;
}

void MapManager::mapParametersChanged(MapInfo *mapInfo)
{
    Map *map = mapInfo->map();
    Q_ASSERT(map);
    mapInfo->mHeight = map->height();
    mapInfo->mWidth = map->width();
    mapInfo->mTileWidth = map->tileWidth();
    mapInfo->mTileHeight = map->tileHeight();
}

#ifdef WORLDED
void MapManager::addReferenceToMap(MapInfo *mapInfo)
{
    Q_ASSERT(mapInfo->mMap != nullptr);
    if (mapInfo->mMap) {
        MapAssetManager::instance().load(mapInfo->mMap);
        mapInfo->mReferenceEpoch = mReferenceEpoch;
        if (mapInfo->getRefCount() == 1)
            ++mReferenceEpoch;
        noise() << "MapManager refCount++ =" << mapInfo->getRefCount() << mapInfo->mFilePath;
    }
}

void MapManager::removeReferenceToMap(MapInfo *mapInfo)
{
    Q_ASSERT(mapInfo->mMap != nullptr);
    if (mapInfo->mMap) {
        Q_ASSERT(mapInfo->getRefCount() > 0);
        MapAssetManager::instance().unload(mapInfo->mMap);
        noise() << "MapManager refCount-- =" << mapInfo->getRefCount() << mapInfo->mFilePath;
        purgeUnreferencedMaps();
    }
}

void MapManager::purgeUnreferencedMaps()
{
    int unpurged = 0;
    for (Asset* asset : m_assets) {
        MapInfo *mapInfo = static_cast<MapInfo*>(asset);
        bool bigMap = mapInfo->size() == QSize(300, 300);
        if ((mapInfo->mMap && mapInfo->mMap->getRefCount() <= 0) &&
                ((bigMap && (mapInfo->mReferenceEpoch <= mReferenceEpoch - 10)) ||
                (mapInfo->mReferenceEpoch <= mReferenceEpoch - 50))) {
            noise() << "MapManager purging" << mapInfo->mFilePath;
            TilesetManager& tilesetMgr = TilesetManager::instance();
            tilesetMgr.removeReferences(mapInfo->map()->tilesets());
            mapInfo->mMap->disconnect(mapInfo);
//            MapAssetManager::instance().enableUnload(true);
//            MapAssetManager::instance().unload(mapInfo->mMap);
            mapInfo->mMap = nullptr;
        }
        else if (mapInfo->mMap && mapInfo->getRefCount() <= 0) {
            unpurged++;
        }
    }
    if (unpurged) noise() << "MapManager unpurged =" << unpurged;
}

void MapManager::newMapFileCreated(const QString &path)
{
    // If a cell view is open with a placeholder map and that map now exists,
    // read the new map and allow the cell-scene to update itself.
    // This code is 90% the same as fileChangedTimeout().
    for (Asset* asset : m_assets) {
        MapInfo* mapInfo = static_cast<MapInfo*>(asset);
        if (!mapInfo->mPlaceholder || !mapInfo->mMap)
            continue;
        if (QFileInfo(mapInfo->path()) != QFileInfo(path))
            continue;
        mFileSystemWatcher->addPath(mapInfo->path()); // FIXME: make canonical?
        fileChanged(mapInfo->path());
    }

    emit mapFileCreated(path);
}
#endif // WORLDED

Tiled::Map *MapManager::convertOrientation(Tiled::Map *map, Tiled::Map::Orientation orient)
{
    Tiled::Map::Orientation orient0 = map->orientation();
    Tiled::Map::Orientation orient1 = orient;

    if (orient0 != orient1) {
        Tiled::Map *newMap = map->clone();
        newMap->setOrientation(orient);
        QPoint offset(3, 3);
        if (orient0 == Tiled::Map::Isometric && orient1 == Map::LevelIsometric) {
            foreach (Layer *layer, newMap->layers()) {
                int level;
                if (MapComposite::levelForLayer(layer, &level) && level > 0)
                    layer->offset(offset * level, layer->bounds(), false, false);
            }
        }
        if (orient0 == Tiled::Map::LevelIsometric && orient1 == Map::Isometric) {
            int level, maxLevel = 0;
            foreach (Layer *layer, map->layers())
                if (MapComposite::levelForLayer(layer, &level))
                    maxLevel = qMax(maxLevel, level);
            newMap->setWidth(map->width() + maxLevel * 3);
            newMap->setHeight(map->height() + maxLevel * 3);
            foreach (Layer *layer, newMap->layers()) {
                MapComposite::levelForLayer(layer, &level);
                layer->resize(newMap->size(), offset * (maxLevel - level));
            }
        }
        TilesetManager& tilesetManager = TilesetManager::instance();
        tilesetManager.addReferences(newMap->tilesets());
        map = newMap;
    }

    return map;
}

void MapManager::fileChanged(const QString &path)
{
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void MapManager::fileChangedTimeout()
{
#if 0
    PROGRESS progress(tr("Examining changed maps..."));
#endif

    foreach (const QString &path, mChangedFiles) {
        if (MapInfo* mapInfo = static_cast<MapInfo*>(get(path))) {
            noise() << "MapManager::fileChanged" << path;
            mFileSystemWatcher->removePath(path);
            QFileInfo info(path);
            if (info.exists()) {
                reload(mapInfo);
//                mFileSystemWatcher->addPath(path);
                if (mapInfo->mMap != nullptr) {
                    Q_ASSERT(!mapInfo->isBeingEdited());
                    MapAssetManager::instance().reload(mapInfo->mMap);
                }
                emit mapFileChanged(mapInfo); // FIXME
            }
        }
    }

    mChangedFiles.clear();
}

void MapManager::metaTilesetAdded(Tileset *tileset)
{
    Q_UNUSED(tileset)
    for (Asset* asset : m_assets)
    {
        MapInfo *mapInfo = static_cast<MapInfo*>(asset);
        if (mapInfo->map() && mapInfo->path().endsWith(QLatin1String(".tbx"))
                && mapInfo->map()->hasUsedMissingTilesets())
        {
            fileChanged(mapInfo->path());
        }
    }
}

void MapManager::metaTilesetRemoved(Tileset *tileset)
{
    for (Asset* asset : m_assets)
    {
        MapInfo *mapInfo = static_cast<MapInfo*>(asset);
        if (mapInfo->map() && mapInfo->path().endsWith(QLatin1String(".tbx"))
                && mapInfo->map()->usedTilesets().contains(tileset))
        {
            fileChanged(mapInfo->path());
        }
    }
}

void MapManager::mapLoadedByThread(Tiled::Map *map, MapInfo *mapInfo)
{
    if (mapInfo != mWaitingForMapInfo && mDeferralDepth > 0) {
        noise() << "MAP LOADED BY THREAD - DEFERR" << mapInfo->path();
        mDeferredMaps += MapDeferral(mapInfo, map);
        if (!mDeferralQueued) {
            QMetaObject::invokeMethod(this, "processDeferrals", Qt::QueuedConnection);
            mDeferralQueued = true;
        }
        return;
    }

    noise() << "MAP LOADED BY THREAD" << mapInfo->path();

    MapManagerDeferral deferral;

    Tile *missingTile = TilesetManager::instance().missingTile();
    for (Tileset *tileset : map->missingTilesets()) {
        if (tileset == missingTile->tileset())
            continue;
        if (tileset->tileHeight() == missingTile->height() && tileset->tileWidth() == missingTile->width()) {
            // Replace the all-red image with something nicer.
            for (int i = 0; i < tileset->tileCount(); i++)
                tileset->tileAt(i)->setImage(missingTile);
        }
    }
    TilesetManager::instance().addReferences(map->tilesets());

    bool replace = mapInfo->mMap != nullptr;
    if (replace) {
        Q_ASSERT(!mapInfo->isBeingEdited());
        emit mapAboutToChange(mapInfo);
        TilesetManager& tilesetMgr = TilesetManager::instance();
        tilesetMgr.removeReferences(mapInfo->map()->tilesets());
//        delete mapInfo->mMap;
        MapAssetManager::instance().unload(mapInfo->mMap);
        mapInfo->mMap = nullptr;
    }

//    mapInfo->mMap = map;
    mapInfo->mOrientation = map->orientation();
    mapInfo->mHeight = map->height();
    mapInfo->mWidth = map->width();
    mapInfo->mTileWidth = map->tileWidth();
    mapInfo->mTileHeight = map->tileHeight();
    mapInfo->mPlaceholder = false;
//    mapInfo->mLoading = false;

    if (replace)
        emit mapChanged(mapInfo);

#ifdef WORLDED
    // The reference count is zero, but prevent it being immediately purged.
    // FIXME: add a reference and let the caller deal with it.
    mapInfo->mReferenceEpoch = mReferenceEpoch;
#endif

    emit mapLoaded(mapInfo);
}

void MapManager::buildingLoadedByThread(Building *building, MapInfo *mapInfo)
{
    MapManagerDeferral deferral;

    BuildingReader reader;
    reader.fix(building);

    BuildingMap::loadNeededTilesets(building);

    BuildingMap bmap(building);
    Map *map = bmap.mergedMap();
    bmap.addRoomDefObjects(map);

    delete building;

    QSet<Tileset*> usedTilesets = map->usedTilesets();
    usedTilesets.remove(TilesetManager::instance().missingTileset());

    TileMetaInfoMgr::instance()->loadTilesets(usedTilesets.toList());

    // The map references TileMetaInfoMgr's tilesets, but we add a reference
    // to them ourself below.
    TilesetManager::instance().removeReferences(map->tilesets());

    mapLoadedByThread(map, mapInfo);
}

void MapManager::failedToLoadByThread(const QString error, MapInfo *mapInfo)
{
//    mapInfo->mLoading = false;
    mError = error;
    emit mapFailedToLoad(mapInfo);
}

void MapManager::mapAssetStateChanged(MapInfo *mapInfo)
{
    MapAsset* mapAsset = mapInfo->mMap;
    if (mapAsset == nullptr)
        return;
    if (mapAsset->isReady())
    {
        if (mapAsset->mBuilding != nullptr)
        {
            buildingLoadedByThread(mapAsset->mBuilding, mapInfo);
            mapAsset->mBuilding = nullptr; // deleted by buildingLoadedByThread()
        }
        else if (mapAsset->mMap != nullptr)
        {
            mapLoadedByThread(mapAsset->mMap, mapInfo);
        }
    }
    else if (mapAsset->isFailure())
    {
        // FIXME: get task's mError
        failedToLoadByThread(QStringLiteral("FIXME mapAssetStateChanged()"), mapInfo);
    }
}

void MapManager::deferThreadResults(bool defer)
{
    if (defer) {
        ++mDeferralDepth;
        noise() << "MapManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
        noise() << "MapManager::deferThreadResults depth-- =" << mDeferralDepth - 1;
        if (--mDeferralDepth == 0) {
            if (!mDeferralQueued && mDeferredMaps.size()) {
                QMetaObject::invokeMethod(this, "processDeferrals", Qt::QueuedConnection);
                mDeferralQueued = true;
            }
        }
    }
}

void MapManager::processDeferrals()
{
    if (mDeferralDepth > 0) {
        noise() << "processDeferrals deferred - FML";
        mDeferralQueued = false;
        return;
    }
    QList<MapDeferral> deferrals = mDeferredMaps;
    mDeferredMaps.clear();
    mDeferralQueued = false;
    for (MapDeferral md : deferrals) {
        mapLoadedByThread(md.map, md.mapInfo);
    }
}

Asset *MapManager::createAsset(AssetPath path, AssetParams *params)
{
    return new MapInfo(path, this/*, params*/);
}

void MapManager::destroyAsset(Asset *asset)
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
        MapManager::instance().loadMapInfoTaskFinished(this);
    }

    void release() override
    {
        delete mMapInfo;
        delete this;
    }

    MapInfo* mMapInfo = nullptr;
    bool bLoaded = false;
};

void MapManager::startLoading(Asset *asset)
{
    MapInfo* mapInfo = static_cast<MapInfo*>(asset);
    AssetTask_LoadMapInfo* assetTask = new AssetTask_LoadMapInfo(mapInfo);
//    assetTask->connect(this, [this, assetTask]{ loadMapInfoTaskFinished(assetTask); });
    setTask(asset, assetTask);
    AssetTaskManager::instance().submit(assetTask);
}

void MapManager::loadMapInfoTaskFinished(AssetTask_LoadMapInfo *task)
{
    if (task->bLoaded)
    {
        MapInfo* mapInfo = static_cast<MapInfo*>(task->m_asset);
        mapInfo->setFrom(task->mMapInfo);
        mFileSystemWatcher->addPath(task->m_asset->getPath().getString());
        onLoadingSucceeded(mapInfo);
    }
    else
    {
        onLoadingFailed(task->m_asset);
    }
}

/////

MapReaderWorker::MapReaderWorker(InterruptibleThread *thread, int id) :
    BaseWorker(thread),
    mID(id)
{
}

MapReaderWorker::~MapReaderWorker()
{
}

void MapReaderWorker::work()
{
    IN_WORKER_THREAD

    if (mJobs.size()) {
        if (aborted()) {
            mJobs.clear();
            return;
        }

        Job job = mJobs.takeFirst();
        debugJobs("take job");

        if (job.mapInfo->path().endsWith(QLatin1String(".tbx"))) {
            Building *building = loadBuilding(job.mapInfo);
            if (building)
                emit loaded(building, job.mapInfo);
            else
                emit failedToLoad(mError, job.mapInfo);
        } else {
//            noise() << "READING STARTED" << job.mapInfo->path();
            Map *map = loadMap(job.mapInfo);
//            noise() << "READING FINISHED" << job.mapInfo->path();
            if (map)
                emit loaded(map, job.mapInfo);
            else
                emit failedToLoad(mError, job.mapInfo);
        }

//        QCoreApplication::processEvents(); // handle changing job priority
    }

    if (mJobs.size()) scheduleWork();
}

void MapReaderWorker::addJob(MapInfo *mapInfo, int priority)
{
    IN_WORKER_THREAD

    int index = 0;
    while ((index < mJobs.size()) && (mJobs[index].priority >= priority))
        ++index;

    mJobs.insert(index, Job(mapInfo, priority));
    debugJobs("add job");
    scheduleWork();
}

void MapReaderWorker::possiblyRaisePriority(MapInfo *mapInfo, int priority)
{
    IN_WORKER_THREAD

    for (int i = 0; i < mJobs.size(); i++) {
        if (mJobs[i].mapInfo == mapInfo && mJobs[i].priority < priority) {
            int j;
            for (j = i - 1; j >= 0 && mJobs[j].priority < priority; j--) {}
            mJobs[i].priority = priority;
            mJobs.move(i, j + 1);
            debugJobs("raise priority");
            break;
        }
    }
}

class MapReaderWorker_MapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath)
    {
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};

Map *MapReaderWorker::loadMap(MapInfo *mapInfo)
{
    MapReaderWorker_MapReader reader;
//    reader.setTilesetImageCache(TilesetManager::instance().imageCache()); // not thread-safe class
    Map *map = reader.readMap(mapInfo->path());
    if (!map)
        mError = reader.errorString();
    return map;
}

MapReaderWorker::Building *MapReaderWorker::loadBuilding(MapInfo *mapInfo)
{
    BuildingReader reader;
    Building *building = reader.read(mapInfo->path());
    if (!building)
        mError = reader.errorString();
    return building;
}

void MapReaderWorker::debugJobs(const char *msg)
{
    QStringList out;
    foreach (Job job, mJobs) {
        out += QString::fromLatin1("    %1 priority=%2\n").arg(QFileInfo(job.mapInfo->path()).fileName()).arg(job.priority);
    }
    noise() << "MRW #" << mID << ": " << msg << "\n" << out;
}
