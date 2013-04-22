#ifndef CHUNKMAP_H
#define CHUNKMAP_H

#include <QMap>
#include <QObject>
#include <QRect>
#include <QStringList>
#include <QVector>

class QBuffer;

class BuildingDef;
class IsoCell;
class IsoChunk;
class IsoChunkMap;
class IsoRoom;
class IsoWorld;
class LotHeader;
class RoomDef;
class SliceY;

class IsoCoord
{
public:
    IsoCoord(int x, int y);
    QPoint toWorld();
    QPoint toCell(int cellX, int cellY);
    QPoint toChunkMap(const IsoChunkMap &cm);
    QPoint toChunk(const IsoChunk &chunk);
private:
    int x;
    int y;
};

class IsoGridSquare
{
public:

    IsoGridSquare(IsoCell *cell, SliceY *slice, int x, int y, int z);

    static IsoGridSquare *getNew(IsoCell *cell, SliceY *slice, int x, int y, int z);

    void discard();

    void RecalcAllWithNeighbours(bool bDoReverse);

    void setX(int x) { this->x = x; }
    void setY(int y) { this->y = y; }
    void setZ(int z) { this->z = z; }

    void setRoomID(int roomID);

    int roomID;
    int ID;
    int x;
    int y;
    int z;

    QStringList tiles; // tnb

    IsoChunk *chunk;
    IsoRoom *room;

    static int IDMax;
    static QList<IsoGridSquare*> isoGridSquareCache;
};

class IsoChunk
{
public:
    IsoChunk(IsoCell *cell = 0);

    void Load(int wx, int wy);
    void LoadForLater(int wx, int wy);

    void recalcNeighboursNow();
    void update();
    void RecalcSquaresTick();

    void setSquare(int x, int y, int z, IsoGridSquare *square);
    IsoGridSquare *getGridSquare(int x, int y, int z);
    IsoRoom *getRoom(int roomID);
    void ClearGridsquares();
    void reuseGridsquares();

    void Save(bool bSaveQuit);

    QVector<QVector<QVector<IsoGridSquare*> > > squares;
    LotHeader *lotheader;
    int wx;
    int wy;

    IsoCell *Cell;
};

class IsoChunkMap
{
public:
    explicit IsoChunkMap(IsoCell *cell);
    ~IsoChunkMap();

    void update();
    void LoadChunk(int wx, int wy, int x, int y);
    void LoadChunkForLater(int wx, int wy, int x, int y);

    IsoChunk *getChunkForGridSquare(int x, int y);
    void setGridSquare(IsoGridSquare *square, int wx, int wy, int x, int y, int z);
    void setGridSquare(IsoGridSquare *square, int x, int y, int z);
    IsoGridSquare *getGridSquare(int x, int y, int z);
    IsoChunk *getChunk(int x, int y);
    void setChunk(int x, int y, IsoChunk *c);

    void LoadLeft();
    void LoadRight();
    void LoadUp();
    void LoadDown();

    void UpdateCellCache();

    void Up();
    void Down();
    void Left();
    void Right();

    int getWorldXMin();
    int getWorldYMin();
    
    void ProcessChunkPos(int x, int y);

    int getWidthInTiles() const { return CellSize; }

    int getWorldXMinTiles();
    int getWorldYMinTiles();

    static const int ChunkDiv = 10;
    static const int ChunksPerWidth = 10; // Number of tiles per chunk.
    static const int ChunkGridWidth = 30; // Columns/Rows of chunks displayed.
    static const int CellSize = ChunksPerWidth * ChunkGridWidth; // Columns/Rows of tiles displayed.
    static const int MaxLevels = 16;

    IsoCell *cell;

    int WorldCellX;
    int WorldCellY;
    int WorldX;
    int WorldY;

    QVector<QVector<IsoChunk*> > Chunks;

    int XMinTiles;
    int XMaxTiles;
    int YMinTiles;
    int YMaxTiles;
};

class RoomRect
{
public:
    RoomRect(int x, int y, int w, int h) :
        x(x),
        y(y),
        w(w),
        h(h)
    {

    }

    bool contains(int x, int y)
    {
        return QRect(x, y, w, h).contains(x, y);
    }

    int x;
    int y;
    int w;
    int h;
};

class MetaObject
{
public:
    MetaObject(int type, int x, int y, RoomDef *def) :
        type(type),
        x(x),
        y(y),
        def(def)
    {

    }

    int type;
    int x;
    int y;
    RoomDef *def;
};

class RoomDef
{
public:
    RoomDef(int id, const QString &name) :
        ID(id),
        name(name)
    {

    }

    void CalculateBounds() {}

    int ID;
    int x;
    int y;
    int x2;
    int y2;
    int level;
    BuildingDef *building;
    QString name;
    QList<RoomRect*> rects;
    QList<MetaObject*> objects;

};

class BuildingDef
{
public:

    BuildingDef(int id) :
        ID(id)
    {

    }

    void CalculateBounds() {}

    int ID;
    QList<RoomDef*> rooms;
};

class LotHeader
{
public:
    IsoRoom *getRoom(int roomID);
    int getRoomAt(int x, int y, int z);

    QStringList tilesUsed;
    QMap<int,RoomDef*> Rooms;
    QList<BuildingDef*> Buildings;

    int width;
    int height;
    int levels;
    int version;
};

class IsoLot
{
public:
    IsoLot(QString directory, int cX, int cY, int wX, int wY, IsoChunk *ch);

    static unsigned char readByte(QDataStream &in);
    static int readInt(QDataStream &in);
    static QString readString(QDataStream &in);

    static QMap<QString,LotHeader*> InfoHeaders;
    QVector<QVector<QVector<int> > > roomIDs;
    QVector<QVector<QVector<QList<int> > > > data;
    LotHeader *info;
    int wx;
    int wy;
};

class IsoCell
{
public:
    IsoCell(IsoWorld *world, /*IsoSpriteManager &spr, */int width, int height);
    ~IsoCell();

    void PlaceLot(IsoLot *lot, int sx, int sy, int sz, bool bClearExisting);
    void PlaceLot(IsoLot *lot, int sx, int sy, int sz, IsoChunk *ch, int WX, int WY, bool bForLater);

    void setCacheGridSquare(int x, int y, int z, IsoGridSquare *square);
    void setCacheGridSquare(int wx, int wy, int x, int y, int z, IsoGridSquare *square);
    void setCacheGridSquareLocal(int x, int y, int z, IsoGridSquare *square);
    IsoGridSquare *getGridSquare(int x, int y, int z);

    static int MaxHeight;
    int width;
    int height;
    IsoChunkMap *ChunkMap;
    IsoWorld *World;

    QVector<QVector<QVector<IsoGridSquare*> > > gridSquares;
};

class CellLoader
{
public:
    static CellLoader *instance();
    static void LoadCellBinaryChunk(IsoCell *cell, int wx, int wy, IsoChunk *chunk);
    static void LoadCellBinaryChunkForLater(IsoCell *cell, int wx, int wy, IsoChunk *chunk);
    static IsoCell *LoadCellBinaryChunk(IsoWorld *world, /*IsoSpriteManager &spr, */int wx, int wy);

    QBuffer *openLotPackFile(const QString &name);
    void reset();

    QList<QBuffer*> OpenLotPackFiles;
    QMap<QString,QBuffer*> BufferByName;

    static CellLoader *mInstance;
};

class IsoMetaGrid
{
public:
    IsoMetaGrid();

    void Create(const QString &directory);

    int minx;
    int miny;
    int maxx;
    int maxy;
};

class IsoWorld
{
public:
    IsoWorld(const QString &path);
    ~IsoWorld();

    void init();

    int getWidthInTiles() { return (MetaGrid->maxx - MetaGrid->minx + 1) * CurrentCell->width; }
    int getHeightInTiles() { return (MetaGrid->maxy - MetaGrid->miny + 1) * CurrentCell->height; }

    IsoMetaGrid *MetaGrid;
    IsoCell *CurrentCell;
    QString Directory;
};

#endif // CHUNKMAP_H
