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

#include "mapcomposite.h"
#include "preferences.h"
#include "progress.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

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

MapManager *MapManager::mInstance = nullptr;

MapManager *MapManager::instance()
{
    if (!mInstance)
        mInstance = new MapManager;
    return mInstance;
}

void MapManager::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

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
    connect(mFileSystemWatcher, &FileSystemWatcher::fileChanged,
            this, &MapManager::fileChanged);

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, &QTimer::timeout,
            this, &MapManager::fileChangedTimeout);

    qRegisterMetaType<MapInfo*>("BuildingEditor::Building*");
    qRegisterMetaType<MapInfo*>("MapInfo*");

    mMapReaderThread.resize(4);
    mMapReaderWorker.resize(mMapReaderThread.size());
    for (int i = 0; i < mMapReaderThread.size(); i++) {
        mMapReaderThread[i] = new InterruptibleThread;
        mMapReaderWorker[i] = new MapReaderWorker(mMapReaderThread[i], i);
        mMapReaderWorker[i]->moveToThread(mMapReaderThread[i]);
        connect(mMapReaderWorker[i], qOverload<Map*,MapInfo*>(&MapReaderWorker::loaded),
                this, &MapManager::mapLoadedByThread);
        connect(mMapReaderWorker[i], qOverload<BuildingEditor::Building*,MapInfo*>(&MapReaderWorker::loaded),
                this, &MapManager::buildingLoadedByThread);
        connect(mMapReaderWorker[i], &MapReaderWorker::failedToLoad,
                this, &MapManager::failedToLoadByThread);
        mMapReaderThread[i]->start();
    }

    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded,
            this, &MapManager::metaTilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved,
            this, &MapManager::metaTilesetRemoved);
}

MapManager::~MapManager()
{
    for (int i = 0; i < mMapReaderThread.size(); i++) {
        mMapReaderThread[i]->interrupt(); // stop the long-running task
        mMapReaderThread[i]->quit(); // exit the event loop
        mMapReaderThread[i]->wait(); // wait for thread to terminate
        delete mMapReaderThread[i];
        delete mMapReaderWorker[i];
    }

    TilesetManager *tilesetManager = TilesetManager::instance();

    const QMap<QString,MapInfo*>::const_iterator end = mMapInfo.constEnd();
    QMap<QString,MapInfo*>::const_iterator it = mMapInfo.constBegin();
    while (it != end) {
        MapInfo *mapInfo = it.value();
        if (Map *map = mapInfo->map()) {
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

    if (mMapInfo.contains(mapFilePath) && mMapInfo[mapFilePath]->map()) {
        return mMapInfo[mapFilePath];
    }

    QFileInfo fileInfoMap(mapFilePath);

    MapInfo *mapInfo = this->mapInfo(mapFilePath);
    if (!mapInfo)
        return nullptr;
    if (mapInfo->mLoading) {
        foreach (MapReaderWorker *w, mMapReaderWorker)
            QMetaObject::invokeMethod(w, "possiblyRaisePriority",
                                      Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                                      Q_ARG(int,priority));
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
            while (mapInfo->mLoading) {
                qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
            }
            mWaitingForMapInfo = nullptr;
            if (!mapInfo->map())
                return nullptr;
        }
        return mapInfo;
    }
    mapInfo->mLoading = true;
    QMetaObject::invokeMethod(mMapReaderWorker[mNextThreadForJob], "addJob",
                              Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                              Q_ARG(int,priority));
    mNextThreadForJob = (mNextThreadForJob + 1) % mMapReaderThread.size();

    if (asynch)
        return mapInfo;

    // Wow.  Had a map *finish loading* after the PROGRESS call below displayed
    // the dialog and started processing events but before the qApp->processEvents()
    // call below, resulting in a hang because mWaitingForMapInfo wasn't set until
    // after the PROGRESS call.
    Q_ASSERT(mWaitingForMapInfo == nullptr);
    mWaitingForMapInfo = mapInfo;

    PROGRESS progress(tr("Reading %1").arg(fileInfoMap.completeBaseName()));
    foreach (MapReaderWorker *w, mMapReaderWorker)
        QMetaObject::invokeMethod(w, "possiblyRaisePriority",
                                  Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                                  Q_ARG(int,priority));
    noise() << "WAITING FOR MAP" << mapName << "with priority" << priority;
    for (int i = 0; i < mDeferredMaps.size(); i++) {
        MapDeferral md = mDeferredMaps[i];
        if (md.mapInfo == mapInfo) {
            mDeferredMaps.removeAt(i);
            mapLoadedByThread(md.map, md.mapInfo);
            break;
        }
    }
    while (mapInfo->mLoading) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mWaitingForMapInfo = nullptr;
    if (mapInfo->map())
        return mapInfo;
    return nullptr;
}

MapInfo *MapManager::newFromMap(Map *map, const QString &mapFilePath)
{
    MapInfo *info = new MapInfo(map->orientation(),
                                map->width(), map->height(),
                                map->tileWidth(), map->tileHeight());
    info->mMap = map;
    info->mBeingEdited = true;

    if (!mapFilePath.isEmpty()) {
        Q_ASSERT(!QFileInfo(mapFilePath).isRelative());
        info->setFilePath(mapFilePath);
//        mMapInfo[mapFilePath] = info;
    }

    // Not adding to the mMapInfo table because this function is for maps being edited, not lots

    return info;
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
            return NULL;

        if (mapFilePath.endsWith(QLatin1String(".tbx")))
            return readBuilding(&file, QFileInfo(mapFilePath).absolutePath());

        return readMap(&file, QFileInfo(mapFilePath).absolutePath());
    }

    MapInfo *readMap(QIODevice *device, const QString &path)
    {
        Q_UNUSED(path)

        mError.clear();
        mMapInfo = NULL;

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
        mMapInfo = NULL;

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

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("properties")) {
                mMapInfo->setProperties(readProperties());
                break;
            } else {
                readUnknownElement();
            }
        }

        return mMapInfo;
    }

    Tiled::Properties readProperties()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("properties"));

        Tiled::Properties properties;

        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("property")) {
                readProperty(&properties);
            } else {
                readUnknownElement();
            }
        }

        return properties;
    }

    void readProperty(Tiled::Properties *properties)
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("property"));

        const QXmlStreamAttributes atts = xml.attributes();
        QString propertyName = atts.value(QLatin1String("name")).toString();
        QString propertyValue = atts.value(QLatin1String("value")).toString();

        while (xml.readNext() != QXmlStreamReader::Invalid) {
            if (xml.isEndElement()) {
                break;
            }
            if (xml.isCharacters() && !xml.isWhitespace()) {
                if (propertyValue.isEmpty()) {
                    propertyValue = xml.text().toString();
                }
            } else if (xml.isStartElement()) {
                readUnknownElement();
            }
        }

        properties->insert(propertyName, propertyValue);
    }

    void readUnknownElement()
    {
        qDebug() << "Unknown element (fixme):" << xml.name();
        xml.skipCurrentElement();
    }

    QXmlStreamReader xml;
    MapInfo *mMapInfo;
    QString mError;
};

MapInfo *MapManager::mapInfo(const QString &mapFilePath)
{
    if (mMapInfo.contains(mapFilePath))
       return mMapInfo[mapFilePath];

    QFileInfo fileInfo(mapFilePath);
    if (!fileInfo.exists())
        return NULL;

    MapInfoReader reader;
    MapInfo *mapInfo = reader.readMap(mapFilePath);
    if (!mapInfo)
        return NULL;

    noise() << "read map info for" << mapFilePath;
    mapInfo->setFilePath(mapFilePath);

    mMapInfo[mapFilePath] = mapInfo;
    mFileSystemWatcher->addPath(mapFilePath);

    return mapInfo;
}

// FIXME: this map is shared by any CellDocument whose cell has no map specified.
// Adding sub-maps to a cell may add new layers to this shared map.
// If that happens, all CellScenes using this map will need to be updated.
MapInfo *MapManager::getEmptyMap()
{
    QString mapFilePath(QLatin1String("<empty>"));
    if (mMapInfo.contains(mapFilePath))
        return mMapInfo[mapFilePath];

    MapInfo *mapInfo = new MapInfo(Map::LevelIsometric, 300, 300, 64, 32);
    Map *map = new Map(mapInfo->orientation(),
                       mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

    mapInfo->mMap = map;
    mapInfo->setFilePath(mapFilePath);
    mMapInfo[mapFilePath] = mapInfo;
#ifdef WORLDED
    addReferenceToMap(mapInfo);
#endif
    return mapInfo;
}

MapInfo *MapManager::getPlaceholderMap(const QString &mapName, int width, int height)
{
    QString mapFilePath(mapName);
    if (mMapInfo.contains(mapFilePath) && mMapInfo[mapFilePath]->mMap) {
        return mMapInfo[mapFilePath];
    }

    if (width <= 0) width = 32;
    if (height <= 0) height = 32;
    MapInfo *mapInfo = mMapInfo[mapFilePath];
    if (!mapInfo)
        mapInfo = new MapInfo(Map::LevelIsometric, width, height, 64, 32);
    Map *map = new Map(mapInfo->orientation(), mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

    mapInfo->mMap = map;
    mapInfo->setFilePath(mapFilePath);
    mapInfo->mPlaceholder = true;
    mMapInfo[mapFilePath] = mapInfo;

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
    Q_ASSERT(mapInfo->mMap != 0);
    if (mapInfo->mMap) {
        mapInfo->mMapRefCount++;
        mapInfo->mReferenceEpoch = mReferenceEpoch;
        if (mapInfo->mMapRefCount == 1)
            ++mReferenceEpoch;
        noise() << "MapManager refCount++ =" << mapInfo->mMapRefCount << mapInfo->mFilePath;
    }
}

void MapManager::removeReferenceToMap(MapInfo *mapInfo)
{
    Q_ASSERT(mapInfo->mMap != 0);
    if (mapInfo->mMap) {
        Q_ASSERT(mapInfo->mMapRefCount > 0);
        mapInfo->mMapRefCount--;
        noise() << "MapManager refCount-- =" << mapInfo->mMapRefCount << mapInfo->mFilePath;
        purgeUnreferencedMaps();
    }
}

void MapManager::purgeUnreferencedMaps()
{
    int unpurged = 0;
    foreach (MapInfo *mapInfo, mMapInfo) {
        bool bigMap = mapInfo->size() == QSize(300, 300);
        if ((mapInfo->mMap && mapInfo->mMapRefCount <= 0) &&
                ((bigMap && (mapInfo->mReferenceEpoch <= mReferenceEpoch - 10)) ||
                (mapInfo->mReferenceEpoch <= mReferenceEpoch - 50))) {
            noise() << "MapManager purging" << mapInfo->mFilePath;
            TilesetManager *tilesetMgr = TilesetManager::instance();
            tilesetMgr->removeReferences(mapInfo->mMap->tilesets());
            delete mapInfo->mMap;
            mapInfo->mMap = 0;
        }
        else if (mapInfo->mMap && mapInfo->mMapRefCount <= 0)
            unpurged++;
    }
    if (unpurged) noise() << "MapManager unpurged =" << unpurged;
}

void MapManager::newMapFileCreated(const QString &path)
{
    // If a cell view is open with a placeholder map and that map now exists,
    // read the new map and allow the cell-scene to update itself.
    // This code is 90% the same as fileChangedTimeout().
    foreach (MapInfo *mapInfo, mMapInfo) {
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

Map *MapManager::convertOrientation(Map *map, Tiled::Map::Orientation orient)
{
    Map::Orientation orient0 = map->orientation();
    Map::Orientation orient1 = orient;

    if (orient0 != orient1) {
        Map *newMap = map->clone();
        newMap->setOrientation(orient);
        QPoint offset(3, 3);
        if (orient0 == Map::Isometric && orient1 == Map::LevelIsometric) {
            foreach (Layer *layer, newMap->layers()) {
                int level;
                if (MapComposite::levelForLayer(layer, &level) && level > 0)
                    layer->offset(offset * level, layer->bounds(), false, false);
            }
        }
        if (orient0 == Map::LevelIsometric && orient1 == Map::Isometric) {
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
        TilesetManager *tilesetManager = TilesetManager::instance();
        tilesetManager->addReferences(newMap->tilesets());
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
        if (mMapInfo.contains(path)) {
            noise() << "MapManager::fileChanged" << path;
            mFileSystemWatcher->removePath(path);
            QFileInfo info(path);
            if (info.exists()) {
                mFileSystemWatcher->addPath(path);
                MapInfo *mapInfo = mMapInfo[path];
                if (mapInfo->map()) {
                    Q_ASSERT(!mapInfo->isBeingEdited());
                    if (!mapInfo->isLoading()) {
                        mapInfo->mLoading = true; // FIXME: seems weird to change this for a loaded map
                        QMetaObject::invokeMethod(mMapReaderWorker[mNextThreadForJob], "addJob",
                                                  Qt::QueuedConnection, Q_ARG(MapInfo*,mapInfo),
                                                  Q_ARG(int,PriorityLow));
                        mNextThreadForJob = (mNextThreadForJob + 1) % mMapReaderThread.size();
                    }
                }
                {
                    MapInfoReader reader;
                    if (MapInfo *mapInfo2 = reader.readMap(path)) {
                        mapInfo->properties() = mapInfo2->properties();
                        delete mapInfo2;
                    }
                }
                emit mapFileChanged(mapInfo);
            }
        }
    }

    mChangedFiles.clear();
}

void MapManager::metaTilesetAdded(Tileset *tileset)
{
    Q_UNUSED(tileset)
    foreach (MapInfo *mapInfo, mMapInfo) {
        if (mapInfo->map() && mapInfo->path().endsWith(QLatin1String(".tbx"))
                && mapInfo->map()->hasUsedMissingTilesets())
            fileChanged(mapInfo->path());
    }
}

void MapManager::metaTilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    foreach (MapInfo *mapInfo, mMapInfo) {
        if (mapInfo->map() && mapInfo->path().endsWith(QLatin1String(".tbx"))
                && mapInfo->map()->usedTilesets().contains(tileset))
            fileChanged(mapInfo->path());
    }
}

void MapManager::mapLoadedByThread(Map *map, MapInfo *mapInfo)
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

    Tile *missingTile = TilesetManager::instance()->missingTile();
    foreach (Tileset *tileset, map->missingTilesets()) {
        if (tileset == missingTile->tileset())
            continue;
        if (tileset->tileHeight() == missingTile->height() && tileset->tileWidth() == missingTile->width()) {
            // Replace the all-red image with something nicer.
            for (int i = 0; i < tileset->tileCount(); i++)
                tileset->tileAt(i)->setImage(missingTile);
        }
    }
    TilesetManager::instance()->addReferences(map->tilesets());

    bool replace = mapInfo->mMap != 0;
    if (replace) {
        Q_ASSERT(!mapInfo->isBeingEdited());
        emit mapAboutToChange(mapInfo);
        TilesetManager *tilesetMgr = TilesetManager::instance();
        tilesetMgr->removeReferences(mapInfo->mMap->tilesets());
        delete mapInfo->mMap;
    }

    mapInfo->mMap = map;
    mapInfo->mOrientation = map->orientation();
    mapInfo->mHeight = map->height();
    mapInfo->mWidth = map->width();
    mapInfo->mTileWidth = map->tileWidth();
    mapInfo->mTileHeight = map->tileHeight();
    mapInfo->mPlaceholder = false;
    mapInfo->mLoading = false;

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

#if 0
    map->setProperty(QStringLiteral("Legend"), QStringLiteral("CommunityServices"));
    map->setProperty(QStringLiteral("File"), QFileInfo(mapInfo->path()).fileName());
#else
    map->setProperties(building->properties());
#endif

    delete building;

    QSet<Tileset*> usedTilesets = map->usedTilesets();
    usedTilesets.remove(TilesetManager::instance()->missingTileset());

    TileMetaInfoMgr::instance()->loadTilesets({ usedTilesets.begin(), usedTilesets.end() });

    // The map references TileMetaInfoMgr's tilesets, but we add a reference
    // to them ourself below.
    TilesetManager::instance()->removeReferences(map->tilesets());

    mapLoadedByThread(map, mapInfo);
}

void MapManager::failedToLoadByThread(const QString error, MapInfo *mapInfo)
{
    mapInfo->mLoading = false;
    mError = error;
    emit mapFailedToLoad(mapInfo);
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
    foreach (MapDeferral md, deferrals)
        mapLoadedByThread(md.map, md.mapInfo);
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
//    reader.setTilesetImageCache(TilesetManager::instance()->imageCache()); // not thread-safe class
    Map *map = reader.readMap(mapInfo->path());
    if (!map)
        mError = reader.errorString();
    return map;
}

Building *MapReaderWorker::loadBuilding(MapInfo *mapInfo)
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
