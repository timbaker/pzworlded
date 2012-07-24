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

#include "map.h"
#include "mapreader.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Tiled;

extern TilesetImageCache *gTilesetImageCache;

MapManager *MapManager::mInstance = NULL;

MapManager *MapManager::instance()
{
    if (!mInstance)
        mInstance = new MapManager;
    return mInstance;
}

MapManager::MapManager()
{
}

// mapName could be "Lot_Foo", "../Lot_Foo", "C:/maptools/Lot_Foo" with/without ".tmx"
QString MapManager::pathForMap(const QString &mapName, const QString &relativeTo)
{
#if 1
    if (!QDir::isRelativePath(mapName)) {
        QString mapFilePath = mapName;
        if (!mapFilePath.endsWith(QLatin1String(".tmx")))
            mapFilePath += QLatin1String(".tmx");
        QFileInfo fileInfo(mapFilePath);
        if (fileInfo.exists())
            return fileInfo.canonicalFilePath();
        return QString();
    }

    Preferences *prefs = Preferences::instance();
    QStringList searchPaths = prefs->searchPaths();
    if (!relativeTo.isEmpty())
        searchPaths.insert(0, relativeTo);

    foreach (QString searchPath, searchPaths) {
        QString mapFilePath = mapName;

        Q_ASSERT(!searchPath.isEmpty());
        Q_ASSERT(!QDir::isRelativePath(searchPath));
        mapFilePath = searchPath + QLatin1Char('/') + mapName;

        if (!mapFilePath.endsWith(QLatin1String(".tmx")))
            mapFilePath += QLatin1String(".tmx");

        QFileInfo fileInfo(mapFilePath);
        if (fileInfo.exists())
            return fileInfo.canonicalFilePath();
    }

    return QString();
#else
    QString mapFilePath = mapName;
    QString relativeTo = _relativeTo;

    QDir mapsDirectory(Preferences::instance()->mapsDirectory());
    if (relativeTo.isEmpty() && mapsDirectory.exists())
        relativeTo = mapsDirectory.canonicalPath();

    if (QDir::isRelativePath(mapName)) {
        Q_ASSERT(!relativeTo.isEmpty());
        Q_ASSERT(!QDir::isRelativePath(relativeTo));
        mapFilePath = relativeTo + QLatin1Char('/') + mapName;
    }

    if (!mapFilePath.endsWith(QLatin1String(".tmx")))
        mapFilePath += QLatin1String(".tmx");

    QFileInfo fileInfo(mapFilePath);

    if (fileInfo.exists())
        return fileInfo.canonicalFilePath();

    if (mapsDirectory.exists() && (relativeTo != mapsDirectory.canonicalPath()))
        return pathForMap(mapName, mapsDirectory.canonicalPath());

    return QString();
#endif
}

#include <QApplication>
class EditorMapReader : public MapReader
{
protected:
    /**
     * Overridden to make sure the resolved reference is canonical.
     */
    QString resolveReference(const QString &reference, const QString &mapPath)
    {
#if 1
        Preferences *prefs = Preferences::instance();
        QStringList paths(mapPath);
        paths += prefs->searchPaths();
        foreach (QString path, paths) {
            QString resolved = MapReader::resolveReference(reference, path);
            QFileInfo info(resolved);
            if (info.exists())
                return info.canonicalFilePath();
        }
        return MapReader::resolveReference(reference, mapPath);
#endif
        QString resolved = MapReader::resolveReference(reference, mapPath);
        QString canonical = QFileInfo(resolved).canonicalFilePath();

        // Hack to keep the app responsive.
        // TODO: Move map reading to a worker thread. Only issue is tileset images
        // cannot be accessed outside the GUI thread.
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

        // Make sure that we're not returning an empty string when the file is
        // not found.
        return canonical.isEmpty() ? resolved : canonical;
    }
};

#ifdef THREADIT
#include <QThread>

class MapReaderThread : public QThread
{
public:
    MapReaderThread(const QString &mapFilePath)
        : mMapFilePath(mapFilePath)
        , mMap(0)
    {
    }

    void run()
    {
        EditorMapReader reader;
        reader.setTilesetImageCache(gTilesetImageCache);
        mMap = reader.readMap(mMapFilePath);
    }

    QString mMapFilePath;
    Map *mMap;
};
#endif

MapInfo *MapManager::loadMap(const QString &mapName, Map::Orientation orient, const QString &relativeTo)
{
    QString mapFilePath = pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty())
        return 0;

    if (mMapInfo.contains(mapFilePath) && mMapInfo[mapFilePath]->map()) {
convertOrientation:
        MapInfo *mapInfo = mMapInfo[mapFilePath];
        if (orient != Map::Unknown && mapInfo->orientation() != orient) {
            if (!mapInfo->mConverted) {
                qDebug() << "converting" << mapFilePath << "to" << orientationToString(orient);
                Map *newMap = convertOrientation(mapInfo->map(), orient);
                MapInfo *newInfo = new MapInfo(orient, newMap->width(), newMap->height(),
                                               newMap->tileWidth(), newMap->tileHeight());
                newInfo->setFilePath(mapFilePath);
                newInfo->mMap = newMap;
                newInfo->mConverted = mapInfo;
                mapInfo->mConverted = newInfo;
            }
            mapInfo = mapInfo->mConverted;
        }
        return mapInfo;
    }

    QFileInfo fileInfoMap(mapFilePath);

    PROGRESS progress(tr("Reading %1").arg(fileInfoMap.completeBaseName()));

#ifdef THREADIT
    MapReaderThread thread(mapFilePath);
    thread.start();
    while (thread.isRunning()) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    Map *map = thread.mMap;
#else
    EditorMapReader reader;
    reader.setTilesetImageCache(gTilesetImageCache);
    Map *map = reader.readMap(mapFilePath);
#endif
    if (!map)
        return 0; // TODO: Add error handling

    if (!mMapInfo.contains(mapFilePath)) {
        MapInfo *info = new MapInfo(map->orientation(), map->width(), map->height(),
                                    map->tileWidth(), map->tileHeight());
        info->setFilePath(mapFilePath);
        mMapInfo[mapFilePath] = info;
    }

    mMapInfo[mapFilePath]->mMap = map;

    if (orient != Map::Unknown && map->orientation() != orient)
        goto convertOrientation;

    return mMapInfo[mapFilePath];
}

MapInfo *MapManager::newFromMap(Map *map, const QString &mapFilePath)
{
    MapInfo *info = new MapInfo(map->orientation(), map->width(), map->height(),
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
    bool openFile(QFile *file)
    {
        if (!file->exists()) {
            mError = tr("File not found: %1").arg(file->fileName());
            return false;
        } else if (!file->open(QFile::ReadOnly | QFile::Text)) {
            mError = tr("Unable to read file: %1").arg(file->fileName());
            return false;
        }

        return true;
    }

    MapInfo *readMap(const QString &mapFilePath)
    {
        QFile file(mapFilePath);
        if (!openFile(&file))
            return NULL;

        return readMap(&file, QFileInfo(mapFilePath).absolutePath());
    }

    MapInfo *readMap(QIODevice *device, const QString &path)
    {
        Q_UNUSED(path)

        mError.clear();
        mMapInfo = NULL;

        xml.setDevice(device);

        if (xml.readNextStartElement() && xml.name() == "map") {
            mMapInfo = readMap();
        } else {
            xml.raiseError(tr("Not a map file."));
        }

        return mMapInfo;
    }

    MapInfo *readMap()
    {
        Q_ASSERT(xml.isStartElement() && xml.name() == "map");

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

    QXmlStreamReader xml;
    MapInfo *mMapInfo;
    QString mError;
};

MapInfo *MapManager::mapInfo(const QString &mapFilePath, Map::Orientation orient)
{
    if (mMapInfo.contains(mapFilePath)) {
        MapInfo *mapInfo = mMapInfo[mapFilePath];
        if (orient != Map::Unknown && orient != mapInfo->orientation())
            mapInfo = loadMap(mapFilePath, orient);
        return mapInfo;
    }

    QFileInfo fileInfo(mapFilePath);
    if (!fileInfo.exists())
        return NULL;

    MapInfoReader reader;
    MapInfo *mapInfo = reader.readMap(mapFilePath);
    if (!mapInfo)
        return NULL;

    qDebug() << "read map info for" << mapFilePath;
    mapInfo->setFilePath(mapFilePath);

    mMapInfo[mapFilePath] = mapInfo;
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
    Map *map = new Map(mapInfo->orientation(), mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

#if 0
    for (int level = 0; level < 1; level++) {
        TileLayer *tl = new TileLayer(tr("%1_Layer").arg(level), 0, 0, 300, 300);
        tl->setLevel(level);
        map->addLayer(tl);
    }
#endif

    mapInfo->mMap = map;
    mapInfo->setFilePath(mapFilePath);
    mMapInfo[mapFilePath] = mapInfo;

    return mapInfo;
}

MapInfo *MapManager::getPlaceholderMap(const QString &mapName, Map::Orientation orient, int width, int height)
{
    if (orient == Map::Unknown)
        orient = Map::LevelIsometric;

    QString mapFilePath(mapName);
    if (mMapInfo.contains(mapFilePath)) {
        MapInfo *mapInfo = mMapInfo[mapFilePath];
        if (mapInfo->orientation() != orient) {
            if (!mapInfo->mConverted) {
                qDebug() << "converting" << mapFilePath << "to" << orientationToString(orient);
                Map *newMap = convertOrientation(mapInfo->map(), orient);
                MapInfo *newInfo = new MapInfo(orient, newMap->width(), newMap->height(),
                                               newMap->tileWidth(), newMap->tileHeight());
                newInfo->setFilePath(mapFilePath);
                newInfo->mPlaceholder = true;
                newInfo->mMap = newMap;
                newInfo->mConverted = mapInfo;
                mapInfo->mConverted = newInfo;
            }
            return mapInfo->mConverted;
        }
        return mapInfo;
    }

    if (width <= 0) width = 32;
    if (height <= 0) height = 32;
    MapInfo *mapInfo = new MapInfo(orient, width, height, 64, 32);
    Map *map = new Map(mapInfo->orientation(), mapInfo->width(), mapInfo->height(),
                       mapInfo->tileWidth(), mapInfo->tileHeight());

#if 0
    for (int level = 0; level < 1; level++) {
        TileLayer *tl = new TileLayer(tr("%1_Layer").arg(level), 0, 0, 300, 300);
        tl->setLevel(level);
        map->addLayer(tl);
    }
#endif

    mapInfo->mMap = map;
    mapInfo->setFilePath(mapFilePath);
    mapInfo->mPlaceholder = true;
    mMapInfo[mapFilePath] = mapInfo;

    return mapInfo;
}

Map *MapManager::convertOrientation(Map *map, Tiled::Map::Orientation orient)
{
    Map::Orientation orient0 = map->orientation();
    Map::Orientation orient1 = orient;

    if (orient0 != orient1) {
        Map *newMap = new Map(orient1, map->width(), map->height(), map->tileWidth(), map->tileHeight());
        QPoint offset(3, 3);
        if (orient0 == Map::Isometric && orient1 == Map::LevelIsometric) {
            foreach (Layer *layer, map->layers()) {
                Layer *newLayer = layer->clone();
                int level;
                if (MapComposite::levelForLayer(layer, &level) && level > 0)
                    newLayer->offset(offset * level, layer->bounds(), false, false);
                newMap->addLayer(newLayer);
            }
        }
        if (orient0 == Map::LevelIsometric && orient1 == Map::Isometric) {
            foreach (Layer *layer, map->layers()) {
                Layer *newLayer = layer->clone();
                int level;
                if (MapComposite::levelForLayer(layer, &level) && level > 0)
                    newLayer->setPosition(-offset * level);
                newMap->addLayer(newLayer);
            }
        }
        map = newMap;
    }

    return map;
}
