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
#include "mapasset.h"
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

SINGLETON_IMPL(MapManager)

MapManager::MapManager() :
    mFileSystemWatcher(new FileSystemWatcher(this)),
    mDeferralDepth(0),
    mWaitingForMapInfo(nullptr),
#ifdef WORLDED
    mReferenceEpoch(0)
#endif
{
    connect(mFileSystemWatcher, &FileSystemWatcher::fileChanged,
            this, &MapManager::fileChanged);

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, &QTimer::timeout,
            this, &MapManager::fileChangedTimeout);

    qRegisterMetaType<MapInfo*>("MapInfo*");

    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded,
            this, &MapManager::metaTilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved,
            this, &MapManager::metaTilesetRemoved);

    connect(IdleTasks::instancePtr(), &IdleTasks::idleTime, this, &MapManager::processDeferrals);
}

MapManager::~MapManager()
{
    TilesetManager *tilesetManager = TilesetManager::instancePtr();

    for (Asset* asset : m_assets) {
        MapAsset *mapAsset = static_cast<MapAsset*>(asset);
        if (Tiled::Map *map = mapAsset->map()) {
            tilesetManager->removeReferences(map->tilesets());
            delete map;
        }
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
MapAsset *MapManager::loadMap(const QString &mapName, const QString &relativeTo,
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

    MapAsset* mapAsset = static_cast<MapAsset*>(get(mapFilePath));
    if (mapAsset != nullptr) {
        if (mapAsset->isReady()) {
            return mapAsset;
        }
        if (mapAsset->isFailure()) {
            return nullptr;
        }
    }

    // getRefCount() is zero if the asset was previously loaded, then unloaded
    if (mapAsset == nullptr || mapAsset->getRefCount() == 0)
    {
        mapAsset = static_cast<MapAsset*>(load(mapFilePath));
    }

    if (mapAsset->isEmpty()) {
        if (!asynch) {
            noise() << "WAITING FOR MAP" << mapName << "with priority" << priority;
            Q_ASSERT(mWaitingForMapInfo == nullptr);
            mWaitingForMapInfo = mapAsset;
            for (int i = 0; i < mDeferredMaps.size(); i++) {
                MapDeferral md = mDeferredMaps[i];
                if (md.mapAsset == mapAsset) {
                    mDeferredMaps.removeAt(i);
                    mapLoadedByThread(md.mapAsset);
                    break;
                }
            }
            processEventsWhile([&]{ return mapAsset->isEmpty(); });
            mWaitingForMapInfo = nullptr;
            if (!mapAsset->map())
                return nullptr;
        }
        return mapAsset;
    }

    Q_ASSERT(mapAsset->mMap == nullptr);

    if (asynch)
        return mapAsset;

    // Wow.  Had a map *finish loading* after the PROGRESS call below displayed
    // the dialog and started processing events but before the qApp->processEvents()
    // call below, resulting in a hang because mWaitingForMapInfo wasn't set till
    // after the PROGRESS call.
    Q_ASSERT(mWaitingForMapInfo == nullptr);
    mWaitingForMapInfo = mapAsset;

    QFileInfo fileInfoMap(mapFilePath);
    PROGRESS progress(tr("Reading %1").arg(fileInfoMap.completeBaseName()));

    noise() << "WAITING FOR MAP" << mapName << "with priority" << priority;
    for (int i = 0; i < mDeferredMaps.size(); i++) {
        MapDeferral md = mDeferredMaps[i];
        if (md.mapAsset == mapAsset) {
            mDeferredMaps.removeAt(i);
            mapLoadedByThread(md.mapAsset);
            break;
        }
    }

    processEventsWhile([&]{ return mapAsset->isEmpty(); });

    mWaitingForMapInfo = nullptr;
    if (mapAsset->map())
        return mapAsset;
    return nullptr;
}

MapAsset *MapManager::newFromMap(Tiled::Map *map, const QString &mapFilePath)
{
    MapAsset* mapAsset = new MapAsset(map, mapFilePath, this);
    mapAsset->mBeingEdited = true;

    if (!mapFilePath.isEmpty())
    {
        Q_ASSERT(!QFileInfo(mapFilePath).isRelative());
        mapAsset->setFilePath(mapFilePath);
//        mMapInfo[mapFilePath] = info;
    }

    // Not adding to the m_assets table because this function is for maps being edited, not lots

    return mapAsset;
}

// FIXME: this map is shared by any CellDocument whose cell has no map specified.
// Adding sub-maps to a cell may add new layers to this shared map.
// If that happens, all CellScenes using this map will need to be updated.
MapAsset *MapManager::getEmptyMap()
{
    QString mapFilePath(QLatin1String("<empty>"));
    if (MapAsset *mapAsset = static_cast<MapAsset*>(get(mapFilePath)))
    {
        return mapAsset;
    }

    Map *map = new Map(Map::LevelIsometric, 300, 300, 64, 32);

    MapAsset* mapAsset = new MapAsset(map, mapFilePath, this);
    mapAsset->setFilePath(mapFilePath);
    m_assets[mapFilePath] = mapAsset;
#ifdef WORLDED
    addReferenceToMap(mapAsset);
#endif
    return mapAsset;
}

MapAsset *MapManager::getPlaceholderMap(const QString &mapName, int width, int height)
{
    QString mapFilePath(mapName);
    MapAsset *mapAsset = static_cast<MapAsset*>(get(mapFilePath));
    if (mapAsset != nullptr && mapAsset->isReady())
    {
         return mapAsset;
    }

    if (width <= 0)
        width = 32;
    if (height <= 0)
        height = 32;
    if (mapAsset == nullptr)
    {
        mapAsset = new MapAsset(mapFilePath, this);
    }
    Map *map = new Map(Map::LevelIsometric, width, height, 64, 32);

    if (mapAsset->mMap != nullptr)
    {
        // FIXME: old map failed to load, release it
        int dbg = 1;
    }

    mapAsset->mMap = map;
    mapAsset->setFilePath(mapFilePath);
    mapAsset->mPlaceholder = true;
    m_assets[mapFilePath] = mapAsset;

    return mapAsset;
}

void MapManager::mapParametersChanged(MapAsset *mapAsset)
{
    Map *map = mapAsset->map();
    Q_ASSERT(map);
    mapAsset->mHeight = map->height();
    mapAsset->mWidth = map->width();
    mapAsset->mTileWidth = map->tileWidth();
    mapAsset->mTileHeight = map->tileHeight();
}

#ifdef WORLDED
void MapManager::addReferenceToMap(MapAsset *mapAsset)
{
    Q_ASSERT(mapAsset->mMap != nullptr);
    if (mapAsset->mMap != nullptr) {
        mapAsset->mMapRefCount++;
        mapAsset->mReferenceEpoch = mReferenceEpoch;
        if (mapAsset->mMapRefCount == 1)
            ++mReferenceEpoch;
        noise() << "MapManager refCount++ =" << mapAsset->mMapRefCount << mapAsset->mFilePath;
    }
}

void MapManager::removeReferenceToMap(MapAsset *mapAsset)
{
    Q_ASSERT(mapAsset->mMap != nullptr);
    if (mapAsset->mMap != nullptr) {
        Q_ASSERT(mapAsset->mMapRefCount > 0);
        mapAsset->mMapRefCount--;
        noise() << "MapManager refCount-- =" << mapAsset->mMapRefCount << mapAsset->mFilePath;
        purgeUnreferencedMaps();
    }
}

void MapManager::purgeUnreferencedMaps()
{
    int unpurged = 0;
    for (Asset* asset : m_assets) {
        MapAsset *mapAsset = static_cast<MapAsset*>(asset);
        bool bigMap = mapAsset->size() == QSize(300, 300);
        if (mapAsset->isEmpty())
            continue;
        if (mapAsset != nullptr && mapAsset->isEmpty())
            continue;
        if ((mapAsset && mapAsset->mMapRefCount <= 0) &&
                ((bigMap && (mapAsset->mReferenceEpoch <= mReferenceEpoch - 10)) ||
                (mapAsset->mReferenceEpoch <= mReferenceEpoch - 50))) {
            noise() << "MapManager purging" << mapAsset->mFilePath;
            enableUnload(true);
            unload(mapAsset);
            enableUnload(false);
        }
        else if (mapAsset && mapAsset->mMapRefCount <= 0) {
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
        MapAsset* mapAsset = static_cast<MapAsset*>(asset);
        if (!mapAsset->mPlaceholder || !mapAsset->mMap)
            continue;
        if (QFileInfo(mapAsset->path()) != QFileInfo(path))
            continue;
        mFileSystemWatcher->addPath(mapAsset->path()); // FIXME: make canonical?
        fileChanged(mapAsset->path());
    }

    emit mapFileCreated(path);
}

void MapManager::processEventsWhile(std::function<bool()> predicate)
{
    IdleTasks::instance().blockTasks(true);
    while (predicate())
    {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        AssetTaskManager::instance().updateAsyncTransactions();
        processDeferrals();
    }
    IdleTasks::instance().blockTasks(false);
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

    MapManagerDeferral deferral;

    foreach (const QString &path, mChangedFiles) {
        if (MapAsset* mapAsset = static_cast<MapAsset*>(get(path))) {
            noise() << "MapManager::fileChanged" << path;
            mFileSystemWatcher->removePath(path);
            QFileInfo info(path);
            if (info.exists()) {
                Q_ASSERT(!mapAsset->isBeingEdited());
                emit mapAboutToChange(mapAsset);
                reload(mapAsset);
                if (mapAsset->mMap == nullptr)
                    mapAsset->mMap = new Map(Map::LevelIsometric, mapAsset->width(), mapAsset->height(), 64, 32);;
                emit mapChanged(mapAsset);
//                mFileSystemWatcher->addPath(path);
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
        MapAsset *mapAsset = static_cast<MapAsset*>(asset);
        if (mapAsset->map() && mapAsset->path().endsWith(QLatin1String(".tbx"))
                && mapAsset->map()->hasUsedMissingTilesets())
        {
            fileChanged(mapAsset->path());
        }
    }
}

void MapManager::metaTilesetRemoved(Tileset *tileset)
{
    for (Asset* asset : m_assets)
    {
        MapAsset *mapAsset = static_cast<MapAsset*>(asset);
        if (mapAsset->map() && mapAsset->path().endsWith(QLatin1String(".tbx"))
                && mapAsset->map()->usedTilesets().contains(tileset))
        {
            fileChanged(mapAsset->path());
        }
    }
}

void MapManager::mapLoadedByThread(MapAsset *mapAsset)
{
    if (mapAsset != mWaitingForMapInfo && mDeferralDepth > 0) {
        noise() << "MAP LOADED BY THREAD - DEFERR" << mapAsset->path();
        mDeferredMaps += MapDeferral(mapAsset);
        return;
    }

    Tiled::Map* map = mapAsset->mLoadedMap;
    mapAsset->mLoadedMap = nullptr;

    noise() << "MAP LOADED BY THREAD" << mapAsset->path();

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

    bool replace = mapAsset->mMap != nullptr;
    if (replace) {
        Q_ASSERT(!mapAsset->isBeingEdited());
        emit mapAboutToChange(mapAsset);
        TilesetManager& tilesetMgr = TilesetManager::instance();
        tilesetMgr.removeReferences(mapAsset->map()->tilesets());
        delete mapAsset->mMap;
    }

    mapAsset->mMap = map;
    mapAsset->mOrientation = map->orientation();
    mapAsset->mHeight = map->height();
    mapAsset->mWidth = map->width();
    mapAsset->mTileWidth = map->tileWidth();
    mapAsset->mTileHeight = map->tileHeight();
    mapAsset->mPlaceholder = false;
//    mapInfo->mLoading = false;

    onLoadingSucceeded(mapAsset);

    if (replace)
        emit mapChanged(mapAsset);

#ifdef WORLDED
    // The reference count is zero, but prevent it being immediately purged.
    // FIXME: add a reference and let the caller deal with it.
    mapAsset->mReferenceEpoch = mReferenceEpoch;
#endif

    emit mapLoaded(mapAsset);
}

void MapManager::buildingLoadedByThread(MapAsset *mapAsset)
{
    MapManagerDeferral deferral;

    Building* building = mapAsset->mLoadedBuilding;
    mapAsset->mLoadedBuilding = nullptr;

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

    mapAsset->mLoadedMap = map;
    mapLoadedByThread(mapAsset);
}

void MapManager::failedToLoadByThread(const QString error, MapAsset *mapAsset)
{
//    mapInfo->mLoading = false;
    mError = error;
    emit mapFailedToLoad(mapAsset);
}

void MapManager::deferThreadResults(bool defer)
{
    if (defer) {
        ++mDeferralDepth;
        noise() << "MapManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
        --mDeferralDepth;
        noise() << "MapManager::deferThreadResults depth-- =" << mDeferralDepth;
    }
}

void MapManager::processDeferrals()
{
    if (mDeferralDepth > 0) {
        noise() << "MapManager::processDeferrals deferred - FML";
        return;
    }
    QList<MapDeferral> deferrals = mDeferredMaps;
    mDeferredMaps.clear();
    for (MapDeferral md : deferrals) {
        mapLoadedByThread(md.mapAsset);
    }
}

class MapManager_MapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath) override
    {
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};

class AssetTask_LoadBuilding : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadBuilding(MapAsset* asset)
        : BaseAsyncAssetTask(asset)
        , mMapAsset(asset)
    {

    }

    void run() override
    {
        QString path = m_asset->getPath().getString();

        BuildingReader reader;
        mBuilding = reader.read(path);
        if (mBuilding == nullptr)
        {
            mError = reader.errorString();
        }
        else
        {
            bLoaded = true;
        }
    }

    void handleResult() override
    {
        MapManager::instance().loadBuildingTaskFinished(this);
    }

    void release() override
    {
        delete mBuilding;
        delete this;
    }

    MapAsset* mMapAsset = nullptr;
    Building* mBuilding = nullptr;
    bool bLoaded = false;
    QString mError;
};

class AssetTask_LoadMap : public BaseAsyncAssetTask
{
public:
    AssetTask_LoadMap(MapAsset* asset)
        : BaseAsyncAssetTask(asset)
        , mMapAsset(asset)
    {

    }

    void run() override
    {
        QString path = m_asset->getPath().getString();

        MapManager_MapReader reader;
//        reader.setTilesetImageCache(TilesetManager::instance().imageCache()); // not thread-safe class
        mMap = reader.readMap(path);
        if (mMap == nullptr)
        {
            mError = reader.errorString();
        }
        else
        {
            bLoaded = true;
        }
    }

    void handleResult() override
    {
        MapManager::instance().loadMapTaskFinished(this);
    }

    void release() override
    {
        delete mMap;
        delete this;
    }

    MapAsset* mMapAsset = nullptr;
    Tiled::Map* mMap = nullptr;
    bool bLoaded = false;
    QString mError;
};

void MapManager::loadBuildingTaskFinished(AssetTask_LoadBuilding *task)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(task->m_asset);
    if (task->bLoaded)
    {
        mapAsset->mLoadedBuilding = task->mBuilding;
        task->mBuilding = nullptr;
        mFileSystemWatcher->addPath(task->m_asset->getPath().getString());
        buildingLoadedByThread(task->mMapAsset);
        // Could be deferred
//        onLoadingSucceeded(task->m_asset);
    }
    else
    {
        onLoadingFailed(task->m_asset);
    }
}

void MapManager::loadMapTaskFinished(AssetTask_LoadMap *task)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(task->m_asset);
    if (task->bLoaded)
    {
        mapAsset->mLoadedMap = task->mMap;
        task->mMap = nullptr;
        mFileSystemWatcher->addPath(task->m_asset->getPath().getString());
        mapLoadedByThread(task->mMapAsset);
        // Could be deferred
//        onLoadingSucceeded(task->m_asset);
    }
    else
    {
        // TODO: handle task->mError
        onLoadingFailed(task->m_asset);
    }
}

Asset *MapManager::createAsset(AssetPath path, AssetParams *params)
{
    return new MapAsset(path, this/*, params*/);
}

void MapManager::destroyAsset(Asset *asset)
{
    Q_UNUSED(asset);
}

void MapManager::startLoading(Asset *asset)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(asset);

    QString path = asset->getPath().getString();
    if (path.endsWith(QLatin1String(".tbx")))
    {
        AssetTask_LoadBuilding* assetTask = new AssetTask_LoadBuilding(mapAsset);
        setTask(asset, assetTask);
        AssetTaskManager::instance().submit(assetTask);
    }
    else
    {
        AssetTask_LoadMap* assetTask = new AssetTask_LoadMap(mapAsset);
        setTask(asset, assetTask);
        AssetTaskManager::instance().submit(assetTask);
    }
}

void MapManager::unloadData(Asset *asset)
{
    MapAsset* mapAsset = static_cast<MapAsset*>(asset);

    if (mapAsset->mMap != nullptr)
    {
        TilesetManager& tilesetMgr = TilesetManager::instance();
        tilesetMgr.removeReferences(mapAsset->mMap->tilesets());
        delete mapAsset->mMap;
        mapAsset->mMap = nullptr;
    }
}
