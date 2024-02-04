#ifndef CHUNKMAP_H
#define CHUNKMAP_H

#include <BuildingEditor/buildingtiles.h>

#include <QMap>
#include <QObject>
#include <QRect>
#include <QStringList>
#include <QVector>

class QBuffer;

class BuildingDef;
class IsoCell;
class IsoChunk;
class IsoChunkLevel;
class IsoChunkMap;
class IsoRoom;
class IsoWorld;
class LotHeader;
class RoomDef;
class SliceY;

struct IsoConstants
{
    int CHUNKS_PER_CELL;
    int SQUARES_PER_CHUNK;
    int SQUARES_PER_CELL;

    IsoConstants(bool b256) :
        CHUNKS_PER_CELL(b256 ? 32 : 30),
        SQUARES_PER_CHUNK(b256 ? 8 : 10),
        SQUARES_PER_CELL(CHUNKS_PER_CELL * SQUARES_PER_CHUNK)
    {

    }
};

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

class IsoChunkLevel
{
public:
    IsoChunkLevel();
    IsoChunkLevel *init(IsoChunk *chunk, int level);
    void clear();
    void release();
    void reuseGridSquares();

    static IsoChunkLevel *alloc();

    IsoChunk *mChunk;
    int mLevel;
    QVector<QVector<IsoGridSquare*> > mSquares;
};

class IsoChunk
{
public:
    IsoChunk(IsoCell *cell);
    ~IsoChunk();

    void Load(int wx, int wy);
    void LoadForLater(int wx, int wy);

    void recalcNeighboursNow();
    void update();
    void RecalcSquaresTick();

    void setMinMaxLevel(int minLevel, int maxLevel);
    IsoChunkLevel *getLevelData(int level);
    int getMinLevel() const
    {
        return mMinLevel;
    }
    int getMaxLevel() const
    {
        return mMaxLevel;
    }
    bool isValidLevel(int level) const
    {
        return level >= getMinLevel() && level <= getMaxLevel();
    }
    int squaresIndexOfLevel(int worldSquareZ) const;
    QVector<QVector<IsoGridSquare* > > &getSquaresForLevel(int level);
    void setSquare(int x, int y, int z, IsoGridSquare *square);
    IsoGridSquare *getGridSquare(int x, int y, int z);
    IsoRoom *getRoom(int roomID);
    void ClearGridsquares();
    void reuseGridsquares();

    void Save(bool bSaveQuit);

    IsoCell *Cell;
    const IsoConstants isoConstants;
    int mMinLevel;
    int mMaxLevel;
    QVector<IsoChunkLevel*> mLevels;
    LotHeader *lotheader;
    int wx;
    int wy;
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

    int getWidthInTiles() const { return ChunkGridWidth * isoConstants.SQUARES_PER_CHUNK; }

    int getWorldXMinTiles();
    int getWorldYMinTiles();

//    static const int ChunksPerWidth = 10; // Number of tiles per chunk.
    static const int ChunkGridWidth = 30; // Columns/Rows of chunks displayed.
//    static const int CellSize = ChunksPerWidth * ChunkGridWidth; // Columns/Rows of tiles displayed.
//    static const int MaxLevels = 16;

    IsoCell *cell;
    const IsoConstants isoConstants;

    int WorldCellX;
    int WorldCellY;
    int WorldX;
    int WorldY;

    QVector<QVector<IsoChunk*> > Chunks;
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
    static const int VERSION0 = 0;
    static const int VERSION1 = 1;
    static const int VERSION_LATEST = VERSION1;

    IsoRoom *getRoom(int roomID);
    int getRoomAt(int x, int y, int z);

    QStringList tilesUsed;
    QList<BuildingEditor::BuildingTile> buildingTiles;
    QMap<int,RoomDef*> Rooms;
    QList<BuildingDef*> Buildings;

    int width;
    int height;
    int minLevel;
    int maxLevel;
    int version;
};

class IsoLot
{
public:
    static const int VERSION0 = 0;
    static const int VERSION1 = 1;
    static const int VERSION_LATEST = VERSION1;

    IsoLot(QString directory, int cX, int cY, int wX, int wY, IsoChunk *ch);

    static unsigned char readByte(QDataStream &in);
    static int readInt(QDataStream &in);
    static QString readString(QDataStream &in);

    static QMap<QString,LotHeader*> InfoHeaders;
    class CellCoord
    {
    public:
        CellCoord(int x, int y) :
            x(x),
            y(y)
        {

        }

        int key() const
        {
            return x | (y << 16);
        }

        bool operator<(const CellCoord &rhs) const
        {
            return key() < rhs.key();
        }

        int x;
        int y;
    };

    static QMap<CellCoord,LotHeader*> CellCoordToLotHeader;
    QVector<QVector<QVector<int> > > roomIDs;
    QVector<QVector<QVector<QList<int> > > > data;
    LotHeader *info;
    int wx;
    int wy;
};

class IsoCell
{
public:
    IsoCell(IsoWorld *world);
    ~IsoCell();

    void PlaceLot(IsoLot *lot, int sx, int sy, int sz, bool bClearExisting);
    void PlaceLot(IsoLot *lot, int sx, int sy, int sz, IsoChunk *ch, int WX, int WY, bool bForLater);

    void setCacheGridSquare(int x, int y, int z, IsoGridSquare *square);
    void setCacheGridSquare(int wx, int wy, int x, int y, int z, IsoGridSquare *square);
    void setCacheGridSquareLocal(int x, int y, int z, IsoGridSquare *square);
    IsoGridSquare *getGridSquare(int x, int y, int z);

    const IsoConstants isoConstants;
    int minLevel;
    int maxLevel;
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

    IsoConstants isoConstants;
    QList<QBuffer*> OpenLotPackFiles;
    QMap<QString,QBuffer*> BufferByName;

private:
    CellLoader() :
        isoConstants(false)
    {

    }

    static CellLoader *mInstance;
};

class IsoMetaGrid
{
public:
    IsoMetaGrid(const IsoConstants &isoConstants);

    void Create(const QString &directory);

    QRect cellBounds() const
    {
        return QRect(minx, miny, maxx - minx + 1, maxy - miny + 1);
    }

    QRect chunkBounds() const
    {
        return QRect(minx * isoConstants.CHUNKS_PER_CELL, miny * isoConstants.CHUNKS_PER_CELL,
                   (maxx - minx + 1) * isoConstants.CHUNKS_PER_CELL,
                   (maxy - miny + 1) * isoConstants.CHUNKS_PER_CELL);
    }

    const IsoConstants isoConstants;
    int minx;
    int miny;
    int maxx;
    int maxy;
};

class IsoWorld
{
public:
    IsoWorld(const QString &path, const IsoConstants &isoConstants);
    ~IsoWorld();

    void init();

    int getWidthInTiles() const { return (MetaGrid->maxx - MetaGrid->minx + 1) * isoConstants.SQUARES_PER_CELL; }
    int getHeightInTiles() const { return (MetaGrid->maxy - MetaGrid->miny + 1) * isoConstants.SQUARES_PER_CELL; }

    QRect tileBounds() const
    {
        return QRect(MetaGrid->minx * isoConstants.SQUARES_PER_CELL,
                     MetaGrid->miny * isoConstants.SQUARES_PER_CELL,
                     getWidthInTiles(), getHeightInTiles());
    }

    IsoConstants isoConstants;
    IsoMetaGrid *MetaGrid;
    IsoCell *CurrentCell;
    QString Directory;
};

#endif // CHUNKMAP_H
