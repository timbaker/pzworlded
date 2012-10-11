#include "lotfilesmanager.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "preferences.h"
#include "progress.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "tile.h"
#include "tileset.h"

#include <qmath.h>
#include <QDateTime>
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
    PROGRESS progress(QLatin1String("Generating .lot files"));

    // Choose an existing directory to save the .lot files if it hasn't
    // been specified in the preferences or doesn't exist.
    QString lotsDirectory = Preferences::instance()->lotsDirectory();
    QDir dir(lotsDirectory);
    if (!dir.exists()) {
        QString f = QFileDialog::getExistingDirectory(0,
            tr("Choose the Lots Folder"));
        if (f.isEmpty())
            return true;
        Preferences::instance()->setLotsDirectory(f);
    }

    QString projectDir = QFileInfo(worldDoc->fileName()).absolutePath();
    QString spawnMap = projectDir + QLatin1Char('/') + QLatin1String("ZombieSpawnMap.bmp");
    if (!QFileInfo(spawnMap).exists()) {
        mError = tr("Couldn't find ZombieSpawnMap.bmp in the project directory.");
        return false;
    }
    ZombieSpawnMap = QImage(spawnMap);

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

    QMessageBox::information(0, tr("Generate Lot Files"), tr("Finished!"));

    return true;
}

bool LotFilesManager::generateCell(WorldCell *cell)
{
//    if (cell->x() != 5 || cell->y() != 3) return true;
    if (cell->mapFilePath().isEmpty())
        return true;

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
    if (!mapInfo)
        return false;

    MapComposite *mapComposite = new MapComposite(mapInfo);

    generateHeader(cell, mapComposite);

    MaxLevel = 15;

    int mapWidth = mapInfo->width();
    int mapHeight = mapInfo->height();

    // Cleanup last call
    for (int x = 0; x < mGridData.size(); x++) {
        for (int y = 0; y < mGridData[x].size(); y++) {
            for (int z = 0; z < mGridData[x][y].size(); z++)  {
                const QList<LotFile::Entry*> &entries = mGridData[x][y][z].Entries;
                foreach (LotFile::Entry *entry, entries)
                    delete entry;
            }
        }
    }
    mGridData.clear();

    mGridData.resize(mapWidth);
    for (int x = 0; x < mapWidth; x++) {
        mGridData[x].resize(mapHeight);
        for (int y = 0; y < mapHeight; y++)
            mGridData[x][y].resize(MaxLevel);
    }

    foreach (CompositeLayerGroup *lg, mapComposite->layerGroups()) {
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                QVector<const Tiled::Cell *> cells;
                lg->orderedCellsAt2(QPoint(x, y), cells);
                foreach (const Tiled::Cell *cell, cells) {
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    int lx = x, ly = y;
                    if (true/*mapInfo->orientation() == Map::Isometric*/) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    mGridData[lx][ly][lg->level()].Entries.append(e);
                    TileMap[e->gid]->used = true;
                }
            }
        }
    }

    generateHeaderAux(cell, mapComposite);

    /////

    QString fileName = tr("world_%1_%2.lotpack")
            .arg(cell->x())
            .arg(cell->y());

    QString lotsDirectory = Preferences::instance()->lotsDirectory();
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

    Map *map = mapComposite->map();

    qDeleteAll(roomList);
    qDeleteAll(buildingList);
    qDeleteAll(ZoneList);

    roomList.clear();
    buildingList.clear();
    ZoneList.clear();

    // Create the set of all tilesets used by the map and its sub-maps.
    // FIXME: should be recursive.
    QList<Tileset*> tilesets;
    tilesets += map->tilesets();
    foreach (MapComposite *subMap, mapComposite->subMaps())
        tilesets += subMap->map()->tilesets();

    TileMap.clear();
    mTilesetToFirstGid.clear();
    uint firstGid = 1;
    foreach (Tileset *tileset, tilesets) {
        if (!handleTileset(tileset, firstGid))
            return false;
    }

    foreach (Layer *layer, map->layers()) {
        int level;
        if (!MapComposite::levelForLayer(layer, &level))
            continue;
        if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
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

                if (map->orientation() == Map::Isometric) {
                    x += 3 * level;
                    y += 3 * level;
                }

                if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
                    LotFile::Room *room = new LotFile::Room(name, x, y, level, w, h);
                    room->ID = roomList.count();
                    roomList += room;
                } else {
                    LotFile::Zone *z = new LotFile::Zone(name,
                                                         mapObject->type(),
                                                         x, y, level, w, h);
                    ZoneList.append(z);
                }
            }
        }
    }

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

            if (r->IsSameBuilding(comp)) {
                if (comp->building != 0) {
                    r->building->RoomList += comp->building->RoomList;
                    buildingList.removeOne(comp->building);
                    delete comp->building;
                }
                comp->building = r->building;
                if (!r->building->RoomList.contains(comp))
                    r->building->RoomList += comp;
            }
        }
    }

    return true;
}

bool LotFilesManager::generateHeaderAux(WorldCell *cell, MapComposite *mapComposite)
{
    Q_UNUSED(mapComposite)

    QString fileName = tr("%1_%2.lotheader")
            .arg(cell->x())
            .arg(cell->y());

    QString lotsDirectory = Preferences::instance()->lotsDirectory();
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
        out << qint32(room->x);
        out << qint32(room->y);
        out << qint32(room->w);
        out << qint32(room->h);
        out << qint32(room->floor);
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
            QRgb pixel = ZombieSpawnMap.pixel(cell->x() * 30 + x, cell->y() * 30 + y);
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
        TileMap[ID] = new LotFile::Tile(name + QLatin1String("_") + QString::number(localID));
    }

    mTilesetToFirstGid.insert(tileset, firstGid);
    firstGid += tileset->tileCount();

    return true;
}

int LotFilesManager::getRoomID(int x, int y, int z)
{
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
