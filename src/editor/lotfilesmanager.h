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

#include <cmath>

class BMPToTMXImages;
class MapComposite;
class MapInfo;
class PropertyHolder;
class PropertyList;
class Road;
class World;
class WorldDocument;
class WorldCell;

namespace Tiled {
class ObjectGroup;
}

#define CELL_WIDTH 300
#define CELL_HEIGHT 300

#define CHUNKS_PER_CELL 30

#define CHUNK_WIDTH 10
#define CHUNK_HEIGHT 10

namespace LotFile
{

template<typename T>
T clamp(T v, T min, T max)
{
    return qMin(qMax(v, min), max);
};

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

    const QRect& bounds() const
    {
        return mBounds;
    }

    QRect calculateBounds() const
    {
        QRect bounds;
        if (rects.isEmpty()) {
            return bounds;
        }
        bounds = rects[0]->bounds();
        for (int i = 1; i < rects.size(); i++) {
            bounds = bounds.united(rects[i]->bounds());
        }
        return bounds;
    }

    int ID;
    int floor;
    QString name;
    Building *building;
    QList<RoomRect*> rects;
    QList<RoomObject> objects;
    QRect mBounds;
};

class Building
{
public:
    QRect calculateBounds() const
    {
        QRect bounds;
        if (RoomList.isEmpty()) {
            return bounds;
        }
        bounds = RoomList[0]->bounds();
        for (int i = 1; i < RoomList.size(); i++) {
            bounds = bounds.united(RoomList[i]->bounds());
        }
        return bounds;
    }
    QList<Room*> RoomList;
};

template <typename T>
class RectLookup
{
public:
    RectLookup(int squaresPerChunk) :
        mMinSquareX(-1),
        mMinSquareY(-1),
        mSquaresPerChunk(squaresPerChunk)
    {

    }

    void clear(int minSquareX, int minSquareY, int widthInChunks, int heightInChunks)
    {
        for (int i = 0; i < mGrid.size(); i++) {
            mGrid[i].clear();
        }
        mMinSquareX = minSquareX;
        mMinSquareY = minSquareY;
        mWidthInChunks = widthInChunks;
        mHeightInChunks = heightInChunks;
        mGrid.resize(mWidthInChunks * mHeightInChunks);
        mBoundsInSquares = QRect(mMinSquareX, mMinSquareY, mWidthInChunks * mSquaresPerChunk, mHeightInChunks * mSquaresPerChunk);
    }

    void add(T* element, const QRect& bounds)
    {
        QRect bounds1(bounds & mBoundsInSquares);
        if (bounds1.isEmpty()) {
            return;
        }
        int xMin = (bounds1.x() - mMinSquareX) / mSquaresPerChunk;
        int yMin = (bounds1.y() - mMinSquareY) / mSquaresPerChunk;
        int xMax = std::ceil((bounds1.x() + bounds1.width() - mMinSquareX) / float(mSquaresPerChunk));
        int yMax = std::ceil((bounds1.y() + bounds1.height() - mMinSquareY) / float(mSquaresPerChunk));
//        xMin = clamp(xMin, 0, mWidthInChunks-1);
//        yMin = clamp(yMin, 0, mHeightInChunks-1);
        xMax = clamp(xMax, 0, mWidthInChunks-1);
        yMax = clamp(yMax, 0, mHeightInChunks-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                mGrid[x + y * mWidthInChunks] += element;
            }
        }
    }

    void overlapping(const QRect& rect, QList<T*>& elements) const
    {
        QRect rect1(rect & mBoundsInSquares);
        if (rect1.isEmpty()) {
            return;
        }
        int xMin = (rect1.x() - mMinSquareX) / mSquaresPerChunk;
        int yMin = (rect1.y() - mMinSquareY) / mSquaresPerChunk;
        int xMax = std::ceil((rect1.x() + rect1.width() - mMinSquareX) / float(mSquaresPerChunk));
        int yMax = std::ceil((rect1.y() + rect1.height() - mMinSquareY) / float(mSquaresPerChunk));
//        xMin = clamp(xMin, 0, mWidthInChunks-1);
//        yMin = clamp(yMin, 0, mHeightInChunks-1);
        xMax = clamp(xMax, 0, mWidthInChunks-1);
        yMax = clamp(yMax, 0, mHeightInChunks-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                for (T* e : mGrid[x + y * mWidthInChunks]) {
                    if (elements.contains(e) == false) {
                        elements += e;
                    }
                }
            }
        }
    }

    int mSquaresPerChunk;
    int mMinSquareX;
    int mMinSquareY;
    int mWidthInChunks;
    int mHeightInChunks;
    QRect mBoundsInSquares;
    QVector<QVector<T*>> mGrid;
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

    void reset()
    {
        numBuildings = 0;
        numRooms = 0;
        numRoomRects = 0;
        numRoomObjects = 0;
    }

    void combine(const Stats& rhs)
    {
        numBuildings += rhs.numBuildings;
        numRooms += rhs.numRooms;
        numRoomRects += rhs.numRoomRects;
        numRoomObjects += rhs.numRoomObjects;
    }

    int numBuildings;
    int numRooms;
    int numRoomRects;
    int numRoomObjects;
};

} // namespace LotFile

class DelayedMapLoader : public QObject
{
    Q_OBJECT
public:
    DelayedMapLoader();
    ~DelayedMapLoader()
    {
        qDeleteAll(mLoading);
        qDeleteAll(mLoaded);
    }

    void addMap(MapInfo *info);
    bool isLoading();
    const QString &errorString() const
    { return mError; }

private slots:
    void mapLoaded(MapInfo *info);
    void mapFailedToLoad(MapInfo *info);

private:
    class SubMapLoading
    {
    public:
        SubMapLoading(MapInfo *info);
        ~SubMapLoading();
        MapInfo *mapInfo;
        bool holdsReference;
    private:
        Q_DISABLE_COPY(SubMapLoading)
    };

    QList<SubMapLoading*> mLoading;
    QList<SubMapLoading*> mLoaded;
    QString mError;
};

struct JumboZone
{
    QString zoneName;
    quint8 density;

    JumboZone(const QString& zoneName, quint8 density)
        : zoneName(zoneName)
        , density(density)
    {

    }
};

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

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);
    bool generateCell(WorldCell *cell);
    bool generateCell256(WorldDocument *worldDoc, int cell256X, int cell256Y);
    bool generateHeader(WorldCell *cell, MapComposite *mapComposite);
    bool generateHeaderAux(WorldCell *cell, MapComposite *mapComposite);
    bool generateChunk(QDataStream &out, WorldCell *cell, MapComposite *mapComposite, int cx, int cy);
    void generateBuildingObjects(int mapWidth, int mapHeight);
    void generateBuildingObjects(int mapWidth, int mapHeight,
                                 LotFile::Room *room, LotFile::RoomRect *rr);
    void generateJumboTrees(WorldCell *cell, MapComposite *mapComposite);

    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);

    int getRoomID(int x, int y, int z);

    QString errorString() const { return mError; }

signals:
        
private:
    uint cellToGid(const Tiled::Cell *cell);
    bool processObjectGroups(WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(WorldCell *cell, Tiled::ObjectGroup *objectGroup,
                            int levelOffset, const QPoint &offset);
    void resolveProperties(PropertyHolder *ph, PropertyList &result);

private:
    Q_DISABLE_COPY(LotFilesManager)

    explicit LotFilesManager(QObject *parent = 0);
    ~LotFilesManager();

    static LotFilesManager *mInstance;

    WorldDocument *mWorldDoc;
    QList<LotFile::Zone*> ZoneList;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<QString, uint> mTilesetNameToFirstGid;
    Tiled::Tileset *mJumboTreeTileset;
    QMap<int,LotFile::Tile*> TileMap;
    QVector<QVector<QVector<LotFile::Square> > > mGridData;
    int mMinLevel;
    int mMaxLevel;
    QList<LotFile::RoomRect*> mRoomRects;
    QMap<int,QList<LotFile::RoomRect*> > mRoomRectByLevel;
    LotFile::RectLookup<LotFile::RoomRect> mRoomRectLookup;
    LotFile::RectLookup<LotFile::Room> mRoomLookup;
    QList<LotFile::Room*> roomList;
    QList<LotFile::Building*> buildingList;
    QImage ZombieSpawnMap;
    QList<const JumboZone*> mJumboZoneList;
    LotFile::Stats mStats;
    QList<GenerateCellFailure> mFailures;
    QString mError;
};

#endif // LOTFILESMANAGER_H
