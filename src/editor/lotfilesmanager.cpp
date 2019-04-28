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

#include "bmpblender.h"
#include "mainwindow.h"
#include "mapasset.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "preferences.h"
#include "progress.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "navigation/chunkdatafile.h"
#include "navigation/isogridsquare.h"

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
    for (int i = 0; i < str.length(); i++) {
        if (str[i].toLatin1() == '\n') continue;
        out << quint8(str[i].toLatin1());
    }
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

    if (!Navigate::IsoGridSquare::loadTileDefFiles(lotSettings, mError)) {
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

#if 1
    PROGRESS progress(tr("Loading maps (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));

    MapAsset *mapAsset = MapManager::instance().loadMap(cell->mapFilePath(),
                                                       QString(), true);
    if (!mapAsset) {
        mError = MapManager::instance().errorString();
        return false;
    }

    DelayedMapLoader mapLoader;
    mapLoader.addMap(mapAsset);

    foreach (WorldCellLot *lot, cell->lots()) {
        if (MapAsset *info = MapManager::instance().loadMap(lot->mapName(),
                                                            QString(), true,
                                                            MapManager::PriorityMedium)) {
            mapLoader.addMap(info);
        } else {
            mError = MapManager::instance().errorString();
            return false;
        }
    }

    // The cell map must be loaded before creating the MapComposite, which will
    // possibly load embedded lots.
    while (mapAsset->isEmpty())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    MapComposite staticMapComposite(mapAsset);
    MapComposite *mapComposite = &staticMapComposite;
    while (mapComposite->waitingForMapsToLoad() || mapLoader.isLoading())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    if (!mapLoader.errorString().isEmpty()) {
        mError = mapLoader.errorString();
        return false;
    }

    for (WorldCellLot *lot : cell->lots()) {
        MapAsset *info = MapManager::instance().loadMap(lot->mapName());
        Q_ASSERT(info && info->map() && info->isReady());
        mapComposite->addMap(info, lot->pos(), lot->level());
    }

    mapComposite->generateRoadLayers(QPoint(cell->x() * 300, cell->y() * 300),
                                     cell->world()->roads());

    progress.update(tr("Generating .lot files (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));
#else
    MapInfo *mapInfo = MapManager::instance().loadMap(cell->mapFilePath());
    if (!mapInfo) {
        mError = MapManager::instance().errorString();
        return false;
    }

    PROGRESS progress(tr("Generating .lot files (%1,%2)")
                      .arg(cell->x()).arg(cell->y()));

    MapComposite staticMapComposite(mapInfo);
    MapComposite *mapComposite = &staticMapComposite;
    while (mapComposite->waitingForMapsToLoad())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    mapComposite->generateRoadLayers(QPoint(cell->x() * 300, cell->y() * 300),
                                     cell->world()->roads());

    foreach (WorldCellLot *lot, cell->lots()) {
        if (MapInfo *info = MapManager::instance().loadMap(lot->mapName())) {
            mapComposite->addMap(info, lot->pos(), lot->level());
        } else {
            mError = MapManager::instance().errorString();
            return false;
        }
    }
#endif


    // Check for missing tilesets.
    foreach (MapComposite *mc, mapComposite->maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            mError = tr("Some tilesets are missing in a map in cell %1,%2:\n%3")
                    .arg(cell->x()).arg(cell->y()).arg(mc->mapAsset()->path());
            return false;
        }
    }

    if (!generateHeader(cell, mapComposite))
        return false;

    bool chunkDataOnly = false;
    if (chunkDataOnly) {
        foreach (CompositeLayerGroup *lg, mapComposite->layerGroups()) {
            lg->prepareDrawing2();
        }
        const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
        Navigate::ChunkDataFile cdf;
        cdf.fromMap(cell->x(), cell->y(), mapComposite, mRoomRectByLevel[0], lotSettings);
        return true;
    }

    MaxLevel = 15;

    int mapWidth = mapAsset->width();
    int mapHeight = mapAsset->height();

    // Resize the grid and cleanup data from the previous cell.
    mGridData.resize(mapWidth);
    for (int x = 0; x < mapWidth; x++) {
        mGridData[x].resize(mapHeight);
        for (int y = 0; y < mapHeight; y++)
            mGridData[x][y].fill(LotFile::Square(), MaxLevel);
    }

    Tile *missingTile = Tiled::Internal::TilesetManager::instance().missingTile();
    QVector<const Tiled::Cell *> cells(40);
    foreach (CompositeLayerGroup *lg, mapComposite->layerGroups()) {
        lg->prepareDrawing2();
        int d = (mapAsset->orientation() == Map::Isometric) ? -3 : 0;
        d *= lg->level();
        for (int y = d; y < mapHeight; y++) {
            for (int x = d; x < mapWidth; x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                foreach (const Tiled::Cell *cell, cells) {
                    if (cell->tile == missingTile) continue;
                    int lx = x, ly = y;
                    if (mapAsset->orientation() == Map::Isometric) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    if (lx >= mapWidth) continue;
                    if (ly >= mapHeight) continue;
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    mGridData[lx][ly][lg->level()].Entries.append(e);
                    TileMap[e->gid]->used = true;
                }
            }
        }
    }

    generateBuildingObjects(mapWidth, mapHeight);

    generateJumboTrees(cell, mapComposite);

    generateHeaderAux(cell, mapComposite);

    /////

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    QString fileName = tr("world_%1_%2.lotpack")
            .arg(lotSettings.worldOrigin.x() + cell->x())
            .arg(lotSettings.worldOrigin.y() + cell->y());

    QString lotsDirectory = lotSettings.exportDir;
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

    for (int x = 0; x < mapAsset->width() / CHUNK_WIDTH; x++) {
        for (int y = 0; y < mapAsset->height() / CHUNK_HEIGHT; y++) {
            PositionMap += file.pos();
            if (!generateChunk(out, cell, mapComposite, x, y))
                return false;
        }
    }

    file.seek(4);
    for (int m = 0; m < WorldDiv * WorldDiv; m++)
        out << qint64(PositionMap[m]);

    file.close();

    Navigate::ChunkDataFile cdf;
    cdf.fromMap(cell->x(), cell->y(), mapComposite, mRoomRectByLevel[0], lotSettings);

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

    mJumboTreeTileset = 0;
    if (mJumboTreeTileset == 0) {
        mJumboTreeTileset = new Tiled::Tileset(QLatin1Literal("jumbo_tree_01"), 64, 128);
        mJumboTreeTileset->loadFromNothing(QSize(64, 128), QLatin1String("jumbo_tree_01"));
    }
    tilesets += mJumboTreeTileset;
    QScopedPointer<Tiled::Tileset> scoped(mJumboTreeTileset);

    qDeleteAll(TileMap.values());
    TileMap.clear();
    TileMap[0] = new LotFile::Tile;

    mTilesetToFirstGid.clear();
    uint firstGid = 1;
    foreach (Tileset *tileset, tilesets) {
        if (!handleTileset(tileset, firstGid))
            return false;
    }

    if (!processObjectGroups(cell, mapComposite))
        return false;

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

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    QString fileName = tr("%1_%2.lotheader")
            .arg(lotSettings.worldOrigin.x() + cell->x())
            .arg(lotSettings.worldOrigin.y() + cell->y());

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
            if (tile->name.startsWith(QLatin1Literal("jumbo_tree_01"))) {
                int nnn = 0;
            }
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
    out << qint32(cell->objects().size());
    foreach (WorldCellObject *o, cell->objects()) {
        SaveString(out, o->name());
        SaveString(out, o->type()->name());
        out << qint32(o->x());
        out << qint32(o->y());
        out << qint32(o->level());
        out << qint32(o->width());
        out << qint32(o->height());
        PropertyList properties;
        resolveProperties(o, properties);
        out << qint32(properties.size());
        foreach (Property *p, properties) {
            SaveString(out, p->mDefinition->mName);
            SaveString(out, p->mValue);
        }
    }
#endif

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

    int wx = cell->x() * 300 + cx * CHUNK_WIDTH;
    int wy = cell->y() * 300 + cy * CHUNK_HEIGHT;

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

void LotFilesManager::generateJumboTrees(WorldCell *cell, MapComposite *mapComposite)
{
    const quint8 JUMBO_ZONE = 1;
    const quint8 PREVENT_JUMBO = 2;
    const quint8 REMOVE_TREE = 3;
    const quint8 JUMBO_TREE = 4;

    QSet<QString> treeTiles;
    QSet<QString> floorVegTiles;
    foreach (TileDefFile *tdf, Navigate::IsoGridSquare::mTileDefFiles) {
        foreach (TileDefTileset *tdts, tdf->tilesets()) {
            foreach (TileDefTile *tdt, tdts->mTiles) {
                // Get the set of all tree tiles.
                if (tdt->mProperties.contains(QLatin1Literal("tree")) || (tdts->mName.startsWith(QLatin1Literal("vegetation_trees")))) {
                    treeTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
                // Get the set of all floor + vegetation tiles.
                if (tdt->mProperties.contains(QLatin1Literal("solidfloor")) ||
                        tdt->mProperties.contains(QLatin1Literal("FloorOverlay")) ||
                        tdt->mProperties.contains(QLatin1Literal("vegitation"))) {
                    floorVegTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
            }
        }
    }

    quint8 grid[300][300];
    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }

    // Allow jumbo trees in Forest and DeepForest zones
    foreach (WorldCellObject *obj, cell->objects()) {
        if (obj->level() == 0 && obj->type() != 0 && obj->type()->name().contains(QLatin1Literal("Forest"))) {
            int ox = obj->x(), oy = obj->y(), ow = obj->width(), oh = obj->height();
            for (int y = oy; y < oy + oh; y++) {
                for (int x = ox; x < ox + ow; x++) {
                    if (x >= 0 && x < 300 && y >= 0 && y < 300)
                        grid[x][y] = JUMBO_ZONE;
                }
            }
        }
    }

    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            // Prevent jumbo trees near any second-story tiles
            if (!mGridData[x][y][1].Entries.isEmpty()) {
                for (int yy = y; yy <= y + 4; yy++) {
                    for (int xx = x; xx <= x + 4; xx++) {
                        if (xx >= 0 && xx < 300 && yy >= 0 && yy < 300)
                            grid[xx][yy] = PREVENT_JUMBO;
                    }
                }
            }

            // Prevent jumbo trees near non-floor, non-vegetation (fences, etc)
            foreach (LotFile::Entry *e, mGridData[x][y][0].Entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (!floorVegTiles.contains(tile->name)) {
                    for (int yy = y - 1; yy <= y + 1; yy++) {
                        for (int xx = x - 1; xx <= x + 1; xx++) {
                            if (xx >= 0 && xx < 300 && yy >= 0 && yy < 300)
                                grid[xx][yy] = PREVENT_JUMBO;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Prevent jumbo trees near north/west edges of cells
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 300; x++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 300; y++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }

#if 0
    // Prevent jumbo trees near second-story rooms.
    foreach (LotFile::Room *room, roomList) {
//        if (room->floor != 1)
//            continue;
        foreach (LotFile::RoomRect *rr, room->rects) {
            for (int y = rr->y - 1; y < rr->y + rr->h + 4 + 1; y++) {
                for (int x = rr->x - 1; x < rr->x + rr->w + 4 + 1; x++) {
                    if (x >= 0 && x < 300 && y >= 0 && y < 300)
                        grid[x][y] = PREVENT_JUMBO;
                }
            }
        }
    }
#endif

    // Get a list of all tree positions in the cell.
    QList<QPoint> allTreePos;
    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            foreach (LotFile::Entry *e, mGridData[x][y][0].Entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (treeTiles.contains(tile->name)) {
                    allTreePos += QPoint(x, y);
                    break;
                }
            }
        }
    }

    while (!allTreePos.isEmpty()) {
        int r = qrand() % allTreePos.size();
        QPoint treePos = allTreePos.takeAt(r);
        quint8 g = grid[treePos.x()][treePos.y()];
        if (g == JUMBO_ZONE) {
            grid[treePos.x()][treePos.y()] = JUMBO_TREE;
            // Remove all trees surrounding a jumbo tree.
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0)
                        continue;
                    int x = treePos.x() + dx;
                    int y = treePos.y() + dy;
                    if (x >= 0 && x < 300 && y >= 0 && y < 300)
                        grid[x][y] = REMOVE_TREE;
                }
            }
        }
    }

    for (int y = 0; y < 300; y++) {
        for (int x = 0; x < 300; x++) {
            if (grid[x][y] == JUMBO_TREE) {
                foreach (LotFile::Entry *e, mGridData[x][y][0].Entries) {
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        e->gid = mTilesetToFirstGid[mJumboTreeTileset];
                        TileMap[e->gid]->used = true;
                        break;
                    }
                }
            }
            if (grid[x][y] == REMOVE_TREE) {
                for (int i = 0; i < mGridData[x][y][0].Entries.size(); i++) {
                    LotFile::Entry *e = mGridData[x][y][0].Entries[i];
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        mGridData[x][y][0].Entries.removeAt(i);
                        break;
                    }
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
    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
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

    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end && i.key() != tileset)
        ++i;
    if (i == i_end) // tileset not found
        return 0;

    return i.value() + cell->tile->id();
}

bool LotFilesManager::processObjectGroups(WorldCell *cell, MapComposite *mapComposite)
{
    foreach (Layer *layer, mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (!processObjectGroup(cell, og, mapComposite->levelRecursive(),
                                    mapComposite->originRecursive()))
                return false;
        }
    }

    foreach (MapComposite *subMap, mapComposite->subMaps())
        if (!processObjectGroups(cell, subMap))
            return false;

    return true;
}

bool LotFilesManager::processObjectGroup(WorldCell *cell, ObjectGroup *objectGroup,
                                         int levelOffset, const QPoint &offset)
{
    int level;
    if (!MapComposite::levelForLayer(objectGroup, &level))
        return true;
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
            if (x < 0 || y < 0 || x + w > 300 || y + h > 300) {
                x = qBound(0, x, 300);
                y = qBound(0, y, 300);
                mError = tr("A RoomDef in cell %1,%2 overlaps cell boundaries.\nNear x,y=%3,%4")
                        .arg(cell->x()).arg(cell->y()).arg(x).arg(y);
                return false;
            }
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
    return true;
}

void LotFilesManager::resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    foreach (PropertyTemplate *pt, ph->templates())
        resolveProperties(pt, result);
    foreach (Property *p, ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

/////

DelayedMapLoader::DelayedMapLoader()
{
    connect(MapManager::instancePtr(), &MapManager::mapLoaded,
            this, &DelayedMapLoader::mapLoaded);
    connect(MapManager::instancePtr(), &MapManager::mapFailedToLoad,
            this, &DelayedMapLoader::mapFailedToLoad);
}

void DelayedMapLoader::addMap(MapAsset *info)
{
    mLoading += new SubMapLoading(info);
}

bool DelayedMapLoader::isLoading()
{
    for (int i = 0; i < mLoading.size(); i++) {
        if (mLoading[i]->mapAsset->isEmpty())
            return true;
    }
    return false;
}

void DelayedMapLoader::mapLoaded(MapAsset *mapAsset)
{
    for (int i = 0; i < mLoading.size(); i++) {
        SubMapLoading *sml = mLoading[i];
        if (sml->mapAsset == mapAsset) {
            mLoaded += new SubMapLoading(mapAsset); // add reference
            delete mLoading.takeAt(i);
            --i;
            // Keep going, could be duplicate submaps to load
        }
    }
}

void DelayedMapLoader::mapFailedToLoad(MapAsset *mapAsset)
{
    for (int i = 0; i < mLoading.size(); i++) {
        if (mLoading[i]->mapAsset == mapAsset) {
            mError = MapManager::instance().errorString();
            delete mLoading.takeAt(i);
            --i;
            // Keep going, could be duplicate submaps to load
        }
    }
}

/////

DelayedMapLoader::SubMapLoading::SubMapLoading(MapAsset *info) :
    mapAsset(info), holdsReference(false)
{
    if (mapAsset->map()) {
        MapManager::instance().addReferenceToMap(mapAsset);
        holdsReference = true;
    }
}

DelayedMapLoader::SubMapLoading::~SubMapLoading()
{
    if (holdsReference)
        MapManager::instance().removeReferenceToMap(mapAsset);
}
