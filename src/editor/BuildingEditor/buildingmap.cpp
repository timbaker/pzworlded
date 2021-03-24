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

#include "buildingmap.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingroomdef.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingtmx.h"

#include "bmpblender.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "tilesetmanager.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maplevel.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

#include <QDebug>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingMap::BuildingMap(Building *building) :
    mBuilding(building),
    mMapComposite(nullptr),
    mMap(nullptr),
    mBlendMapComposite(nullptr),
    mBlendMap(nullptr),
    mMapRenderer(nullptr),
    mCursorObjectFloor(nullptr),
    mShadowBuilding(nullptr),
    pending(false),
    pendingRecreateAll(false),
    pendingBuildingResized(false)
{
    BuildingToMap();
}

BuildingMap::~BuildingMap()
{
    if (mMapComposite) {
        MapInfo *mapInfo = mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;
        delete mapInfo;

        mapInfo = mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;
        delete mapInfo;

        delete mMapRenderer;
    }

    if (mShadowBuilding)
        delete mShadowBuilding;
}

BuildingCell BuildingMap::buildingTileAt(int x, int y, int level, const QString &layerName)
{
    CompositeLayerGroup *layerGroup = mBlendMapComposite->layerGroupForLevel(level);

    BuildingCell buildingCell;

    for (TileLayer *tl : layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            if (tl->contains(x, y)) {
                const Cell &cell = tl->cellAt(x, y);
                if (!cell.isEmpty()) {
                    buildingCell.mTileName = BuildingTilesMgr::nameForTile(cell.tile);
                    buildingCell.mRotation = cell.rotation;
                }
            }
            break;
        }
    }

    return buildingCell;
}

BuildingCell BuildingMap::buildingTileAt(int x, int y, const QList<bool> visibleLevels)
{
    // x and y are scene coordinates
    // Perform per-pixel hit detection
    Tile *tile = nullptr;
    MapRotation rotation = MapRotation::NotRotated;

    for (int level = 0; level < mBuilding->floorCount(); level++) {
        if (!visibleLevels[level])
            continue;
        CompositeLayerGroup *lgBlend = mBlendMapComposite->layerGroupForLevel(level);
        CompositeLayerGroup *lg = mMapComposite->layerGroupForLevel(level);
        QPoint tilePos = mMapRenderer->pixelToTileCoordsInt(QPoint(x, y), level);
        for (int ty = tilePos.y() - 4; ty < tilePos.y() + 4; ty++) {
            for (int tx = tilePos.x() - 4; tx < tilePos.x() + 4; tx++) {
                QRectF tileBox = mMapRenderer->boundingRect(QRect(tx, ty, 1, 1), level);
                for (int i = 0; i < lg->layerCount(); i++) {
                    // Automatic building tiles first.
                    // User-drawn tiles second.
                    TileLayer *tlBlend = lgBlend->layers().at(i);
                    TileLayer *tl = lg->layers().at(i);
                    QString layerName = MapComposite::layerNameWithoutPrefix(tl->name());
                    if (!mBuilding->floor(level)->layerVisibility(layerName))
                        continue;
                    if (!tl->contains(tx, ty))
                        continue;
                    Tile *test = tl->cellAt(tx, ty).tile; // user tile
                    rotation = tl->cellAt(tx, ty).rotation;
                    if (!test) {
                        test = tlBlend->cellAt(tx, ty).tile; // building tile
                        rotation = tlBlend->cellAt(tx, ty).rotation;
                    }
                    if (test) {
                        Tile *realTile = test;
                        if (test->image().isNull()) {
                            test = TilesetManager::instance()->missingTile();
                        }
                        QRect imageBox(test->offset(), test->image().size());
                        QPoint p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height()));
                        // Handle double-size tiles
                        if (qRound(tileBox.width()) == test->width() * 2) {
                            p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height() * 2));
                            p.rx() /= 2;
                            p.ry() /= 2;
                        } else if (qRound(tileBox.width()) == test->width() / 2) {

                        }
                        // Hit test a small box around the cursor?
                        QRect box(p - QPoint(0, 0), p + QPoint(0, 0));
                        for (int px = box.left(); px <= box.right(); px++) {
                            for (int py = box.top(); py <= box.bottom(); py++) {
                                if (imageBox.contains(px, py)) {
                                    QRgb pixel = test->image().pixel(px - imageBox.x(), py - imageBox.y());
                                    if (qAlpha(pixel) > 0)
                                        tile = realTile;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (tile) {
        return BuildingCell(BuildingTilesMgr::nameForTile(tile), rotation);
    }
    return BuildingCell();
}

// The order must match the BuildingFloor::Square::SquareSection constants.
static const char *gLayerNames[] = {
    "Floor",
    "FloorGrime",
    "FloorGrime2",
    "FloorGrime3",
    "FloorGrime4",
    "Wall",
    "WallTrim",
    "Wall2",
    "WallTrim2",
    "Wall3",
    "WallTrim3",
    "Wall4",
    "WallTrim4",
    "RoofCap",
    "RoofCap2",
    "WallOverlay",
    "WallOverlay2",
    "WallOverlay3",
    "WallOverlay4",
    "WallGrime",
    "WallGrime2",
    "WallGrime3",
    "WallGrime4",
    "WallFurniture",
    "WallFurniture2",
    "WallFurniture3",
    "WallFurniture4",
    "Frame",
    "Frame2",
    "Frame3",
    "Frame4",
    "Door",
    "Door2",
    "Door3",
    "Door4",
    "Window",
    "Window2",
    "Window3",
    "Window4",
    "Curtains",
    "Curtains2",
    "Curtains3",
    "Curtains4",
    "Furniture",
    "Furniture2",
    "Furniture3",
    "Furniture4",
    "Roof",
    "Roof2",
    "RoofTop",
    nullptr
};

QStringList BuildingMap::layerNames(int level)
{
    return BuildingTMX::instance()->tileLayerNamesForLevel(level);
}

QStringList BuildingMap::requiredLayerNames()
{
    QStringList ret;
    for (int i = 0; gLayerNames[i]; i++)
        ret += QLatin1String(gLayerNames[i]);
    return ret;
}

void BuildingMap::setCursorObject(BuildingFloor *floor, BuildingObject *object)
{
    if (mCursorObjectFloor && (mCursorObjectFloor != floor)) {
        pendingLayoutToSquares.insert(mCursorObjectFloor);
        if (mCursorObjectFloor->floorAbove())
            pendingLayoutToSquares.insert(mCursorObjectFloor->floorAbove());
        schedulePending();
        mCursorObjectFloor = nullptr;
    }

    if (mShadowBuilding->setCursorObject(floor, object)) {
        pendingLayoutToSquares.insert(floor);
        if (floor && floor->floorAbove())
            pendingLayoutToSquares.insert(floor->floorAbove());
        schedulePending();
        mCursorObjectFloor = object ? floor : nullptr;
    }
}

void BuildingMap::dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset)
{
    mShadowBuilding->dragObject(floor, object, offset);
    pendingLayoutToSquares.insert(floor);
    if (floor->floorAbove())
        pendingLayoutToSquares.insert(floor->floorAbove());
    schedulePending();
}

void BuildingMap::resetDrag(BuildingFloor *floor, BuildingObject *object)
{
    mShadowBuilding->resetDrag(object);
    pendingLayoutToSquares.insert(floor);
    if (floor->floorAbove())
        pendingLayoutToSquares.insert(floor->floorAbove());
    schedulePending();
}

void BuildingMap::changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room*> > &grid)
{
    mShadowBuilding->changeFloorGrid(floor, grid);
    pendingLayoutToSquares.insert(floor);
    schedulePending();
}

void BuildingMap::resetFloorGrid(BuildingFloor *floor)
{
    mShadowBuilding->resetFloorGrid(floor);
    pendingLayoutToSquares.insert(floor);
    schedulePending();
}

void BuildingMap::changeUserTiles(BuildingFloor *floor, const QMap<QString,FloorTileGrid*> &tiles)
{
    mShadowBuilding->changeUserTiles(floor, tiles);
    pendingSquaresToTileLayers[floor] |= floor->bounds(1, 1);
    foreach (QString layerName, floor->grimeLayers())
        pendingUserTilesToLayer[floor][layerName] |= floor->bounds(1, 1);
    schedulePending();
}

void BuildingMap::resetUserTiles(BuildingFloor *floor)
{
    mShadowBuilding->resetUserTiles(floor);
    pendingSquaresToTileLayers[floor] |= floor->bounds();
    foreach (QString layerName, floor->grimeLayers())
        pendingUserTilesToLayer[floor][layerName] |= floor->bounds(1, 1);
    schedulePending();
}

void BuildingMap::suppressTiles(BuildingFloor *floor, const QRegion &rgn)
{
    QRegion update;
    if (mSuppressTiles.contains(floor) && mSuppressTiles[floor] == rgn)
        return;
    if (rgn.isEmpty()) {
        update = mSuppressTiles[floor];
        mSuppressTiles.remove(floor);
    } else {
        update = rgn | mSuppressTiles[floor];
        mSuppressTiles[floor] = rgn;
    }
    if (!update.isEmpty()) {
        foreach (QRect r, update.rects()) {
            r &= floor->bounds(1, 1);
            pendingSquaresToTileLayers[floor] |= r;
            foreach (QString layerName, floor->grimeLayers())
                pendingUserTilesToLayer[floor][layerName] |= r;
        }
        schedulePending();
    }
}

Map *BuildingMap::mergedMap() const
{
    Map *map = mBlendMap->clone();
    TilesetManager::instance()->addReferences(map->tilesets());
    for (int z = 0; z < map->levelCount(); z++) {
        MapLevel *level = map->levelAt(z);
        for (int j = 0; j < level->layerCount(); j++) {
            if (TileLayer *tl = level->layerAt(j)->asTileLayer()) {
                TileLayer *tl2 =  mMap->levelAt(level->z())->layerAt(j)->asTileLayer();
                tl->merge(tl->position(), tl2);
            }
        }
    }
    return map;
}

void BuildingMap::loadNeededTilesets(Building *building)
{
    // If the building uses any tilesets that aren't in Tilesets.txt, then
    // try to load them in now.
    foreach (QString tilesetName, building->tilesetNames()) {
        if (!TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            QString source = TileMetaInfoMgr::instance()->tilesDirectory() +
                    QLatin1Char('/') + tilesetName + QLatin1String(".png");
            QFileInfo info(source);
            if (!info.exists())
                continue;
            source = info.canonicalFilePath();
            if (Tileset *ts = TileMetaInfoMgr::instance()->loadTileset(source)) {
                TileMetaInfoMgr::instance()->addTileset(ts);
            }
        }
    }
}

// Copied from BuildingFloor::roomRegion()
static QList<QRect> cleanupRegion(QRegion region)
{
    // Clean up the region by merging vertically-adjacent rectangles of the
    // same width.
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++) {
        QRect r = rects[i];
        if (!r.isValid()) continue;
        for (int j = 0; j < rects.size(); j++) {
            if (i == j) continue;
            QRect r2 = rects.at(j);
            if (!r2.isValid()) continue;
            if (r2.left() == r.left() && r2.right() == r.right()) {
                if (r.bottom() + 1 == r2.top()) {
                    r.setBottom(r2.bottom());
                    rects[j] = QRect();
                } else if (r.top() == r2.bottom() + 1) {
                    r.setTop(r2.top());
                    rects[j] = QRect();
                }
            }
        }
        rects[i] = r;
    }

    QList<QRect> ret;
    foreach (QRect r, rects) {
        if (r.isValid())
            ret += r;
    }
    return ret;
}

void BuildingMap::addRoomDefObjects(Map *map)
{
    foreach (BuildingFloor *floor, mBuilding->floors())
        addRoomDefObjects(map, floor);
}

void BuildingMap::addRoomDefObjects(Map *map, BuildingFloor *floor)
{
    Building *building = floor->building();
    ObjectGroup *objectGroup = new ObjectGroup(tr("RoomDefs"),
                                               0, 0, map->width(), map->height());
    map->addLayer(objectGroup);

    int delta = (building->floorCount() - 1 - floor->level()) * 3;
    if (map->orientation() == Map::LevelIsometric)
        delta = 0;
    QPoint offset(delta, delta);
    int roomID = 1;
    foreach (Room *room, building->rooms()) {
#if 1
        BuildingRoomDefecator rd(floor, room);
        rd.defecate();
        foreach (QRegion roomRgn, rd.mRegions) {
            foreach (QRect rect, cleanupRegion(roomRgn)) {
                QString name = room->internalName + QLatin1Char('#')
                        + QString::number(roomID);
                MapObject *mapObject = new MapObject(name, QLatin1String("room"),
                                                     rect.topLeft() + offset,
                                                     rect.size());
                objectGroup->addObject(mapObject);
            }
            ++roomID;
        }
#else
        foreach (QRect rect, floor->roomRegion(room)) {
            QString name = room->internalName + QLatin1Char('#')
                    + QString::number(roomID);
            MapObject *mapObject = new MapObject(name, QLatin1String("room"),
                                                 rect.topLeft() + offset,
                                                 rect.size());
            objectGroup->addObject(mapObject);
        }
        ++roomID;
#endif
    }
}

int BuildingMap::defaultOrientation()
{
    return Map::LevelIsometric;
}

bool BuildingMap::isTilesetUsed(Tileset *tileset)
{
    return mMap->isTilesetUsed(tileset) || mBlendMap->isTilesetUsed(tileset);
}

void BuildingMap::buildingRotated()
{
    pendingBuildingResized = true;

    // When rotating or flipping, all the user tiles are cleared.
    // However, no signal is emitted until the buildingRotated signal.
    pendingEraseUserTiles = mBuilding->floors().toSet();

    schedulePending();
}

void BuildingMap::buildingResized()
{
    pendingBuildingResized = true;
    schedulePending();
}

void BuildingMap::BuildingToMap()
{
    if (mMapComposite) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;

        delete mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;

        delete mMapRenderer;
    }

    if (mShadowBuilding)
        delete mShadowBuilding;
    mShadowBuilding = new ShadowBuilding(mBuilding);
    mCursorObjectFloor = nullptr;

    Map::Orientation orient = static_cast<Map::Orientation>(defaultOrientation());

    int maxLevel =  mBuilding->floorCount() - 1;
    int extraForWalls = 1;
    int extra = (orient == Map::LevelIsometric)
            ? extraForWalls : maxLevel * 3 + extraForWalls;
    QSize mapSize(mBuilding->width() + extra,
                  mBuilding->height() + extra);

    mMap = new Map(orient,
                   mapSize.width(), mapSize.height(),
                   64, 32);

    // Add tilesets from Tilesets.txt
    mMap->addTileset(TilesetManager::instance()->missingTileset());
    for (Tileset *ts : TileMetaInfoMgr::instance()->tilesets()) {
        mMap->addTileset(ts);
    }
    TilesetManager::instance()->addReferences(mMap->tilesets());

    switch (mMap->orientation()) {
    case Map::Isometric:
        mMapRenderer = new IsometricRenderer(mMap);
        break;
    case Map::LevelIsometric:
        mMapRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return;
    }

    Q_ASSERT(sizeof(gLayerNames)/sizeof(gLayerNames[0]) == BuildingSquare::MaxSection + 1);

    QMap<QString,int> layerToSection;
    for (int i = 0; gLayerNames[i]; i++)
        layerToSection.insert(QLatin1String(gLayerNames[i]), i);

    mLayerToSection.clear();
    for (BuildingFloor *floor : mBuilding->floors()) {
        for (QString name : layerNames(floor->level())) {
            QString layerName = name;
            TileLayer *tl = new TileLayer(layerName, 0, 0, mapSize.width(), mapSize.height());
            tl->setLevel(floor->level());
            mMap->addLayer(tl);
            mLayerToSection[layerName] = layerToSection.contains(name)
                    ? layerToSection[name] : -1;
        }
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);
    mMapComposite = new MapComposite(mapInfo);

    // Synch layer opacity with the floor.
    for (CompositeLayerGroup *layerGroup : mMapComposite->layerGroups()) {
        BuildingFloor *floor = mBuilding->floor(layerGroup->level());
        for (TileLayer *tl : layerGroup->layers()) {
            QString layerName = MapComposite::layerNameWithoutPrefix(tl);
            layerGroup->setLayerOpacity(tl, floor->layerOpacity(layerName));
        }
    }

    // This map displays the automatically-generated tiles from the building.
    mBlendMap = mMap->clone();
    TilesetManager::instance()->addReferences(mBlendMap->tilesets());
    mapInfo = MapManager::instance()->newFromMap(mBlendMap);
    mBlendMapComposite = new MapComposite(mapInfo);
    mMapComposite->setBlendOverMap(mBlendMapComposite);

    // Set the automatically-generated tiles.
    for (CompositeLayerGroup *layerGroup : mBlendMapComposite->layerGroups()) {
        BuildingFloor *floor = mBuilding->floor(layerGroup->level());
        floor->LayoutToSquares();
        BuildingSquaresToTileLayers(floor, floor->bounds(1, 1), layerGroup);
    }

    // Set the user-drawn tiles.
    for (BuildingFloor *floor : mBuilding->floors()) {
        for (QString layerName : floor->grimeLayers())
            userTilesToLayer(floor, layerName, floor->bounds(1, 1));
    }

    // Do this before calculating the bounds of CompositeLayerGroupItem
    mMapRenderer->setMaxLevel(mMapComposite->maxLevel());
}

void BuildingMap::BuildingSquaresToTileLayers(BuildingFloor *floor,
                                              const QRect &area,
                                              CompositeLayerGroup *layerGroup)
{
    BuildingFloor *shadowFloor = mShadowBuilding->floor(floor->level());

    int maxLevel = floor->building()->floorCount() - 1;
    int offset = (mMap->orientation() == Map::LevelIsometric)
            ? 0 : (maxLevel - floor->level()) * 3;

    QRegion suppress;
    if (mSuppressTiles.contains(floor))
        suppress = mSuppressTiles[floor];

    int layerIndex = 0;
    for (TileLayer *tl : layerGroup->layers()) {
        int section = mLayerToSection[tl->name()];
        if (section == -1) // Skip user-added layers.
            continue;
        if (area == floor->bounds(1, 1))
            tl->erase();
        else
            tl->erase(area/*.adjusted(0,0,1,1)*/);
        for (int x = area.x(); x <= area.right(); x++) {
            for (int y = area.y(); y <= area.bottom(); y++) {
                if (section != BuildingSquare::SectionFloor
                        && suppress.contains(QPoint(x, y))) {
                    continue;
                }
                const BuildingSquare &square = shadowFloor->squares[x][y];
                MapRotation rotation = square.mRotation[section];
                if (BuildingTile *btile = square.mTiles[section]) {
                    if (!btile->isNone()) {
                        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(btile)) {
                            tl->setCell(x + offset, y + offset, Cell(tile, rotation));
                        }
                    }
                    continue;
                }
                if (BuildingTileEntry *entry = square.mEntries[section]) {
                    int tileOffset = square.mEntryEnum[section];
                    if (entry->isNone() || entry->tile(tileOffset)->isNone()) {
                        continue;
                    }
                    BuildingTile *buildingTile = entry->tile(tileOffset);
                    if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(buildingTile)) {
                        tl->setCell(x + offset, y + offset, Cell(tile, rotation));
                    }
                }
            }
        }
        layerGroup->regionAltered(tl); // possibly set mNeedsSynch
        layerIndex++;
    }
}

void BuildingMap::userTilesToLayer(BuildingFloor *floor,
                                   const QString &layerName,
                                   const QRect &bounds)
{
    CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
    TileLayer *layer = nullptr;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    if (!layer) {
        // The building has user-drawn tiles in a layer that is not in TMXConfig.txt.
//        Q_ASSERT(false);
        return;
    }

    QMap<QString,Tileset*> tilesetByName;
    for (Tileset *ts : mMap->tilesets())
        tilesetByName[ts->name()] = ts;

    QRegion suppress;
    if (mSuppressTiles.contains(floor))
        suppress = mSuppressTiles[floor];

    BuildingFloor *shadowFloor = mShadowBuilding->floor(floor->level());

    for (int x = bounds.left(); x <= bounds.right(); x++) {
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            if (suppress.contains(QPoint(x, y))) {
                layer->setCell(x, y, Cell());
                continue;
            }
            BuildingCell buildingCell = shadowFloor->grimeAt(layerName, x, y);
            Tile *tile = nullptr;
            MapRotation rotation = MapRotation::NotRotated;
            if (!buildingCell.isEmpty()) {
                tile = TilesetManager::instance()->missingTile();
                QString tilesetName;
                int index;
                if (BuildingTilesMgr::parseTileName(buildingCell.tileName(), tilesetName, index)) {
                    if (tilesetByName.contains(tilesetName)) {
                        tile = tilesetByName[tilesetName]->tileAt(index);
                        rotation = buildingCell.rotation();
                    }
                }
            }
            layer->setCell(x, y, Cell(tile, rotation));
        }
    }

    layerGroup->regionAltered(layer); // possibly set mNeedsSynch
}

void BuildingMap::floorAdded(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    recreateAllLater();
}

void BuildingMap::floorRemoved(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    recreateAllLater();
}

void BuildingMap::floorEdited(BuildingFloor *floor)
{
    mShadowBuilding->floorEdited(floor);

    pendingLayoutToSquares.insert(floor);
    schedulePending();
}

void BuildingMap::floorTilesChanged(BuildingFloor *floor)
{
    mShadowBuilding->floorTilesChanged(floor);

    pendingEraseUserTiles.insert(floor);

    // Painting tiles in the Walls/Walls2 layer affects which grime tiles are chosen.
//    if (tiles.contains(QLatin1Literal("Walls")) || tiles.contains(QLatin1Literal("Walls2")))
        pendingLayoutToSquares.insert(floor);

    schedulePending();
}

void BuildingMap::floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                                    const QRect &bounds)
{
    mShadowBuilding->floorTilesChanged(floor, layerName, bounds);

    pendingUserTilesToLayer[floor][layerName] |= bounds;

    // Painting tiles in the Walls/Walls2 layer affects which grime tiles are chosen.
    if (layerName == QLatin1Literal("Wall") || layerName == QLatin1Literal("Wall2"))
        pendingLayoutToSquares.insert(floor);

    schedulePending();
}

void BuildingMap::objectAdded(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    schedulePending();

    mShadowBuilding->objectAdded(object);
}

void BuildingMap::objectAboutToBeRemoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    schedulePending();

    mShadowBuilding->objectAboutToBeRemoved(object);
}

void BuildingMap::objectRemoved(BuildingObject *object)
{
    Q_UNUSED(object)
}

void BuildingMap::objectMoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    schedulePending();

    mShadowBuilding->objectMoved(object);
}

void BuildingMap::objectTileChanged(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    schedulePending();

    mShadowBuilding->objectTileChanged(object);
}

void BuildingMap::roomAdded(Room *room)
{
    mShadowBuilding->roomAdded(room);
}

void BuildingMap::roomRemoved(Room *room)
{
    mShadowBuilding->roomRemoved(room);
}

// When tilesets are added/removed, BuildingTile -> Tiled::Tile needs to be
// redone.
void BuildingMap::tilesetAdded(Tileset *tileset)
{
    int index = mMap->tilesets().indexOf(tileset);
    if (index >= 0)
        return;

    mMap->addTileset(tileset);
    TilesetManager::instance()->addReference(tileset);

    mBlendMap->addTileset(tileset);
    TilesetManager::instance()->addReference(tileset);

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        pendingSquaresToTileLayers[floor] = floor->bounds(1, 1);
        foreach (QString layerName, floor->grimeLayers())
            pendingUserTilesToLayer[floor][layerName] = floor->bounds(1, 1);
    }

    schedulePending();
}

void BuildingMap::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int index = mMap->tilesets().indexOf(tileset);
    if (index < 0)
        return;

    mMap->removeTilesetAt(index);
    TilesetManager::instance()->removeReference(tileset);

    mBlendMap->removeTilesetAt(index);
    TilesetManager::instance()->removeReference(tileset);

    // Erase every layer to get rid of Tiles from the tileset.
    foreach (CompositeLayerGroup *lg, mMapComposite->layerGroups())
        foreach (TileLayer *tl, lg->layers())
            tl->erase();
    foreach (CompositeLayerGroup *lg, mBlendMapComposite->layerGroups())
        foreach (TileLayer *tl, lg->layers())
            tl->erase();

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        pendingSquaresToTileLayers[floor] = floor->bounds(1, 1);
        foreach (QString layerName, floor->grimeLayers())
            pendingUserTilesToLayer[floor][layerName] = floor->bounds(1, 1);
    }

    schedulePending();
}

void BuildingMap::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
}

// FIXME: tilesetChanged?

void BuildingMap::handlePending()
{
    QMap<int,QRegion> updatedLevels;

    if (pendingRecreateAll) {
        emit aboutToRecreateLayers(); // LayerGroupItems need to get ready
        BuildingToMap();
        pendingBuildingResized = false;
        pendingEraseUserTiles.clear(); // no need to erase, we just recreated the layers
    }

    if (pendingRecreateAll || pendingBuildingResized) {
        pendingLayoutToSquares = mBuilding->floors().toSet();
        pendingUserTilesToLayer.clear();
        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (QString layerName, floor->grimeLayers()) {
                pendingUserTilesToLayer[floor][layerName] = floor->bounds(1, 1);
            }
        }
    }

    if (pendingBuildingResized) {
        int extra = (mMap->orientation() == Map::LevelIsometric) ?
                    1 : mMapComposite->maxLevel() * 3 + 1;
        int width = mBuilding->width() + extra;
        int height = mBuilding->height() + extra;

        foreach (Layer *layer, mMap->layers())
            layer->resize(QSize(width, height), QPoint());
        mMap->setWidth(width);
        mMap->setHeight(height);
        MapManager::instance()->mapParametersChanged(mMapComposite->mapInfo());
        mMapComposite->bmpBlender()->recreate();

        foreach (Layer *layer, mBlendMap->layers())
            layer->resize(QSize(width, height), QPoint());
        mBlendMap->setWidth(width);
        mBlendMap->setHeight(height);
        MapManager::instance()->mapParametersChanged(mBlendMapComposite->mapInfo());
        mBlendMapComposite->bmpBlender()->recreate();

        foreach (CompositeLayerGroup *lg, mMapComposite->layerGroups())
            lg->setNeedsSynch(true);

        delete mShadowBuilding;
        mShadowBuilding = new ShadowBuilding(mBuilding);
    }

    if (!pendingLayoutToSquares.isEmpty()) {
        foreach (BuildingFloor *floor, pendingLayoutToSquares) {
            floor->LayoutToSquares(); // not sure this belongs in this class
            pendingSquaresToTileLayers[floor] = floor->bounds(1, 1);

            mShadowBuilding->floor(floor->level())->LayoutToSquares();
        }
    }

    if (!pendingSquaresToTileLayers.isEmpty()) {
        foreach (BuildingFloor *floor, pendingSquaresToTileLayers.keys()) {
            CompositeLayerGroup *layerGroup = mBlendMapComposite->layerGroupForLevel(floor->level());
            QRect area = pendingSquaresToTileLayers[floor].boundingRect(); // TODO: only affected region
            BuildingSquaresToTileLayers(floor, area, layerGroup);
            if (layerGroup->needsSynch()) {
                mMapComposite->layerGroupForLevel(floor->level())->setNeedsSynch(true);
                layerGroup->synch(); // Don't really need to synch the blend-over-map, but do need
                                     // to update its draw margins so MapComposite::regionAltered
                                     // doesn't set mNeedsSynch repeatedly.
            }
            updatedLevels[floor->level()] |= area;
        }
    }

    if (!pendingEraseUserTiles.isEmpty()) {
        foreach (BuildingFloor *floor, pendingEraseUserTiles) {
            CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
            foreach (TileLayer *tl, layerGroup->layers())
                tl->erase();
            foreach (QString layerName, floor->grimeLayers())
                pendingUserTilesToLayer[floor][layerName] = floor->bounds(1, 1);
            updatedLevels[floor->level()] |= floor->bounds();
        }
    }

    if (!pendingUserTilesToLayer.isEmpty()) {
        foreach (BuildingFloor *floor, pendingUserTilesToLayer.keys()) {
            foreach (QString layerName, pendingUserTilesToLayer[floor].keys()) {
                QRegion rgn = pendingUserTilesToLayer[floor][layerName];
                foreach (QRect r, rgn.rects())
                    userTilesToLayer(floor, layerName, r);
                updatedLevels[floor->level()] |= rgn;
            }
        }
    }

    if (pendingRecreateAll)
        emit layersRecreated();
    else if (pendingBuildingResized)
        emit mapResized();

    foreach (int level, updatedLevels.keys())
        emit layersUpdated(level, updatedLevels[level]);

    pending = false;
    pendingRecreateAll = false;
    pendingBuildingResized = false;
    pendingLayoutToSquares.clear();
    pendingSquaresToTileLayers.clear();
    pendingEraseUserTiles.clear();
    pendingUserTilesToLayer.clear();
}

void BuildingMap::recreateAllLater()
{
    pendingRecreateAll = true;
    schedulePending();
}

/////

BuildingModifier::BuildingModifier(ShadowBuilding *shadowBuilding) :
    mShadowBuilding(shadowBuilding)
{
    mShadowBuilding->addModifier(this);
}

BuildingModifier::~BuildingModifier()
{
    mShadowBuilding->removeModifier(this);
}

class AddObjectModifier : public BuildingModifier
{
public:
    AddObjectModifier(ShadowBuilding *sb, BuildingFloor *floor, BuildingObject *object) :
        BuildingModifier(sb)
    {
        BuildingFloor *shadowFloor = mShadowBuilding->floor(floor->level());
        mObject = object;
        mShadowObject = mShadowBuilding->cloneObject(shadowFloor, object);
        shadowFloor->insertObject(shadowFloor->objectCount(), mShadowObject);
    }

    ~AddObjectModifier()
    {
        // It's possible the object was added to the floor after this
        // modifier was created.  For example, RoofTool adds the actual
        // cursor object to the floor when creating a new roof object.
        if (mObject)
            mShadowBuilding->objectAboutToBeRemoved(mObject);
    }

    BuildingObject *mObject;
    BuildingObject *mShadowObject;
};

class ResizeObjectModifier : public BuildingModifier
{
public:
    ResizeObjectModifier(ShadowBuilding *sb, BuildingObject *object,
                         BuildingObject *shadowObject) :
        BuildingModifier(sb),
        mObject(object),
        mShadowObject(shadowObject)
    {
    }

    ~ResizeObjectModifier()
    {
        // When resizing is cancelled/finished, redisplay the object.
        mShadowBuilding->recreateObject(mObject->floor(), mObject);
    }

    BuildingObject *mObject;
    BuildingObject *mShadowObject;
};

class MoveObjectModifier : public BuildingModifier
{
public:
    MoveObjectModifier(ShadowBuilding *sb, BuildingObject *object) :
        BuildingModifier(sb),
        mObject(object)
    {
        // The shadow object should already exist, we're just moving an existing object.

    }

    ~MoveObjectModifier()
    {
        setOffset(QPoint(0, 0));
    }

    void setOffset(const QPoint &offset)
    {
        BuildingObject *shadowObject = mShadowBuilding->shadowObject(mObject);
        shadowObject->setPos(mObject->pos() + offset);
    }

    BuildingObject *mObject;
};

class ChangeFloorGridModifier : public BuildingModifier
{
public:
    ChangeFloorGridModifier(ShadowBuilding *sb, BuildingFloor *floor) :
        BuildingModifier(sb),
        mFloor(floor)
    {
    }

    ~ChangeFloorGridModifier()
    {
        BuildingFloor *shadowFloor = mShadowBuilding->floor(mFloor->level());
        shadowFloor->setGrid(mFloor->grid());
    }

    void setGrid(const QVector<QVector<Room*> > &grid)
    {
        BuildingFloor *shadowFloor = mShadowBuilding->floor(mFloor->level());
        shadowFloor->setGrid(grid);
    }

    BuildingFloor *mFloor;
};

class ChangeUserTilesModifier : public BuildingModifier
{
public:
    ChangeUserTilesModifier(ShadowBuilding *sb, BuildingFloor *floor) :
        BuildingModifier(sb),
        mFloor(floor)
    {
    }

    ~ChangeUserTilesModifier()
    {
        BuildingFloor *shadowFloor = mShadowBuilding->floor(mFloor->level());
        qDeleteAll(shadowFloor->setGrime(mFloor->grimeClone()));
    }

    void setTiles(const QMap<QString,FloorTileGrid*> &tiles)
    {
        BuildingFloor *shadowFloor = mShadowBuilding->floor(mFloor->level());
        qDeleteAll(shadowFloor->setGrime(tiles));
    }

    BuildingFloor *mFloor;
};

ShadowBuilding::ShadowBuilding(const Building *building) :
    mBuilding(building),
    mCursorObjectModifier(nullptr)
{
    mShadowBuilding = new Building(mBuilding->width(), mBuilding->height());
    mShadowBuilding->setTiles(mBuilding->tiles());
    foreach (Room *room, mBuilding->rooms())
        mShadowBuilding->insertRoom(mShadowBuilding->roomCount(), room);
    foreach (BuildingFloor *floor, mBuilding->floors()) {
        BuildingFloor *f = cloneFloor(floor);
        f->LayoutToSquares();
    }

}

ShadowBuilding::~ShadowBuilding()
{
    qDeleteAll(mModifiers);
    while (mShadowBuilding->roomCount())
        mShadowBuilding->removeRoom(0);
    delete mShadowBuilding;
}

BuildingFloor *ShadowBuilding::floor(int level) const
{
    return mShadowBuilding->floor(level);
}

void ShadowBuilding::buildingRotated()
{
}

void ShadowBuilding::buildingResized()
{
}

void ShadowBuilding::floorAdded(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    // The whole ShadowBuilding gets recreated elsewhere.
}

void ShadowBuilding::floorRemoved(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    // The whole ShadowBuilding gets recreated elsewhere.
}

void ShadowBuilding::floorEdited(BuildingFloor *floor)
{
    // BuildingDocument emits roomDefinitionChanged when the exterior wall changes.
    // BuildingTileModeScene::roomDefinitionChanged() calls this method.
    for (int e = 0; e < Building::TileCount; e++)
        mShadowBuilding->setTile(e, mBuilding->tile(e));

    mShadowBuilding->floor(floor->level())->setGrid(floor->grid());
}

void ShadowBuilding::floorTilesChanged(BuildingFloor *floor)
{
    BuildingFloor *shadowFloor = mShadowBuilding->floor(floor->level());

    QMap<QString,FloorTileGrid*> grime = shadowFloor->setGrime(floor->grimeClone());
    foreach (FloorTileGrid *grid, grime.values())
        delete grid;
}

void ShadowBuilding::floorTilesChanged(BuildingFloor *floor,
                                       const QString &layerName,
                                       const QRect &bounds)
{
    BuildingFloor *shadowFloor = mShadowBuilding->floor(floor->level());

    FloorTileGrid *grid = floor->grimeAt(layerName, bounds);
    shadowFloor->setGrime(layerName, bounds.topLeft(), grid);
    delete grid;
}

void ShadowBuilding::objectAdded(BuildingObject *object)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (AddObjectModifier *mod = dynamic_cast<AddObjectModifier*>(bmod)) {
            if (mod->mObject == object) {
                mod->mObject = nullptr;
            }
        }
    }

    BuildingFloor *shadowFloor = mShadowBuilding->floor(object->floor()->level());

    // Check if the object was already added.  For example, RoofTool creates
    // a cursor-object for a new roof, then adds that object to the floor
    // when new roof object is added to the building.
    if (mOriginalToShadowObject.contains(object)) {
        BuildingObject *shadowObject = mOriginalToShadowObject[object];
        shadowFloor->removeObject(shadowObject->index());
        shadowFloor->insertObject(object->index(), shadowObject);
        return;
    }

    shadowFloor->insertObject(object->index(), cloneObject(shadowFloor, object));
}

void ShadowBuilding::objectAboutToBeRemoved(BuildingObject *object)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (MoveObjectModifier *mod = dynamic_cast<MoveObjectModifier*>(bmod)) {
            if (mod->mObject == object) {
                delete mod;
                continue;
            }
        }
    }

    if (mOriginalToShadowObject.contains(object)) {
        BuildingObject *shadowObject = mOriginalToShadowObject[object];
        shadowObject->floor()->removeObject(shadowObject->index());
        delete shadowObject;
        mOriginalToShadowObject.remove(object);
    }
}

void ShadowBuilding::objectRemoved(BuildingObject *object)
{
    Q_UNUSED(object)
}

void ShadowBuilding::objectMoved(BuildingObject *object)
{
    // This also gets called when a roof object is resized.
    if (mOriginalToShadowObject.contains(object)) {
        recreateObject(object->floor(), object);
    }
}

void ShadowBuilding::objectTileChanged(BuildingObject *object)
{
    recreateObject(object->floor(), object);
}

void ShadowBuilding::roomAdded(Room *room)
{
    mShadowBuilding->insertRoom(mBuilding->indexOf(room), room);
}

void ShadowBuilding::roomRemoved(Room *room)
{
    mShadowBuilding->removeRoom(mShadowBuilding->indexOf(room));
}

BuildingFloor *ShadowBuilding::cloneFloor(BuildingFloor *floor)
{
    BuildingFloor *f = new BuildingFloor(mShadowBuilding, floor->level());
    f->setGrid(floor->grid());
    f->setGrime(floor->grimeClone());
    mShadowBuilding->insertFloor(f->level(), f);
    foreach (BuildingObject *object, floor->objects())
        f->insertObject(f->objectCount(), cloneObject(f, object));
    return f;
}

BuildingObject *ShadowBuilding::cloneObject(BuildingFloor *shadowFloor, BuildingObject *object)
{
    Q_ASSERT(!mOriginalToShadowObject.contains(object));
    BuildingObject *clone = object->clone();
    clone->setFloor(shadowFloor);
    mOriginalToShadowObject[object] = clone;
    return clone;
}

void ShadowBuilding::recreateObject(BuildingFloor *originalFloor, BuildingObject *object)
{
    if (mOriginalToShadowObject.contains(object)) {
        BuildingObject *shadowObject = mOriginalToShadowObject[object];
        int index = shadowObject->index();
        BuildingFloor *shadowFloor = shadowObject->floor();
        shadowFloor->removeObject(index);
        delete shadowObject;
        mOriginalToShadowObject.remove(object);

        shadowFloor = mShadowBuilding->floor(originalFloor->level());
        shadowObject = cloneObject(shadowFloor, object);
        shadowFloor->insertObject(index, shadowObject);
    }
}

void ShadowBuilding::addModifier(BuildingModifier *modifier)
{
    mModifiers += modifier;
}

void ShadowBuilding::removeModifier(BuildingModifier *modifier)
{
    mModifiers.removeAll(modifier);
}

bool ShadowBuilding::setCursorObject(BuildingFloor *floor, BuildingObject *object)
{
    if (!object) {
        if (mCursorObjectModifier) {
            delete mCursorObjectModifier;
            mCursorObjectModifier = nullptr;
            return true;
        }
        return false;
    }

    // Recreate the object, its tile or orientation may have changed.
    // Also, the floor the cursor object is on might have changed.
    if (mCursorObjectModifier) {
        // FIXME: any modifier using the recreated shadow object must get updated
        // or they will still point to the old shadow object.
        if (mOriginalToShadowObject.contains(object)) {
//            if (object->sameAs(mOriginalToShadowObject[object])) {
//                qDebug() << "sameAs";
//                return false;
//            }
            recreateObject(floor, object);
        }
    } else {
        bool cursorObject = floor->indexOf(object) == -1;
        if (cursorObject) {
            mCursorObjectModifier = new AddObjectModifier(this, floor, object);
        } else {
            mCursorObjectModifier = new ResizeObjectModifier(this, object,
                                                             mOriginalToShadowObject[object]);
        }
    }

    return true;
}

void ShadowBuilding::dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset)
{
    if (object->floor() == nullptr) {
        foreach (BuildingModifier *bmod, mModifiers) {
            if (AddObjectModifier *mod = dynamic_cast<AddObjectModifier*>(bmod)) {
                if (mod->mObject == object) {
                    mod->mShadowObject->setPos(object->pos() + offset);
                    return;
                }
            }
        }
        AddObjectModifier *mod = new AddObjectModifier(this, floor, object);
        mod->mShadowObject->setPos(object->pos() + offset);
        return;
    }

    foreach (BuildingModifier *bmod, mModifiers) {
        if (MoveObjectModifier *mod = dynamic_cast<MoveObjectModifier*>(bmod)) {
            if (mod->mObject == object) {
                mod->setOffset(offset);
                return;
            }
        }
    }

    MoveObjectModifier *mod = new MoveObjectModifier(this, object);
    mod->setOffset(offset);
}

void ShadowBuilding::resetDrag(BuildingObject *object)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (MoveObjectModifier *mod = dynamic_cast<MoveObjectModifier*>(bmod)) {
            if (mod->mObject == object) {
                delete mod;
                return;
            }
        }
        if (AddObjectModifier *mod = dynamic_cast<AddObjectModifier*>(bmod)) {
            if (mod->mObject == object) {
                delete mod;
                return;
            }
        }
    }
}

void ShadowBuilding::changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room*> > &grid)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (ChangeFloorGridModifier *mod = dynamic_cast<ChangeFloorGridModifier*>(bmod)) {
            if (mod->mFloor == floor) {
                mod->setGrid(grid);
                return;
            }
        }
    }

    ChangeFloorGridModifier *mod = new ChangeFloorGridModifier(this, floor);
    mod->setGrid(grid);
}

void ShadowBuilding::resetFloorGrid(BuildingFloor *floor)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (ChangeFloorGridModifier *mod = dynamic_cast<ChangeFloorGridModifier*>(bmod)) {
            if (mod->mFloor == floor) {
                delete mod;
                return;
            }
        }
    }
}

void ShadowBuilding::changeUserTiles(BuildingFloor *floor, const QMap<QString,FloorTileGrid*> &tiles)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (ChangeUserTilesModifier *mod = dynamic_cast<ChangeUserTilesModifier*>(bmod)) {
            if (mod->mFloor == floor) {
                mod->setTiles(tiles);
                return;
            }
        }
    }

    ChangeUserTilesModifier *mod = new ChangeUserTilesModifier(this, floor);
    mod->setTiles(tiles);
}

void ShadowBuilding::resetUserTiles(BuildingFloor *floor)
{
    foreach (BuildingModifier *bmod, mModifiers) {
        if (ChangeUserTilesModifier *mod = dynamic_cast<ChangeUserTilesModifier*>(bmod)) {
            if (mod->mFloor == floor) {
                delete mod;
                return;
            }
        }
    }
}
