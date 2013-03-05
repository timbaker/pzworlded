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

#include "lotfilesmanager.h"

#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "preferences.h"
#include "progress.h"
#include "tilemetainfomgr.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "tile.h"
#include "tileset.h"

#include <qmath.h>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QRgb>

using namespace Tiled;

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++)
        out << quint8(str[i].toAscii());
    out << quint8('\n');
}

LotFilesManager *LotFilesManager::mInstance = 0;

LotFilesManager *LotFilesManager::instance()
{
    if (!mInstance)
        mInstance = new LotFilesManager();
    return mInstance;
}

void LotFilesManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

LotFilesManager::LotFilesManager(QObject *parent)
    : QObject(parent)
{
}

LotFilesManager::~LotFilesManager()
{
}

bool LotFilesManager::generateWorld(WorldDocument *worldDoc, GenerateMode mode)
{
    mWorldDoc = worldDoc;

    PROGRESS progress(QLatin1String("Reading Zombie Spawn Map"));

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
    QString spawnMap = lotSettings.zombieSpawnMap;
    if (!QFileInfo(spawnMap).exists()) {
        mError = tr("Couldn't find the Zombie Spawn Map image.\n%1")
                .arg(spawnMap);
        return false;
    }
    ZombieSpawnMap = QImage(spawnMap);
    if (ZombieSpawnMap.isNull()) {
        mError = tr("Couldn't read the Zombie Spawn Map image.\n%1")
                .arg(spawnMap);
        return false;
    }

    QString tilesDirectory = TileMetaInfoMgr::instance()->tilesDirectory();
    if (tilesDirectory.isEmpty() || !QFileInfo(tilesDirectory).exists()) {
        mError = tr("The Tiles Directory could not be found.  Please set it in the Tilesets Dialog in TileZed.");
        return false;
    }
#if 0
    if (!TileMetaInfoMgr::instance()->readTxt()) {
        mError = tr("%1\n(while reading %2)")
                .arg(TileMetaInfoMgr::instance()->errorString())
                .arg(TileMetaInfoMgr::instance()->txtName());
        return false;
    }
#endif

    mStats = LotFile::Stats();

    progress.update(QLatin1String("Generating .lot files"));

    World *world = worldDoc->world();

    if (mode == GenerateSelected) {
        foreach (WorldCell *cell, worldDoc->selectedCells())
            if (!generateCell(cell))
                return false;
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                if (!generateCell(world->cellAt(x, y)))
                    return false;
            }
        }
    }

    QString stats = tr("Finished!\n\nBuildings: %1\nRooms: %2\nRoom rects: %3\nRoom objects: %4")
            .arg(mStats.numBuildings)
            .arg(mStats.numRooms)
            .arg(mStats.numRoomRects)
            .arg(mStats.numRoomObjects);
    QMessageBox::information(MainWindow::instance(),
                             tr("Generate Lot Files"), stats);

    return true;
}

bool LotFilesManager::generateCell(WorldCell *cell)
{
//    if (cell->x() != 5 || cell->y() != 3) return true;
    if (cell->mapFilePath().isEmpty())
        return true;

    if (cell->x() * 30 + 30 > ZombieSpawnMap.width() ||
            cell->y() * 30 + 30 > ZombieSpawnMap.height()) {
        mError = tr("The Zombie Spawn Map doesn't cover cell %1,%2.")
                .arg(cell->x()).arg(cell->y());
        return false;
    }

#if 0
    // Don't regenerate the .lot files unless the cell's map is newer than
    // the .lot files.
    // TODO: check the modification time of all the lots included by the map.
    // TODO: check for roads changing
    {
    QFileInfo mapFileInfo(cell->mapFilePath());
    QString fileName = tr("world_%1_%2.lotpack")
            .arg(cell->x())
            .arg(cell->y());
    QString lotsDirectory = Preferences::instance()->lotsDirectory();
    QFileInfo lotFileInfo(lotsDirectory + QLatin1Char('/') + fileName);
    if (lotFileInfo.exists() && mapFileInfo.exists() &&
            (mapFileInfo.lastModified() < lotFileInfo.lastModified()))
        return true;
    }
#endif

    MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath());
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return false;
    }

    PROGRESS progress(tr("Generating .lot files (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));

    MapComposite staticMapComposite(mapInfo);
    MapComposite *mapComposite = &staticMapComposite;
    mapComposite->generateRoadLayers(QPoint(cell->x() * 300, cell->y() * 300),
                                     cell->world()->roads());

    foreach (WorldCellLot *lot, cell->lots()) {
        if (MapInfo *info = MapManager::instance()->loadMap(lot->mapName())) {
            mapComposite->addMap(info, lot->pos(), lot->level());
        } else {
            mError = MapManager::instance()->errorString();
            return false;
        }
    }

    // Check for missing tilesets.
    foreach (MapComposite *mc, mapComposite->maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            mError = tr("Some tilesets are missing in a map in cell %1,%2:\n%3")
                    .arg(cell->x()).arg(cell->y()).arg(mc->mapInfo()->path());
            return false;
        }
    }

    generateHeader(cell, mapComposite);

    MaxLevel = 15;

    int mapWidth = mapInfo->width();
    int mapHeight = mapInfo->height();

    // Resize the grid and cleanup data from the previous cell.
    mGridData.resize(mapWidth);
    for (int x = 0; x < mapWidth; x++) {
        mGridData[x].resize(mapHeight);
        for (int y = 0; y < mapHeight; y++)
            mGridData[x][y].fill(LotFile::Square(), MaxLevel);
    }

    QVector<const Tiled::Cell *> cells(40);
    foreach (CompositeLayerGroup *lg, mapComposite->layerGroups()) {
        lg->prepareDrawing2();
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                foreach (const Tiled::Cell *cell, cells) {
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    int lx = x, ly = y;
                    if (mapInfo->orientation() == Map::Isometric) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    mGridData[lx][ly][lg->level()].Entries.append(e);
                    TileMap[e->gid]->used = true;
                }
            }
        }
    }

    generateBuildingObjects(mapWidth, mapHeight);

    generateHeaderAux(cell, mapComposite);

    /////

    QString fileName = tr("world_%1_%2.lotpack")
            .arg(cell->x())
            .arg(cell->y());

    QString lotsDirectory = mWorldDoc->world()->getGenerateLotsSettings().exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    int WorldDiv = CELL_WIDTH / CHUNK_WIDTH;
    // C# 'long' is signed 64-bit integer
    out << qint32(WorldDiv * WorldDiv);
    for (int m = 0; m < WorldDiv * WorldDiv; m++)
        out << qint64(m);

    QList<qint64> PositionMap;

    for (int x = 0; x < mapInfo->width() / CHUNK_WIDTH; x++) {
        for (int y = 0; y < mapInfo->height() / CHUNK_HEIGHT; y++) {
            PositionMap += file.pos();
            if (!generateChunk(out, cell, mapComposite, x, y))
                return false;
        }
    }

    file.seek(4);
    for (int m = 0; m < WorldDiv * WorldDiv; m++)
        out << qint64(PositionMap[m]);

    file.close();

    return true;
}

bool LotFilesManager::generateHeader(WorldCell *cell, MapComposite *mapComposite)
{
    Q_UNUSED(cell)

    qDeleteAll(mRoomRects);
    qDeleteAll(roomList);
    qDeleteAll(buildingList);
    qDeleteAll(ZoneList);

    mRoomRects.clear();
    mRoomRectByLevel.clear();
    roomList.clear();
    buildingList.clear();
    ZoneList.clear();

    // Create the set of all tilesets used by the map and its sub-maps.
    QList<Tileset*> tilesets;
    foreach (MapComposite *mc, mapComposite->maps())
        tilesets += mc->map()->tilesets();

    qDeleteAll(TileMap.values());
    TileMap.clear();
    TileMap[0] = new LotFile::Tile;

    mTilesetToFirstGid.clear();
    uint firstGid = 1;
    foreach (Tileset *tileset, tilesets) {
        if (!handleTileset(tileset, firstGid))
            return false;
    }

    processObjectGroups(mapComposite);

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    foreach (int level, mRoomRectByLevel.keys()) {
        QList<LotFile::RoomRect*> rrList = mRoomRectByLevel[level];
        foreach (LotFile::RoomRect *rr, rrList) {
            if (rr->room == 0) {
                rr->room = new LotFile::Room(rr->nameWithoutSuffix(),
                                             rr->floor);
                rr->room->rects += rr;
                roomList += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#')))
                continue;
            foreach (LotFile::RoomRect *comp, rrList) {
                if (comp == rr)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != 0) {
                        LotFile::Room *room = comp->room;
                        foreach (LotFile::RoomRect *rr2, room->rects) {
                            Q_ASSERT(rr2->room == room);
                            Q_ASSERT(!rr->room->rects.contains(rr2));
                            rr2->room = rr->room;
                        }
                        rr->room->rects += room->rects;
                        roomList.removeOne(room);
                        delete room;
                    } else {
                        comp->room = rr->room;
                        rr->room->rects += comp;
                        Q_ASSERT(rr->room->rects.count(comp) == 1);
                    }
                }
            }
        }
    }
    for (int i = 0; i < roomList.size(); i++)
        roomList[i]->ID = i;
    mStats.numRoomRects += mRoomRects.size();
    mStats.numRooms += roomList.size();

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    foreach (LotFile::Room *r, roomList) {
        if (r->building == 0) {
            r->building = new LotFile::Building();
            buildingList += r->building;
            r->building->RoomList += r;
        }
        foreach (LotFile::Room *comp, roomList) {
            if (comp == r)
                continue;
            if (r->building == comp->building)
                continue;

            if (r->inSameBuilding(comp)) {
                if (comp->building != 0) {
                    LotFile::Building *b = comp->building;
                    foreach (LotFile::Room *r2, b->RoomList) {
                        Q_ASSERT(r2->building == b);
                        Q_ASSERT(!r->building->RoomList.contains(r2));
                        r2->building = r->building;
                    }
                    r->building->RoomList += b->RoomList;
                    buildingList.removeOne(b);
                    delete b;
                } else {
                    comp->building = r->building;
                    r->building->RoomList += comp;
                    Q_ASSERT(r->building->RoomList.count(comp) == 1);
                }
            }
        }
    }
    mStats.numBuildings += buildingList.size();

    return true;
}

bool LotFilesManager::generateHeaderAux(WorldCell *cell, MapComposite *mapComposite)
{
    Q_UNUSED(mapComposite)

    QString fileName = tr("%1_%2.lotheader")
            .arg(cell->x())
            .arg(cell->y());

    QString lotsDirectory = mWorldDoc->world()->getGenerateLotsSettings().exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    Version = 0;
    out << qint32(Version);

    int tilecount = 0;
    foreach (LotFile::Tile *tile, TileMap) {
        if (tile->used) {
            tile->id = tilecount;
            tilecount++;
        }
    }
    out << qint32(tilecount);

    foreach (LotFile::Tile *tile, TileMap) {
        if (tile->used) {
            SaveString(out, tile->name);
        }
    }

    out << quint8(0);
    out << qint32(CHUNK_WIDTH);
    out << qint32(CHUNK_HEIGHT);
    out << qint32(MaxLevel);

    out << qint32(roomList.count());
    foreach (LotFile::Room *room, roomList) {
        SaveString(out, room->name);
        out << qint32(room->floor);

        out << qint32(room->rects.size());
        foreach (LotFile::RoomRect *rr, room->rects) {
            out << qint32(rr->x);
            out << qint32(rr->y);
            out << qint32(rr->w);
            out << qint32(rr->h);
        }

        out << qint32(room->objects.size());
        foreach (const LotFile::RoomObject &object, room->objects) {
            out << qint32(object.metaEnum);
            out << qint32(object.x);
            out << qint32(object.y);
        }
    }

    out << qint32(buildingList.count());
    foreach (LotFile::Building *building, buildingList) {
        out << qint32(building->RoomList.count());
        foreach (LotFile::Room *room, building->RoomList) {
            out << qint32(room->ID);
        }
    }

    for (int x = 0; x < 30; x++) {
        for (int y = 0; y < 30; y++) {
            QRgb pixel = ZombieSpawnMap.pixel(cell->x() * 30 + x,
                                              cell->y() * 30 + y);
            out << quint8(qRed(pixel));
        }
    }

#if 0
    out << qint32(ZoneList.count());
    foreach (LotFile::Zone *zone, ZoneList) {
        SaveString(out, zone->name);
        SaveString(out, zone->val);
        out << qint32(zone->x);
        out << qint32(zone->y);
        out << qint32(zone->z);
        out << qint32(zone->width);
        out << qint32(zone->height);
    }
#endif

    file.close();

    return true;
}

bool LotFilesManager::generateChunk(QDataStream &out, WorldCell *cell,
                                    MapComposite *mapComposite, int cx, int cy)
{
    Q_UNUSED(cell)
    Q_UNUSED(mapComposite)

    int notdonecount = 0;
    for (int z = 0; z < MaxLevel; z++)  {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int gx = cx * CHUNK_WIDTH + x;
                int gy = cy * CHUNK_HEIGHT + y;
                const QList<LotFile::Entry*> &entries = mGridData[gx][gy][z].Entries;
                if (entries.count() == 0)
                    notdonecount++;
                else {
                    if (notdonecount > 0) {
                        out << qint32(-1);
                        out << qint32(notdonecount);
                    }
                    notdonecount = 0;
                    out << qint32(entries.count() + 1);
                    out << qint32(getRoomID(gx, gy, z));
                }
                foreach (LotFile::Entry *entry, entries) {
                    Q_ASSERT(TileMap[entry->gid]);
                    Q_ASSERT(TileMap[entry->gid]->id != -1);
                    out << qint32(TileMap[entry->gid]->id);
                }
            }
        }
    }
    if (notdonecount > 0) {
        out << qint32(-1);
        out << qint32(notdonecount);
    }

    return true;
}

void LotFilesManager::generateBuildingObjects(int mapWidth, int mapHeight)
{
    foreach (LotFile::Room *room, roomList) {
        foreach (LotFile::RoomRect *rr, room->rects)
            generateBuildingObjects(mapWidth, mapHeight, room, rr);
    }
}

void LotFilesManager::generateBuildingObjects(int mapWidth, int mapHeight,
                                              LotFile::Room *room, LotFile::RoomRect *rr)
{
    for (int x = rr->x; x < rr->x + rr->w; x++) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {

            // Remember the room at each position in the map.
            mGridData[x][y][room->floor].roomID = room->ID;

            /* Examine every tile inside the room.  If the tile's metaEnum >= 0
               then create a new RoomObject for it. */
            foreach (LotFile::Entry *entry, mGridData[x][y][room->floor].Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y;
                    object.metaEnum = metaEnum;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check south of the room for doors.
    int y = rr->y + rr->h;
    if (y < mapHeight) {
        for (int x = rr->x; x < rr->x + rr->w; x++) {
            foreach (LotFile::Entry *entry, mGridData[x][y][room->floor].Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumNorth(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y - 1;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check east of the room for doors.
    int x = rr->x + rr->w;
    if (x < mapWidth) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {
            foreach (LotFile::Entry *entry, mGridData[x][y][room->floor].Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumWest(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x - 1;
                    object.y = y;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }
}

static QString nameOfTileset(const Tileset *tileset)
{
    QString name = tileset->imageSource();
    if (name.contains(QLatin1String("/")))
        name = name.mid(name.lastIndexOf(QLatin1String("/")) + 1);
    name.replace(QLatin1String(".png"), QLatin1String(""));
    return name;
}

bool LotFilesManager::handleTileset(const Tiled::Tileset *tileset, uint &firstGid)
{
    if (!tileset->fileName().isEmpty()) {
        mError = tr("Only tileset image files supported, not external tilesets");
        return false;
    }

    QString name = nameOfTileset(tileset);

    // TODO: Verify that two tilesets sharing the same name are identical
    // between maps.
    QMap<Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end) {
        QString name2 = nameOfTileset(i.key());
        if (name == name2) {
            mTilesetToFirstGid.insert(tileset, i.value());
            return true;
        }
        ++i;
    }

    for (int i = 0; i < tileset->tileCount(); ++i) {
        int localID = i;
        int ID = firstGid + localID;
        LotFile::Tile *tile = new LotFile::Tile(name + QLatin1String("_") + QString::number(localID));
        tile->metaEnum = TileMetaInfoMgr::instance()->tileEnumValue(tileset->tileAt(i));
        TileMap[ID] = tile;
    }

    mTilesetToFirstGid.insert(tileset, firstGid);
    firstGid += tileset->tileCount();

    return true;
}

int LotFilesManager::getRoomID(int x, int y, int z)
{
    return mGridData[x][y][z].roomID;
#if 0
    int n = 0;
    foreach (LotFile::Room *room, roomList) {
        if (room->floor != z)
            continue;

        if ((x >= room->x && x < room->x + room->w) &&
                (y >= room->y && y < room->y + room->h))
            return n;

        n++;
    }

    return -1;
#endif
}

uint LotFilesManager::cellToGid(const Cell *cell)
{
    Tileset *tileset = cell->tile->tileset();

    QMap<Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end && i.key() != tileset)
        ++i;
    if (i == i_end) // tileset not found
        return 0;

    return i.value() + cell->tile->id();
}

void LotFilesManager::processObjectGroups(MapComposite *mapComposite)
{
    foreach (Layer *layer, mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup())
            processObjectGroup(og, mapComposite->levelRecursive(),
                               mapComposite->originRecursive());
    }

    foreach (MapComposite *subMap, mapComposite->subMaps())
        processObjectGroups(subMap);
}

void LotFilesManager::processObjectGroup(ObjectGroup *objectGroup,
                                         int levelOffset, const QPoint &offset)
{
    int level;
    if (!MapComposite::levelForLayer(objectGroup, &level))
        return;
    level += levelOffset;

    foreach (const MapObject *mapObject, objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (!mapObject->width() || !mapObject->height())
            continue;

        int x = qFloor(mapObject->x());
        int y = qFloor(mapObject->y());
        int w = qCeil(mapObject->x() + mapObject->width()) - x;
        int h = qCeil(mapObject->y() + mapObject->height()) - y;

        QString name = mapObject->name();
        if (name.isEmpty())
            name = QLatin1String("unnamed");

        if (objectGroup->map()->orientation() == Map::Isometric) {
            x += 3 * level;
            y += 3 * level;
        }

        // Apply the MapComposite offset in the top-level map.
        x += offset.x();
        y += offset.y();

        if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
            LotFile::RoomRect *rr = new LotFile::RoomRect(name, x, y, level,
                                                          w, h);
            mRoomRects += rr;
            mRoomRectByLevel[level] += rr;
        } else {
            LotFile::Zone *z = new LotFile::Zone(name,
                                                 mapObject->type(),
                                                 x, y, level, w, h);
            ZoneList.append(z);
        }
    }
}
