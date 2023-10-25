/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#include "lotfilesmanager256.h"

#include "generatelotsfailuredialog.h"
#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "progress.h"
#include "tiledeffile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "world.h"
#include "worldcell.h"
#include "worldconstants.h"
#include "worlddocument.h"

#include "InGameMap/clipper.hpp"

#include "navigation/chunkdatafile256.h"
#include "navigation/isogridsquare256.h"

#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRandomGenerator>

using namespace Tiled;

static int VERSION1 = 1; // Added 4-byte 'LOTH' at start of .lotheader files.
                         // Added 4-byte 'LOTP' at start of .lotpack files, followed by 4-byte version number.

static int VERSION_LATEST = VERSION1;

static QString nameOfTileset(const Tileset *tileset)
{
    QString name = tileset->imageSource();
    if (name.contains(QLatin1String("/")))
        name = name.mid(name.lastIndexOf(QLatin1String("/")) + 1);
    name.replace(QLatin1String(".png"), QLatin1String(""));
    return name;
}

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++) {
        if (str[i].toLatin1() == '\n') continue;
        out << quint8(str[i].toLatin1());
    }
    out << quint8('\n');
}

namespace {
struct GenerateCellFailure
{
    WorldCell* cell;
    QString error;

    GenerateCellFailure(WorldCell* cell, const QString& error)
        : cell(cell)
        , error(error)
    {
    }
};
}

/////

LotFilesManager256 *LotFilesManager256::mInstance = nullptr;

LotFilesManager256 *LotFilesManager256::instance()
{
    if (mInstance == nullptr) {
        mInstance = new LotFilesManager256();
    }
    return mInstance;
}

void LotFilesManager256::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

LotFilesManager256::LotFilesManager256(QObject *parent)
    : QObject(parent)
{
    mJumboZoneList += new JumboZone(QStringLiteral("DeepForest"), 100);
    mJumboZoneList += new JumboZone(QStringLiteral("Farm"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("FarmLand"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("Forest"), 50);
    mJumboZoneList += new JumboZone(QStringLiteral("TownZone"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("Vegitation"), 10);
}

LotFilesManager256::~LotFilesManager256()
{
}

bool LotFilesManager256::generateWorld(WorldDocument *worldDoc, GenerateMode mode)
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

    if (!Navigate::IsoGridSquare256::loadTileDefFiles(lotSettings, mError)) {
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

    // A single 300x300 cell may overlap 4, 6, or 9 256x256 cells.
    mDoneCells256.clear();

    QList<GenerateCellFailure> failures;

    if (mode == GenerateSelected) {
        for (WorldCell *cell : worldDoc->selectedCells()) {
            if (!generateCell(cell)) {
                failures += GenerateCellFailure(cell, mError);
//                return false;
            }
        }
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell* cell = world->cellAt(x, y);
                if (!generateCell(cell)) {
                    failures += GenerateCellFailure(cell, mError);
//                    return false;
                }
            }
        }
    }

    progress.release();

    if (!failures.isEmpty()) {
        QStringList errorList;
        for (GenerateCellFailure failure : failures) {
            errorList += QString(QStringLiteral("Cell %1,%2: %3")).arg(failure.cell->x()).arg(failure.cell->y()).arg(failure.error);
        }
        GenerateLotsFailureDialog dialog(errorList, MainWindow::instance());
        dialog.exec();
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

bool LotFilesManager256::generateCell(WorldCell *cell)
{
    if (cell == nullptr)
        return true;

    if (cell->mapFilePath().isEmpty())
        return true;

    if ((cell->x() + 1) * CHUNKS_PER_CELL > ZombieSpawnMap.width() ||
            (cell->y() + 1) * CHUNKS_PER_CELL > ZombieSpawnMap.height()) {
        mError = tr("The Zombie Spawn Map doesn't cover cell %1,%2.")
                .arg(cell->x()).arg(cell->y());
        return false;
    }

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    int cell300X = lotSettings.worldOrigin.x() + cell->x();
    int cell300Y = lotSettings.worldOrigin.y() + cell->y();
    int minCell256X = std::floor(cell300X * CELL_WIDTH / float(CELL_SIZE_256));
    int minCell256Y = std::floor(cell300Y * CELL_HEIGHT / float(CELL_SIZE_256));
    int maxCell256X = std::ceil(((cell300X + 1) * CELL_WIDTH - 1) / float(CELL_SIZE_256));
    int maxCell256Y = std::ceil(((cell300Y + 1) * CELL_HEIGHT - 1) / float(CELL_SIZE_256));
    for (int cell256Y = minCell256Y; cell256Y < maxCell256Y; cell256Y++) {
        for (int cell256X = minCell256X; cell256X < maxCell256X; cell256X++) {
            QPair<int, int> doneCell(cell256X, cell256Y);
            if (mDoneCells256.contains(doneCell)) {
                continue;
            }
            mDoneCells256.insert(doneCell);
            if (generateCell(cell256X, cell256Y) == false) {
                return false;
            }
        }
    }
    return true;
}

bool LotFilesManager256::generateCell(int cell256X, int cell256Y)
{
    PROGRESS progress(tr("Loading maps (%1,%2)").arg(cell256X).arg(cell256Y));

    CombinedCellMaps combinedMaps;
    combinedMaps.load(mWorldDoc, cell256X, cell256Y);
    if (combinedMaps.mError.isEmpty() == false) {
        mError = combinedMaps.mError;
        return false;
    }
    MapComposite* mapComposite = combinedMaps.mMapComposite;
    MapInfo* mapInfo = mapComposite->mapInfo();

    progress.update(tr("Generating .lot files (%1,%2)").arg(cell256X).arg(cell256Y));

    // Check for missing tilesets.
    for (MapComposite *mc : mapComposite->maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            mError = tr("Some tilesets are missing in a map in cell %1,%2:\n%3").arg(cell256X).arg(cell256Y).arg(mc->mapInfo()->path());
            return false;
        }
    }

    if (generateHeader(combinedMaps, mapComposite) == false) {
        return false;
    }
#if 0
    bool chunkDataOnly = false;
    if (chunkDataOnly) {
        for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
            lg->prepareDrawing2();
        }
        const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
        Navigate::ChunkDataFile cdf;
        cdf.fromMap(cell->x(), cell->y(), mapComposite, mRoomRectByLevel[0], lotSettings);
        return true;
    }
#endif
    int mapWidth = combinedMaps.mCellsWidth * CELL_WIDTH;
    int mapHeight = combinedMaps.mCellsHeight * CELL_HEIGHT;

    // Resize the grid and cleanup data from the previous cell.
    mGridData.resize(mapWidth);
    for (int x = 0; x < mapWidth; x++) {
        mGridData[x].resize(mapHeight);
        for (int y = 0; y < mapHeight; y++) {
            mGridData[x][y].fill(LotFile::Square(), MAX_WORLD_LEVELS);
        }
    }

    // FIXME: This is for all the 300x300 cells, not just the single 256x256 cell.
    mMinLevel = 10000;
    mMaxLevel = -10000;

    Tile *missingTile = Tiled::Internal::TilesetManager::instance()->missingTile();
    QRect cellBounds256(cell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH,
                        cell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_WIDTH,
                        CELL_SIZE_256, CELL_SIZE_256);
    QVector<const Tiled::Cell *> cells(40);
    for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
        lg->prepareDrawing2();
        int d = (mapInfo->orientation() == Map::Isometric) ? -3 : 0;
        d *= lg->level();
        for (int y = d; y < mapHeight; y++) {
            for (int x = d; x < mapWidth; x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                for (const Tiled::Cell *cell : qAsConst(cells)) {
                    if (cell->tile == missingTile) continue;
                    int lx = x, ly = y;
                    if (mapInfo->orientation() == Map::Isometric) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    if (lx >= mapWidth) continue;
                    if (ly >= mapHeight) continue;
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    mGridData[lx][ly][lg->level() - MIN_WORLD_LEVEL].Entries.append(e);
                    if (cellBounds256.contains(lx, ly)) {
                        TileMap[e->gid]->used = true;
                        mMinLevel = std::min(mMinLevel, lg->level());
                        mMaxLevel = std::max(mMaxLevel, lg->level());
                    }
                }
            }
        }
    }

    if (mMinLevel == 10000) {
        mMinLevel = mMaxLevel = 0;
    }

    generateBuildingObjects(mapWidth, mapHeight);

    generateJumboTrees(combinedMaps);

    generateHeaderAux(cell256X, cell256Y);

    /////

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    QString fileName = tr("world_%1_%2.lotpack")
            .arg(cell256X)
            .arg(cell256Y);

    QString lotsDirectory = lotSettings.exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('L') << quint8('O') << quint8('T') << quint8('P');

    out << qint32(VERSION_LATEST);

    // C# 'long' is signed 64-bit integer
    out << qint32(CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256);
    for (int m = 0; m < CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256; m++) {
        out << qint64(m);
    }

    QList<qint64> PositionMap;

    for (int x = 0; x < CHUNKS_PER_CELL_256; x++) {
        for (int y = 0; y < CHUNKS_PER_CELL_256; y++) {
            PositionMap += file.pos();
            int chunkX = cell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH + x * CHUNK_SIZE_256;
            int chunkY = cell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT + y * CHUNK_SIZE_256;
            if (generateChunk(out, chunkX, chunkY) == false) {
                return false;
            }
        }
    }

    file.seek(4 + 4 + 4); // 'LOTS' + version + #chunks
    for (int m = 0; m < CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256; m++) {
        out << qint64(PositionMap[m]);
    }

    file.close();

    mRoomRectLookup.clear(CHUNKS_PER_CELL_256, CHUNKS_PER_CELL_256);
    for (LotFile::RoomRect *rr : mRoomRectByLevel[0]) {
        mRoomRectLookup.add(rr, rr->bounds());
    }
    Navigate::ChunkDataFile256 cdf;
    cdf.fromMap(combinedMaps, mapComposite, mRoomRectLookup, lotSettings);

    return true;
}

bool LotFilesManager256::generateHeader(CombinedCellMaps& combinedMaps, MapComposite *mapComposite)
{
    qDeleteAll(mRoomRects);
    qDeleteAll(roomList);
    qDeleteAll(buildingList);

    mRoomRects.clear();
    mRoomRectByLevel.clear();
    roomList.clear();
    buildingList.clear();

    // Create the set of all tilesets used by the map and its sub-maps.
    QList<Tileset*> tilesets;
    for (MapComposite *mc : mapComposite->maps())
        tilesets += mc->map()->tilesets();

    mJumboTreeTileset = nullptr;
    if (mJumboTreeTileset == nullptr) {
        mJumboTreeTileset = new Tiled::Tileset(QLatin1String("jumbo_tree_01"), 64, 128);
        mJumboTreeTileset->loadFromNothing(QSize(64, 128), QLatin1String("jumbo_tree_01"));
    }
    tilesets += mJumboTreeTileset;
    QScopedPointer<Tiled::Tileset> scoped(mJumboTreeTileset);

    qDeleteAll(TileMap.values());
    TileMap.clear();
    TileMap[0] = new LotFile::Tile;

    mTilesetToFirstGid.clear();
    mTilesetNameToFirstGid.clear();
    uint firstGid = 1;
    for (Tileset *tileset : tilesets) {
        if (!handleTileset(tileset, firstGid))
            return false;
    }

    for (WorldCell *cell : combinedMaps.mCells) {
        if (processObjectGroups(combinedMaps, cell, mapComposite) == false) {
            return false;
        }
    }

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    for (int level : mRoomRectByLevel.keys()) {
        QList<LotFile::RoomRect*> rrList = mRoomRectByLevel[level];
        // Use spatial partitioning to speed up the code below.
        mRoomRectLookup.clear(combinedMaps.mCellsWidth * CHUNKS_PER_CELL, combinedMaps.mCellsHeight * CHUNKS_PER_CELL);
        for (LotFile::RoomRect *rr : rrList) {
            mRoomRectLookup.add(rr, rr->bounds());
        }
        for (LotFile::RoomRect *rr : rrList) {
            if (rr->room == nullptr) {
                rr->room = new LotFile::Room(rr->nameWithoutSuffix(), rr->floor);
                rr->room->rects += rr;
                roomList += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#')))
                continue;
            QList<LotFile::RoomRect*> rrList2;
            mRoomRectLookup.overlapping(QRect(rr->bounds().adjusted(-1, -1, 1, 1)), rrList2);
            for (LotFile::RoomRect *comp : rrList2) {
                if (comp == rr)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != nullptr) {
                        LotFile::Room *room = comp->room;
                        for (LotFile::RoomRect *rr2 : room->rects) {
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

    mRoomLookup.clear(combinedMaps.mCellsWidth * CHUNKS_PER_CELL, combinedMaps.mCellsHeight * CHUNKS_PER_CELL);
    for (LotFile::Room *r : roomList) {
        r->mBounds = r->calculateBounds();
        mRoomLookup.add(r, r->bounds());
    }

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    for (LotFile::Room *r : roomList) {
        if (r->building == nullptr) {
            r->building = new LotFile::Building();
            buildingList += r->building;
            r->building->RoomList += r;
        }
        QList<LotFile::Room*> roomList2;
        mRoomLookup.overlapping(r->bounds().adjusted(-1, -1, 1, 1), roomList2);
        for (LotFile::Room *comp : roomList2) {
            if (comp == r)
                continue;
            if (r->building == comp->building)
                continue;

            if (r->inSameBuilding(comp)) {
                if (comp->building != nullptr) {
                    LotFile::Building *b = comp->building;
                    for (LotFile::Room *r2 : b->RoomList) {
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

    // Remove buildings with their north-west corner not in the cell.
    // Buildings may extend past the east and south edges of the 256x256 cell.
    QRect cellBounds256(0, 0, CELL_SIZE_256, CELL_SIZE_256);
    for (int i = buildingList.size() - 1; i >= 0; i--) {
        LotFile::Building* building = buildingList[i];
        QRect bounds = building->calculateBounds();
        if (bounds.isEmpty()) {
            continue;
        }
        if (cellBounds256.contains(bounds.topLeft())) {
            continue;
        }
        for (LotFile::Room *room : building->RoomList) {
            for (LotFile::RoomRect *roomRect : room->rects) {
                mRoomRects.removeOne(roomRect);
                mRoomRectByLevel[roomRect->floor].removeOne(roomRect);
                delete roomRect;
            }
            roomList.removeOne(room);
            delete room;
        }
        buildingList.removeAt(i);
        delete building;
    }

    for (int i = 0; i < roomList.size(); i++) {
        roomList[i]->ID = i;
    }
    mStats.numRoomRects += mRoomRects.size();
    mStats.numRooms += roomList.size();

    mStats.numBuildings += buildingList.size();

    return true;
}

bool LotFilesManager256::generateHeaderAux(int cell256X, int cell256Y)
{
    QString fileName = tr("%1_%2.lotheader")
            .arg(cell256X)
            .arg(cell256Y);

    QString lotsDirectory = mWorldDoc->world()->getGenerateLotsSettings().exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('L') << quint8('O') << quint8('T') << quint8('H');

    out << qint32(VERSION_LATEST);

    int tilecount = 0;
    for (LotFile::Tile *tile : TileMap) {
        if (tile->used) {
            tile->id = tilecount;
            tilecount++;
            if (tile->name.startsWith(QLatin1String("jumbo_tree_01"))) {
                int nnn = 0;
            }
        }
    }
    out << qint32(tilecount);

    for (LotFile::Tile *tile : TileMap) {
        if (tile->used) {
            SaveString(out, tile->name);
        }
    }

    out << qint32(CHUNK_SIZE_256);
    out << qint32(CHUNK_SIZE_256);
    out << qint32(mMinLevel);
    out << qint32(mMaxLevel);

    out << qint32(roomList.count());
    for (LotFile::Room *room : roomList) {
        SaveString(out, room->name);
        out << qint32(room->floor);

        out << qint32(room->rects.size());
        for (LotFile::RoomRect *rr : room->rects) {
            out << qint32(rr->x);
            out << qint32(rr->y);
            out << qint32(rr->w);
            out << qint32(rr->h);
        }

        out << qint32(room->objects.size());
        for (const LotFile::RoomObject &object : room->objects) {
            out << qint32(object.metaEnum);
            out << qint32(object.x);
            out << qint32(object.y);
        }
    }

    out << qint32(buildingList.count());
    for (LotFile::Building *building : buildingList) {
        out << qint32(building->RoomList.count());
        for (LotFile::Room *room : building->RoomList) {
            out << qint32(room->ID);
        }
    }

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    int minCell300X = lotSettings.worldOrigin.x();
    int minCell300Y = lotSettings.worldOrigin.y();
    int x1 = cell256X * CELL_SIZE_256 - minCell300X * CELL_WIDTH;
    int y1 = cell256Y * CELL_SIZE_256 - minCell300Y * CELL_WIDTH;
    int x2 = x1 + CHUNK_SIZE_256;
    int y2 = y1 + CHUNK_SIZE_256;

    for (int x = 0; x < CHUNKS_PER_CELL_256; x++) {
        for (int y = 0; y < CHUNKS_PER_CELL_256; y++) {
            qint8 density = calculateZombieDensity(x1 + x * CHUNK_SIZE_256, y1 + y * CHUNK_SIZE_256, x2, y2);
            out << density;
        }
    }

    file.close();

    return true;
}

bool LotFilesManager256::generateChunk(QDataStream &out, int chunkX, int chunkY)
{
    int notdonecount = 0;
    for (int z = mMinLevel; z <= mMaxLevel; z++)  {
        for (int x = 0; x < CHUNK_SIZE_256; x++) {
            for (int y = 0; y < CHUNK_SIZE_256; y++) {
                int gx = chunkX + x;
                int gy = chunkY + y;
                const QList<LotFile::Entry*> &entries = mGridData[gx][gy][z - MIN_WORLD_LEVEL].Entries;
                if (entries.count() == 0) {
                    notdonecount++;
                    continue;
                }
                if (notdonecount > 0) {
                    out << qint32(-1);
                    out << qint32(notdonecount);
                }
                notdonecount = 0;
                out << qint32(entries.count() + 1);
                out << qint32(getRoomID(gx, gy, z));
                for (const LotFile::Entry *entry : entries) {
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

void LotFilesManager256::generateBuildingObjects(int mapWidth, int mapHeight)
{
    for (LotFile::Room *room : roomList) {
        for (LotFile::RoomRect *rr : room->rects) {
            generateBuildingObjects(mapWidth, mapHeight, room, rr);
        }
    }
}

void LotFilesManager256::generateBuildingObjects(int mapWidth, int mapHeight, LotFile::Room *room, LotFile::RoomRect *rr)
{
    for (int x = rr->x; x < rr->x + rr->w; x++) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];

            // Remember the room at each position in the map.
            // TODO: Remove this, it isn't used by Java code now.
            square.roomID = room->ID;

            /* Examine every tile inside the room.  If the tile's metaEnum >= 0
               then create a new RoomObject for it. */
            for (LotFile::Entry *entry : square.Entries) {
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
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];
            for (LotFile::Entry *entry : square.Entries) {
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
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];
            for (LotFile::Entry *entry : square.Entries) {
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

void LotFilesManager256::generateJumboTrees(CombinedCellMaps& combinedMaps)
{
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    const quint8 JUMBO_ZONE = 1;
    const quint8 PREVENT_JUMBO = 2;
    const quint8 REMOVE_TREE = 3;
    const quint8 JUMBO_TREE = 4;

    QSet<QString> treeTiles;
    QSet<QString> floorVegTiles;
    for (TileDefFile *tdf : Navigate::IsoGridSquare256::mTileDefFiles) {
        for (TileDefTileset *tdts : tdf->tilesets()) {
            for (TileDefTile *tdt : tdts->mTiles) {
                // Get the set of all tree tiles.
                if (tdt->mProperties.contains(QLatin1String("tree")) || (tdts->mName.startsWith(QLatin1String("vegetation_trees")))) {
                    treeTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
                // Get the set of all floor + vegetation tiles.
                if (tdt->mProperties.contains(QLatin1String("solidfloor")) ||
                        tdt->mProperties.contains(QLatin1String("FloorOverlay")) ||
                        tdt->mProperties.contains(QLatin1String("vegitation"))) {
                    floorVegTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
            }
        }
    }

    quint8 grid[CELL_SIZE_256][CELL_SIZE_256];
    quint8 densityGrid[CELL_SIZE_256][CELL_SIZE_256];
    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            grid[x][y] = PREVENT_JUMBO;
            densityGrid[x][y] = 0;
        }
    }

    QHash<ObjectType*,const JumboZone*> objectTypeMap;
    for (const JumboZone* jumboZone : qAsConst(mJumboZoneList)) {
        if (ObjectType *objectType = mWorldDoc->world()->objectType(jumboZone->zoneName)) {
            objectTypeMap[objectType] = jumboZone;
        }
    }

    PropertyDef *JumboDensity = mWorldDoc->world()->propertyDefinition(QStringLiteral("JumboDensity"));

    QRect cellBounds256(combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH,
                        combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT,
                        CELL_SIZE_256, CELL_SIZE_256);

    ClipperLib::Path zonePath;
    for (WorldCell* cell : combinedMaps.mCells) {
        QPoint cellPos300((cell->x() + lotSettings.worldOrigin.x() - combinedMaps.mMinCell300X) * CELL_WIDTH,
                          (cell->y() + lotSettings.worldOrigin.y() - combinedMaps.mMinCell300Y) * CELL_HEIGHT);
        for (WorldCellObject *obj : cell->objects()) {
            if ((obj->level() != 0) || !objectTypeMap.contains(obj->type())) {
                continue;
            }
            if (obj->isPoint() || obj->isPolyline()) {
                continue;
            }
            zonePath.clear();
            if (obj->isPolygon()) {
                for (const auto &pt : obj->points()) {
                    zonePath << ClipperLib::IntPoint(cellPos300.x() + pt.x * 100, cellPos300.y() + pt.y * 100);
                }
            }
            quint8 density = objectTypeMap[obj->type()]->density;
            Property* property = JumboDensity ? obj->properties().find(JumboDensity) : nullptr;
            if (property != nullptr) {
                bool ok = false;
                int value = property->mValue.toInt(&ok);
                if (ok && (value >= 0) && (value <= 100)) {
                    density = value;
                }
            }
            int ox = cellPos300.x() + obj->x();
            int oy = cellPos300.y() + obj->y();
            int ow = obj->width();
            int oh = obj->height();
            for (int y = oy; y < oy + oh; y++) {
                for (int x = ox; x < ox + ow; x++) {
                    if ((zonePath.empty() == false)) {
                        ClipperLib::IntPoint pt(x * 100 + 50, y * 100 + 50); // center of the square
                        if (ClipperLib::PointInPolygon(pt, zonePath) == 0) {
                            continue;
                        }
                    }
                    int gx = x - cellBounds256.x();
                    int gy = y - cellBounds256.y();
                    if ((gx >= 0) && (gx < CELL_SIZE_256) && (gy >= 0) && (gy < CELL_SIZE_256)) {
                        grid[gx][gy] = JUMBO_ZONE;
                        densityGrid[gx][gy] = std::max(densityGrid[gx][gy], density);
                    }
                }
            }
        }
    }

    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            // Prevent jumbo trees near any second-story tiles
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            if (!mGridData[wx][wy][1 - MIN_WORLD_LEVEL].Entries.isEmpty()) {
                for (int yy = y; yy <= y + 4; yy++) {
                    for (int xx = x; xx <= x + 4; xx++) {
                        if (xx >= 0 && xx < CELL_SIZE_256 && yy >= 0 && yy < CELL_SIZE_256)
                            grid[xx][yy] = PREVENT_JUMBO;
                    }
                }
            }

            // Prevent jumbo trees near non-floor, non-vegetation (fences, etc)
            const auto& entries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
            for (LotFile::Entry *e : entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (!floorVegTiles.contains(tile->name)) {
                    for (int yy = y - 1; yy <= y + 1; yy++) {
                        for (int xx = x - 1; xx <= x + 1; xx++) {
                            if (xx >= 0 && xx < CELL_SIZE_256 && yy >= 0 && yy < CELL_SIZE_256)
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
        for (int x = 0; x < CELL_SIZE_256; x++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < CELL_SIZE_256; y++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }

    // Get a list of all tree positions in the cell.
    QList<QPoint> allTreePos;
    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            const auto& entries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
            for (LotFile::Entry *e : entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (treeTiles.contains(tile->name) == false) {
                    continue;
                }
                allTreePos += QPoint(x, y);
                break;
            }
        }
    }

    QRandomGenerator qrand;

    while (!allTreePos.isEmpty()) {
        int r = qrand() % allTreePos.size();
        QPoint treePos = allTreePos.takeAt(r);
        quint8 g = grid[treePos.x()][treePos.y()];
        quint8 density = densityGrid[treePos.x()][treePos.y()];
        if ((g == JUMBO_ZONE) && (qrand() % 100 < density)) {
            grid[treePos.x()][treePos.y()] = JUMBO_TREE;
            // Remove all trees surrounding a jumbo tree.
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0)
                        continue;
                    int x = treePos.x() + dx;
                    int y = treePos.y() + dy;
                    if (x >= 0 && x < CELL_SIZE_256 && y >= 0 && y < CELL_SIZE_256)
                        grid[x][y] = REMOVE_TREE;
                }
            }
        }
    }

    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            if (grid[x][y] == JUMBO_TREE) {
                QList<LotFile::Entry*>& squareEntries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
                for (LotFile::Entry *e : squareEntries) {
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        e->gid = mTilesetToFirstGid[mJumboTreeTileset];
                        TileMap[e->gid]->used = true;
                        break;
                    }
                }
            }
            if (grid[x][y] == REMOVE_TREE) {
                QList<LotFile::Entry*>& squareEntries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
                for (int i = 0; i < squareEntries.size(); i++) {
                    LotFile::Entry *e = squareEntries[i];
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        squareEntries.removeAt(i);
                        break;
                    }
                }
            }
        }
    }
}

bool LotFilesManager256::handleTileset(const Tiled::Tileset *tileset, uint &firstGid)
{
    if (!tileset->fileName().isEmpty()) {
        mError = tr("Only tileset image files supported, not external tilesets");
        return false;
    }

    QString name = nameOfTileset(tileset);

    // TODO: Verify that two tilesets sharing the same name are identical
    // between maps.
#if 1
    auto it = mTilesetNameToFirstGid.find(name);
    if (it != mTilesetNameToFirstGid.end()) {
        mTilesetToFirstGid.insert(tileset, it.value());
        return true;
    }
#else
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
#endif

    for (int i = 0; i < tileset->tileCount(); ++i) {
        int localID = i;
        int ID = firstGid + localID;
        LotFile::Tile *tile = new LotFile::Tile(QStringLiteral("%1_%2").arg(name).arg(localID));
        tile->metaEnum = TileMetaInfoMgr::instance()->tileEnumValue(tileset->tileAt(i));
        TileMap[ID] = tile;
    }

    mTilesetToFirstGid.insert(tileset, firstGid);
    mTilesetNameToFirstGid.insert(name, firstGid);
    firstGid += tileset->tileCount();

    return true;
}

int LotFilesManager256::getRoomID(int x, int y, int z)
{
    return mGridData[x][y][z - MIN_WORLD_LEVEL].roomID;
}

uint LotFilesManager256::cellToGid(const Cell *cell)
{
    Tileset *tileset = cell->tile->tileset();

#if 1
    auto v = mTilesetToFirstGid.find(tileset);
    if (v == mTilesetToFirstGid.end()) {
        // tileset not found
        return 0;
    }
    return v.value() + cell->tile->id();
#else
    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end && i.key() != tileset)
        ++i;
    if (i == i_end) // tileset not found
        return 0;

    return i.value() + cell->tile->id();
#endif
}

bool LotFilesManager256::processObjectGroups(CombinedCellMaps &combinedMaps, WorldCell *cell, MapComposite *mapComposite)
{
    for (Layer *layer : mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (!processObjectGroup(combinedMaps, cell, og, mapComposite->levelRecursive(), mapComposite->originRecursive()))
                return false;
        }
    }

    for (MapComposite *subMap : mapComposite->subMaps())
        if (!processObjectGroups(combinedMaps, cell, subMap))
            return false;

    return true;
}

bool LotFilesManager256::processObjectGroup(CombinedCellMaps &combinedMaps, WorldCell *cell, ObjectGroup *objectGroup, int levelOffset, const QPoint &offset)
{
    int level = objectGroup->level();
    level += levelOffset;

    // Align with the 256x256 cell.
    QPoint offset1 = offset;
    offset1.rx() -= combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH;
    offset1.ry() -= combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT;

    for (const MapObject *mapObject : objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (!mapObject->width() || !mapObject->height())
            continue;

        int x = std::floor(mapObject->x());
        int y = std::floor(mapObject->y());
        int w = std::ceil(mapObject->x() + mapObject->width()) - x;
        int h = std::ceil(mapObject->y() + mapObject->height()) - y;

        QString name = mapObject->name();
        if (name.isEmpty())
            name = QLatin1String("unnamed");

        if (objectGroup->map()->orientation() == Map::Isometric) {
            x += 3 * level;
            y += 3 * level;
        }

        if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
            if (x < 0 || y < 0 || x + w > CELL_WIDTH || y + h > CELL_HEIGHT) {
                x = qBound(0, x, CELL_WIDTH);
                y = qBound(0, y, CELL_HEIGHT);
                mError = tr("A RoomDef in cell %1,%2 overlaps cell boundaries.\nNear x,y=%3,%4")
                        .arg(cell->x()).arg(cell->y()).arg(x).arg(y);
                return false;
            }
            // Apply the MapComposite offset in the top-level map.
            x += offset1.x();
            y += offset1.y();
            LotFile::RoomRect *rr = new LotFile::RoomRect(name, x, y, level, w, h);
            mRoomRects += rr;
            mRoomRectByLevel[level] += rr;
        }
    }
    return true;
}

void LotFilesManager256::resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    for (PropertyTemplate *pt : ph->templates())
        resolveProperties(pt, result);
    for (Property *p : ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

qint8 LotFilesManager256::calculateZombieDensity(int x1, int y1, int x2, int y2)
{
    // TODO: Get the total depth of 8x8 squares, then divide by 64.
    Q_UNUSED(x2)
    Q_UNUSED(y2)
    if (x1 < 0 || y1 < 0 || x1 >= ZombieSpawnMap.size().width() || y1 >= ZombieSpawnMap.size().height()) {
        return 0;
    }
    int chunk300X = std::floor(x1 / float(CHUNK_WIDTH));
    int chunk300Y = std::floor(y1 / float(CHUNK_HEIGHT));
    QRgb pixel = ZombieSpawnMap.pixel(chunk300X, chunk300Y);
    return quint8(qRed(pixel));
}

/////

CombinedCellMaps::CombinedCellMaps()
{

}

CombinedCellMaps::~CombinedCellMaps()
{
    MapInfo* mapInfo = mMapComposite->mapInfo(); // 256x256
    delete mMapComposite;
    delete mapInfo->map();
    delete mapInfo;
}

bool CombinedCellMaps::load(WorldDocument *worldDoc, int cell256X, int cell256Y)
{
    const GenerateLotsSettings &lotSettings = worldDoc->world()->getGenerateLotsSettings();
    mCell256X = cell256X;
    mCell256Y = cell256Y;
    int minCell300X = std::floor(cell256X * CELL_SIZE_256 / float(CELL_WIDTH));
    int minCell300Y = std::floor(cell256Y * CELL_SIZE_256 / float(CELL_HEIGHT));
    int maxCell300X = std::ceil(((cell256X + 1) * CELL_SIZE_256 - 1) / float(CELL_WIDTH));
    int maxCell300Y = std::ceil(((cell256Y + 1) * CELL_SIZE_256 - 1) / float(CELL_HEIGHT));
    mMinCell300X = minCell300X;
    mMinCell300Y = minCell300Y;
    mCellsWidth = maxCell300X - minCell300X;
    mCellsHeight = maxCell300Y - minCell300Y;
    mCells.clear();
    for (int cell300Y = minCell300Y; cell300Y < maxCell300Y; cell300Y++) {
        for (int cell300X = minCell300X; cell300X < maxCell300X; cell300X++) {
            WorldCell* cell = worldDoc->world()->cellAt(cell300X - lotSettings.worldOrigin.x(), cell300Y - lotSettings.worldOrigin.y());
            if (cell == nullptr) {
                continue;
            }
            if (cell->mapFilePath().isEmpty()) {
                continue;
            }
            MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath(), QString(), true);
            if (mapInfo == nullptr) {
                mError = MapManager::instance()->errorString();
                return false;
            }
            mLoader.addMap(mapInfo);
            for (WorldCellLot *lot : cell->lots()) {
                if (MapInfo *info = MapManager::instance()->loadMap(lot->mapName(), QString(), true, MapManager::PriorityMedium)) {
                    mLoader.addMap(info);
                } else {
                    mError = MapManager::instance()->errorString();
                    return false;
                }
            }
            mCells += cell;
        }
    }
    while (mLoader.isLoading()) {
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    if (mLoader.errorString().isEmpty() == false) {
        mError = mLoader.errorString();
        return false;
    }

    MapInfo* mapInfo = getCombinedMap();
    mMapComposite = new MapComposite(mapInfo);
    for (WorldCell* cell : mCells) {
        MapInfo *info = MapManager::instance()->mapInfo(cell->mapFilePath());
        QPoint cellPos((cell->x() + lotSettings.worldOrigin.x() - mMinCell300X) * CELL_WIDTH, (cell->y() + lotSettings.worldOrigin.y() - mMinCell300Y) * CELL_HEIGHT);
        MapComposite* subMap = mMapComposite->addMap(info, cellPos, 0);
        subMap->setLotFilesManagerMap(true);
        for (WorldCellLot *lot : cell->lots()) {
            MapInfo *info = MapManager::instance()->mapInfo(lot->mapName());
            subMap->addMap(info, lot->pos(), lot->level());
        }
    }
    mMapComposite->synch(); //
    return true;
}

MapInfo *CombinedCellMaps::getCombinedMap()
{
    QString mapFilePath(QLatin1String("<LotFilesManagerMap>"));
    Map *map = new Map(Map::LevelIsometric, mCellsWidth * CELL_WIDTH, mCellsHeight * CELL_HEIGHT, 64, 32);
    MapInfo *mapInfo = new MapInfo(map);
    mapInfo->setFilePath(mapFilePath);
    return mapInfo;
}