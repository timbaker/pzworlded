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

#include "buildingtmx.h"

#include "building.h"
#ifndef WORLDED
#include "buildingeditorwindow.h"
#endif
#include "buildingfloor.h"
#include "buildingmap.h"
#ifndef WORLDED
#include "buildingpreferences.h"
#endif
#include "buildingtemplates.h"
#include "simplefile.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "map.h"
#include "maplevel.h"
#include "mapobject.h"
#include "mapwriter.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tile.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDebug>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "TMXConfig.txt";

BuildingTMX *BuildingTMX::mInstance = nullptr;

BuildingTMX *BuildingTMX::instance()
{
    if (!mInstance)
        mInstance = new BuildingTMX;
    return mInstance;
}

void BuildingTMX::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

BuildingTMX::BuildingTMX()
{
}

QStringList BuildingTMX::tileLayerNamesForLevel(int level)
{
    QStringList ret;
    foreach (LayerInfo layerInfo, mLayers) {
        if (layerInfo.mType != LayerInfo::Tile)
            continue;
        QString layerName = layerInfo.mName;
        int level2;
        if (MapComposite::levelForLayer(layerName, &level2)) {
            if (level2 != level)
                continue;
            layerName = MapComposite::layerNameWithoutPrefix(layerName);
        }
        ret += layerName;
    }
    return ret;
}

bool BuildingTMX::exportTMX(Building *building, const QString &fileName)
{
#ifdef WORLDED
    Q_UNUSED(building)
    Q_UNUSED(fileName)
    return false;
#else
    BuildingMap bmap(building);

    Map *map = bmap.mergedMap();

    if (map->orientation() == Map::LevelIsometric) {
        if (!BuildingPreferences::instance()->levelIsometric()) {
            Map *isoMap = MapManager::instance()->convertOrientation(map, Map::Isometric);
            TilesetManager::instance()->removeReferences(map->tilesets());
            delete map;
            map = isoMap;
        }
    }

    for (BuildingFloor *floor : qAsConst(building->floors())) {

        // The given map has layers required by the editor, i.e., Floors, Walls,
        // Doors, etc.  The TMXConfig.txt file may specify extra layer names.
        // So we need to insert any extra layers in the order specified in
        // TMXConfig.txt.  If the layer name has a N_ prefix, it is only added
        // to level N, otherwise it is added to every level.  Object layers are
        // added above *all* the tile layers in the map.
        int previousExistingLayer = -1;
        for (const LayerInfo &layerInfo : qAsConst(mLayers)) {
            QString layerName = layerInfo.mName;
            int level;
            if (MapComposite::levelForLayer(layerName, &level)) {
                if (level != floor->level())
                    continue;
                layerName = MapComposite::layerNameWithoutPrefix(layerName);
            } else {
//                layerName = tr("%1_%2").arg(floor->level()).arg(layerName);
            }
            MapLevel *mapLevel = map->levelAt(level);
            int n;
            if ((n = mapLevel->indexOfLayer(layerName)) >= 0) {
                previousExistingLayer = n;
                continue;
            }
            if (layerInfo.mType == LayerInfo::Tile) {
                TileLayer *tl = new TileLayer(layerName, 0, 0,
                                              map->width(), map->height());
                tl->setLevel(level);
                if (previousExistingLayer < 0)
                    previousExistingLayer = 0;
                mapLevel->insertLayer(previousExistingLayer + 1, tl);
                previousExistingLayer++;
            } else {
                ObjectGroup *og = new ObjectGroup(layerName,
                                                  0, 0, map->width(), map->height());
                map->addLayer(og);
            }
        }
#if 1
        bmap.addRoomDefObjects(map, floor);
#else
        ObjectGroup *objectGroup = new ObjectGroup(tr("%1_RoomDefs").arg(floor->level()),
                                                   0, 0, map->width(), map->height());

        // Add the RoomDefs layer above all the tile layers
        map->addLayer(objectGroup);

        int delta = (building->floorCount() - 1 - floor->level()) * 3;
        if (map->orientation() == Map::LevelIsometric)
            delta = 0;
        QPoint offset(delta, delta);
        int roomID = 1;
        foreach (Room *room, building->rooms()) {
            foreach (QRect rect, floor->roomRegion(room)) {
                QString name = room->internalName + QLatin1Char('#')
                        + QString::number(roomID);
                MapObject *mapObject = new MapObject(name, QLatin1String("room"),
                                                     rect.topLeft() + offset,
                                                     rect.size());
                objectGroup->addObject(mapObject);
            }
            ++roomID;
        }
#endif
    }

    TmxMapWriter writer;
    bool ok = writer.write(map, fileName);
    if (!ok)
        mError = writer.errorString();
    TilesetManager::instance()->removeReferences(map->tilesets());
    delete map;
    return ok;
#endif // WORLDED
}

QString BuildingTMX::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTMX::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

#define VERSION0 0

// VERSION1
// Move tilesets block to Tilesets.txt
#define VERSION1 1
// Renamed some layers
#define VERSION2 2
#define VERSION_LATEST VERSION2

bool BuildingTMX::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

#ifndef WORLDED
    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;
#endif

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    for (const SimpleFileBlock &block : simple.blocks) {
        if (block.name == QLatin1String("layers")) {
            for (const SimpleFileKeyValue &kv : block.values) {
                if (kv.name == QLatin1String("tile")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Tile);
                } else if (kv.name == QLatin1String("object")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Object);
                } else {
                    mError = tr("Unknown layer type '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    // Check that TMXConfig.txt contains all the required tile layers.
    const QStringList requiredLayerNames = BuildingMap::requiredLayerNames();
    for (const QString &layerName : requiredLayerNames) {
        bool match = false;
        for (const LayerInfo &layerInfo : qAsConst(mLayers)) {
            if (layerInfo.mType == LayerInfo::Tile && layerInfo.mName == layerName) {
                match = true;
                break;
            }
        }
        if (!match) {
            mError = tr("TMXConfig.txt is missing a required tile layer: %1")
                    .arg(layerName);
            return false;
        }
    }

    return true;
}

bool BuildingTMX::writeTxt()
{
#ifdef WORLDED
    return false;
#endif
    SimpleFile simpleFile;

    SimpleFileBlock layersBlock;
    layersBlock.name = QLatin1String("layers");
    foreach (LayerInfo layerInfo, mLayers) {
        QString key;
        switch (layerInfo.mType) {
        case LayerInfo::Tile: key = QLatin1String("tile"); break;
        case LayerInfo::Object: key = QLatin1String("object"); break;
        }
        layersBlock.values += SimpleFileKeyValue(key, layerInfo.mName);
    }
    simpleFile.blocks += layersBlock;

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTMX::upgradeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    int userVersion = userFile.version(); // may be zero for unversioned file
    if (userVersion == VERSION_LATEST)
        return true;

    if (userVersion > VERSION_LATEST) {
        mError = tr("%1 is from a newer version of TileZed").arg(txtName());
        return false;
    }

    // Not the latest version -> upgrade it.

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    // UPGRADE HERE
    if (userVersion == VERSION0) {
        int index = userFile.findBlock(QLatin1String("tilesets"));
        if (index >= 0) {
            SimpleFileBlock tilesetsBlock = userFile.blocks[index];
            for (const SimpleFileKeyValue &kv : qAsConst(tilesetsBlock.values)) {
                QString tilesetName = QFileInfo(kv.value).completeBaseName();
                if (TileMetaInfoMgr::instance()->tileset(tilesetName) == nullptr) {
                    Tileset *ts = new Tileset(tilesetName, 64, 128);
                    // Since the tileset image height/width wasn't saved, create
                    // a tileset with only a single tile.
                    ts->loadFromImage(TilesetManager::instance()->missingTile()->image(),
                                      kv.value + QLatin1String(".png"));
                    ts->setMissing(true);
                    TileMetaInfoMgr::instance()->addTileset(ts);
                }
            }
            userFile.blocks.removeAt(index);
            TileMetaInfoMgr::instance()->writeTxt();
        }
    }

    // Version 2: rename some layers
    if (userVersion == VERSION1) {
        QMap<QString, QString> renameLookup;
        renameLookup[QLatin1String("Curtains2")] = QLatin1String("Curtains3");
        renameLookup[QLatin1String("Doors")] = QLatin1String("Door");
        renameLookup[QLatin1String("Frames")] = QLatin1String("Frame");
        renameLookup[QLatin1String("Walls")] = QLatin1String("Wall");
        renameLookup[QLatin1String("Walls2")] = QLatin1String("Wall2");
        renameLookup[QLatin1String("Windows")] = QLatin1String("Window");
        int index = userFile.findBlock(QLatin1String("layers"));
        if (index >= 0) {
            SimpleFileBlock &layersBlock = userFile.blocks[index];
            for (SimpleFileKeyValue &kv : layersBlock.values) {
                if (kv.name == QLatin1String("tile")) {
                    if (renameLookup.contains(kv.value)) {
                        kv.value = renameLookup[kv.value];
                    }
                }
            }
        }
    }

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTMX::mergeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    Q_ASSERT(userFile.version() == VERSION_LATEST);

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    int userSourceRevision = userFile.value("source_revision").toInt();
    int sourceRevision = sourceFile.value("revision").toInt();
    if (sourceRevision == userSourceRevision)
        return true;

    // MERGE HERE

    // Get the list of layers from the source file.
    int sourceLayersBlock = sourceFile.findBlock(QLatin1String("layers"));
    if (sourceLayersBlock == -1) {
        mError = tr("Missing 'layers' block!\n%1").arg(sourcePath);
        return false;
    }
    QList<LayerInfo> sourceLayers;
    foreach (SimpleFileKeyValue kv, sourceFile.blocks[sourceLayersBlock].values) {
        if (kv.name == QLatin1String("tile")) {
            sourceLayers += LayerInfo(kv.value, LayerInfo::Tile);
        } else if (kv.name == QLatin1String("object")) {
            sourceLayers += LayerInfo(kv.value, LayerInfo::Object);
        } else {
            mError = tr("Unknown layer type '%1'.\n%2")
                    .arg(kv.name)
                    .arg(sourcePath);
            return false;
        }
    }

    // Get the list of layers from the user file.
    int userLayersBlock = userFile.findBlock(QLatin1String("layers"));
    if (userLayersBlock == -1) {
        mError = tr("Missing 'layers' block!\n%1").arg(userPath);
        return false;
    }
    QList<LayerInfo> userLayers;
    foreach (SimpleFileKeyValue kv, userFile.blocks[userLayersBlock].values) {
        if (kv.name == QLatin1String("tile")) {
            userLayers += LayerInfo(kv.value, LayerInfo::Tile);
        } else if (kv.name == QLatin1String("object")) {
            userLayers += LayerInfo(kv.value, LayerInfo::Object);
        } else {
            mError = tr("Unknown layer type '%1'.\n%2")
                    .arg(kv.name)
                    .arg(userPath);
            return false;
        }
    }

    QList<LayerInfo> mergedLayers = sourceLayers;

    int insertIndex = 0;
    foreach (LayerInfo info, userLayers) {
        if (mergedLayers.contains(info)) {
            insertIndex = mergedLayers.indexOf(info) + 1;
        } else {
            mergedLayers.insert(insertIndex, info);
            qDebug() << "TMXConfig.txt merge: added layer" << info.mName << "at" << insertIndex;
            insertIndex++;
        }
    }

    SimpleFileBlock &userBlock = userFile.blocks[userLayersBlock];
    userBlock.values.clear();
    foreach (LayerInfo info, mergedLayers) {
        if (info.mType == LayerInfo::Tile)
            userBlock.addValue("tile", info.mName);
        else
            userBlock.addValue("object", info.mName);
    }

    userFile.replaceValue("revision", QString::number(sourceRevision + 1));
    userFile.replaceValue("source_revision", QString::number(sourceRevision));

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}
