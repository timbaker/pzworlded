#ifndef LOTFILESMANAGER_H
#define LOTFILESMANAGER_H

#include "gidmapper.h"

#include <QObject>

class MapComposite;
class World;
class WorldDocument;
class WorldCell;

#define CELL_WIDTH 300
#define CELL_HEIGHT 300

#define CHUNK_WIDTH 10
#define CHUNK_HEIGHT 10

namespace LotFile
{
class Tile
{
public:
    Tile(const QString& name)
    {
        this->name = name;
        this->used = false;
        this->id = -1;
    }
    QString name;
    bool used;
    int id;
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
    Entry(int gid)
    {
        this->gid = gid;
    }
    int gid;
};

class Square
{
public:
    QList<Entry*> Entries;
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

class Room
{
public:
    Room(const QString &name, int x, int y, int level, int w, int h)
        : ID(-1)
        , x(x)
        , y(y)
        , w(w)
        , h(h)
        , floor(level)
        , name(name)
        , building(0)
    {

    }

    bool IsSameBuilding(Room *comp)
    {
        QRect a(x - 1, y - 1, w + 2, h + 2);
        QRect b(comp->x - 1, comp->y - 1, comp->w + 2, comp->h + 2);
        return a.intersects(b);
    }

    int ID;
    int x;
    int y;
    int w;
    int h;
    int floor;
    QString name;
    Building *building;
};

class Building
{
public:
    QList<Room*> RoomList;
};

}

class LotFilesManager : public QObject
{
    Q_OBJECT
public:
    
    static LotFilesManager *instance();
    static void deleteInstance();

    bool generateWorld(WorldDocument *worldDoc);
    bool generateCell(WorldCell *cell);
    bool generateHeader(WorldCell *cell, MapComposite *mapComposite);
    bool generateHeaderAux(WorldCell *cell, MapComposite *mapComposite);
    bool generateChunk(QDataStream &out, WorldCell *cell, MapComposite *mapComposite, int cx, int cy);

    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);

    int getRoomID(int x, int y, int z);

    QString errorString() const { return mError; }

signals:
    
public slots:
    
private:
    uint cellToGid(const Tiled::Cell *cell);

private:
    Q_DISABLE_COPY(LotFilesManager)

    explicit LotFilesManager(QObject *parent = 0);
    ~LotFilesManager();

    static LotFilesManager *mInstance;

    QList<LotFile::Zone*> ZoneList;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<int,LotFile::Tile*> TileMap;
    QVector<QVector<QVector<LotFile::Square> > > mGridData;
    int MaxLevel;
    int Version;
    QList<LotFile::Room*> roomList;
    QList<LotFile::Building*> buildingList;
    QImage ZombieSpawnMap;

    QString mError;
};

#endif // LOTFILESMANAGER_H
