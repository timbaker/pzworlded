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

#ifndef LOTFILESMANAGER_H
#define LOTFILESMANAGER_H

#include "gidmapper.h"

#include <QObject>

class BMPToTMXImages;
class MapComposite;
class Road;
class World;
class WorldDocument;
class WorldCell;

namespace Tiled {
class ObjectGroup;
}

#define CELL_WIDTH 300
#define CELL_HEIGHT 300

#define CHUNK_WIDTH 10
#define CHUNK_HEIGHT 10

namespace LotFile
{
class Tile
{
public:
    Tile(const QString &name = QString())
    {
        this->name = name;
        this->used = false;
        this->id = -1;
        this->metaEnum = -1;
    }
    QString name;
    bool used;
    int id; // index into .lotheader's list of used tiles
    int metaEnum; // Tilesets.txt enumeration
};

class Lot
{
public:
    Lot(QString name, int x, int y, int w, int h)
    {
        this->name = name;
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
    }
    QString name;
    int x;
    int y;
    int w;
    int h;
};

class Entry
{
public:
    Entry(int gid) :
        gid(gid)
    {
    }

    int gid;
};

class Square
{
public:
    Square() :
        roomID(-1)
    {

    }
    ~Square()
    {
        qDeleteAll(Entries);
    }
    Square &operator=(const Square &other)
    {
        qDeleteAll(Entries);
        Entries = other.Entries;
        roomID = other.roomID;
        return *this;
    }

    QList<Entry*> Entries;
    int roomID;
};

class Zone
{
public:
    Zone(const QString& name, const QString& val, int x, int y, int z, int width, int height)
    {
        this->name = name;
        this->val = val;
        this->x = x;
        this->y = y;
        this->z = z;
        this->width = width;
        this->height = height;
    }

    QString name;
    QString val;
    int x;
    int y;
    int z;
    int width;
    int height;
};

class Building;
class Room;

class RoomObject
{
public:
    int metaEnum; // Tilesets.txt enumeration
    int x; // Cell coord
    int y; // Cell coord
};

class RoomRect
{
public:
    RoomRect(const QString &name, int x, int y, int level, int w, int h)
        : x(x)
        , y(y)
        , w(w)
        , h(h)
        , floor(level)
        , name(name)
        , room(0)
    {

    }

    QRect bounds() const
    {
        return QRect(x, y, w, h);
    }

    QPoint topLeft() const { return QPoint(x, y); }
    QPoint topRight() const { return QPoint(x + w, y); }
    QPoint bottomLeft() const { return QPoint(x, y + h); }
    QPoint bottomRight() const { return QPoint(x + w, y + h); }

    bool isAdjacent(RoomRect *comp) const
    {
        QRect a(x - 1, y - 1, w + 2, h + 2);
        QRect b(comp->x, comp->y, comp->w, comp->h);
        return a.intersects(b);
    }

    bool isTouchingCorners(RoomRect *comp) const
    {
        return topLeft() == comp->bottomRight() ||
                topRight() == comp->bottomLeft() ||
                bottomLeft() == comp->topRight() ||
                bottomRight() == comp->topLeft();
    }

    bool inSameRoom(RoomRect *comp) const
    {
        if (floor != comp->floor) return false;
        if (name != comp->name) return false;
        if (!name.contains(QLatin1Char('#'))) return false;
        return isAdjacent(comp) && !isTouchingCorners(comp);
    }

    QString nameWithoutSuffix() const
    {
        int pos = name.indexOf(QLatin1Char('#'));
        if (pos == -1) return name;
        return name.left(pos);
    }

    int x;
    int y;
    int w;
    int h;
    int floor;
    QString name;
    Room *room;
};

class Room
{
public:
    Room(const QString &name, int level)
        : ID(-1)
        , floor(level)
        , name(name)
        , building(0)
    {

    }

    bool inSameBuilding(Room *comp)
    {
        foreach (RoomRect *rr, rects) {
            foreach (RoomRect *rr2, comp->rects) {
                if (rr->isAdjacent(rr2))
                    return true;
            }
        }
        return false;
    }

    int ID;
    int floor;
    QString name;
    Building *building;
    QList<RoomRect*> rects;
    QList<RoomObject> objects;
};

class Building
{
public:
    QList<Room*> RoomList;
};

class Stats
{
public:
    Stats() :
        numBuildings(0),
        numRooms(0),
        numRoomRects(0),
        numRoomObjects(0)
    {
    }

    int numBuildings;
    int numRooms;
    int numRoomRects;
    int numRoomObjects;
};

} // namespace LotFile

class LotFilesManager : public QObject
{
    Q_OBJECT
public:
    
    static LotFilesManager *instance();
    static void deleteInstance();

    enum GenerateMode {
        GenerateAll,
        GenerateSelected
    };

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);
    bool generateCell(WorldCell *cell);
    bool generateHeader(WorldCell *cell, MapComposite *mapComposite);
    bool generateHeaderAux(WorldCell *cell, MapComposite *mapComposite);
    bool generateChunk(QDataStream &out, WorldCell *cell, MapComposite *mapComposite, int cx, int cy);
    void generateBuildingObjects(int mapWidth, int mapHeight);
    void generateBuildingObjects(int mapWidth, int mapHeight,
                                 LotFile::Room *room, LotFile::RoomRect *rr);

    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);

    int getRoomID(int x, int y, int z);

    QString errorString() const { return mError; }

signals:
    
public slots:
    
private:
    uint cellToGid(const Tiled::Cell *cell);
    void processObjectGroups(MapComposite *mapComposite);
    void processObjectGroup(Tiled::ObjectGroup *objectGroup, int levelOffset,
                            const QPoint &offset);

private:
    Q_DISABLE_COPY(LotFilesManager)

    explicit LotFilesManager(QObject *parent = 0);
    ~LotFilesManager();

    static LotFilesManager *mInstance;

    WorldDocument *mWorldDoc;
    QList<LotFile::Zone*> ZoneList;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<int,LotFile::Tile*> TileMap;
    QVector<QVector<QVector<LotFile::Square> > > mGridData;
    int MaxLevel;
    int Version;
    QList<LotFile::RoomRect*> mRoomRects;
    QMap<int,QList<LotFile::RoomRect*> > mRoomRectByLevel;
    QList<LotFile::Room*> roomList;
    QList<LotFile::Building*> buildingList;
    QImage ZombieSpawnMap;
    LotFile::Stats mStats;

    QString mError;
};

#endif // LOTFILESMANAGER_H
