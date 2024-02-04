#include "chunkmap.h"

#include "worldconstants.h"

#include <qmath.h>
#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStack>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<quint32> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
#if QT_VERSION >= 060000
// QVector is an alias for QList
template class __declspec(dllimport) QList<QList<quint32> >;
#else
template class __declspec(dllimport) QVector<QVector<quint32> >;
#endif
#endif

/////

int IsoGridSquare::IDMax = -1;
QList<IsoGridSquare*> IsoGridSquare::isoGridSquareCache;

IsoGridSquare::IsoGridSquare(IsoCell *cell, SliceY *slice, int x, int y, int z) :
    ID(++IDMax),
    x(x),
    y(y),
    z(z)
{
}

IsoGridSquare *IsoGridSquare::getNew(IsoCell *cell, SliceY *slice, int x, int y, int z)
{
    if (isoGridSquareCache.isEmpty())
        return new IsoGridSquare(cell, slice, x, y, z);

    IsoGridSquare *sq = isoGridSquareCache.takeFirst();
    sq->ID = ++IDMax;
    sq->x = x;
    sq->y = y;
    sq->z = z;

    return sq;
}

void IsoGridSquare::discard()
{
    chunk = 0;
    roomID = -1;
    room = 0;
    ID = 0;

    tiles.clear();

    isoGridSquareCache += this;
}

void IsoGridSquare::RecalcAllWithNeighbours(bool bDoReverse)
{
}

void IsoGridSquare::setRoomID(int roomID)
{
    this->roomID = roomID;
    this->room = 0; // tnb
    if (roomID != -1)
        room = chunk->getRoom(roomID);
}

/////

IsoChunkLevel::IsoChunkLevel() :
    mChunk(nullptr),
    mLevel(0)
{
}

IsoChunkLevel *IsoChunkLevel::init(IsoChunk *chunk, int level)
{
    mChunk = chunk;
    mLevel = level;
    mSquares.resize(mChunk->isoConstants.SQUARES_PER_CHUNK);
    for (auto &v : mSquares) {
        v.resize(mChunk->isoConstants.SQUARES_PER_CHUNK);
        v.fill(nullptr);
    }
    return this;
}

void IsoChunkLevel::clear()
{
    for (int x = 0; x < mSquares.size(); x++) {
        for (int y = 0; y < mSquares[x].size(); y++) {
            IsoGridSquare *sq = mSquares[x][y];
            if (sq == nullptr)
                continue;
            mSquares[x][y] = nullptr;
            sq->discard();
        }
    }
}

static QStack<IsoChunkLevel*> s_ChunkLevelPool;

void IsoChunkLevel::release()
{
    s_ChunkLevelPool.push(this);
}

void IsoChunkLevel::reuseGridSquares()
{
    clear();
}

IsoChunkLevel *IsoChunkLevel::alloc()
{
    if (s_ChunkLevelPool.isEmpty()) {
        return new IsoChunkLevel();
    }
    return s_ChunkLevelPool.pop();
}

/////

IsoChunk::IsoChunk(IsoCell *cell) :
    Cell(cell),
    isoConstants(cell->isoConstants),
    lotheader(nullptr),
    wx(0),
    wy(0)
{
    mMinLevel = mMaxLevel = 0;
    mLevels.resize(1);
    mLevels[0] = IsoChunkLevel::alloc()->init(this, mMinLevel);
}

IsoChunk::~IsoChunk()
{
    for (IsoChunkLevel *chunkLevel : mLevels) {
        chunkLevel->clear();
        chunkLevel->release();
    }
    mLevels.clear();
}

void IsoChunk::Load(int wx, int wy)
{
    this->wx = wx;
    this->wy = wy;

    CellLoader::LoadCellBinaryChunk(Cell/*IsoWorld.instance.CurrentCell*/, wx, wy, this);
}

void IsoChunk::LoadForLater(int wx, int wy)
{
    this->wx = wx;
    this->wy = wy;

    CellLoader::LoadCellBinaryChunkForLater(Cell/*IsoWorld.instance.CurrentCell*/, wx, wy, this);
}

void IsoChunk::recalcNeighboursNow()
{
}

void IsoChunk::update()
{
}

void IsoChunk::RecalcSquaresTick()
{
}

void IsoChunk::setMinMaxLevel(int minLevel, int maxLevel)
{
    if ((minLevel == mMinLevel) && (maxLevel == mMaxLevel)) {
        return;
    }
    for (int z = mMinLevel; z <= mMaxLevel; z++) {
        if ((z < minLevel) || (z > maxLevel)) {
            IsoChunkLevel *chunkLevel = mLevels[z - mMinLevel];
            if (chunkLevel != nullptr) {
                chunkLevel->clear();
                chunkLevel->release();
                mLevels[z - mMinLevel] = nullptr;
            }
        }
    }
    QVector<IsoChunkLevel*> newLevels;
    newLevels.resize(maxLevel - minLevel + 1);
    for (int z = minLevel; z <= maxLevel; z++) {
        if (isValidLevel(z)) {
            newLevels[z - minLevel] = mLevels[z - mMinLevel];
        } else {
            newLevels[z - minLevel] = IsoChunkLevel::alloc()->init(this, z);
        }
    }
    mMinLevel = minLevel;
    mMaxLevel = maxLevel;
    mLevels = newLevels;
}

IsoChunkLevel *IsoChunk::getLevelData(int level)
{
    if (isValidLevel(level)) {
        return mLevels[level - mMinLevel];
    }
    return nullptr;
}

int IsoChunk::squaresIndexOfLevel(int worldSquareZ) const
{
    return worldSquareZ - mMinLevel;
}

QVector<QVector<IsoGridSquare* > >& IsoChunk::getSquaresForLevel(int level)
{
    return getLevelData(level)->mSquares;
}

void IsoChunk::setSquare(int x, int y, int z, IsoGridSquare *square)
{
    setMinMaxLevel(std::min(getMinLevel(), z), std::max(getMaxLevel(), z));
    auto& squares = getSquaresForLevel(z);
    squares[x][y] = square;
    if (square)
        square->chunk = this;
}

IsoGridSquare *IsoChunk::getGridSquare(int x, int y, int z)
{
    if ((x < 0) || (x >= isoConstants.SQUARES_PER_CHUNK) || (y < 0) || (y >= isoConstants.SQUARES_PER_CHUNK) || (z > mMaxLevel) || (z < mMinLevel))
    {
        return nullptr;
    }
    auto& squares = getSquaresForLevel(z);
    return squares[x][y];
}

IsoRoom *IsoChunk::getRoom(int roomID)
{
    return lotheader->getRoom(roomID);
}

void IsoChunk::ClearGridsquares()
{
}

void IsoChunk::reuseGridsquares()
{
    for (IsoChunkLevel *chunkLevel : mLevels) {
        chunkLevel->reuseGridSquares();
        chunkLevel->clear();
        chunkLevel->release();
    }
    mLevels.clear();
    mLevels.resize(1);
    mMinLevel = mMaxLevel = 0;
    mLevels[0] = IsoChunkLevel::alloc()->init(this, mMinLevel);
}

void IsoChunk::Save(bool bSaveQuit)
{
    // save out to a .bin file here

    reuseGridsquares();
}

/////

IsoChunkMap::IsoChunkMap(IsoCell *cell) :
    cell(cell),
    isoConstants(cell->isoConstants),
    WorldX(-1000),
    WorldY(-1000)
{
    Chunks.resize(ChunkGridWidth);
    for (int x = 0; x < ChunkGridWidth; x++)
        Chunks[x].resize(ChunkGridWidth);
}

IsoChunkMap::~IsoChunkMap()
{
    for (int x = 0; x < Chunks.size(); x++)
        qDeleteAll(Chunks[x]);
}

void IsoChunkMap::update()
{
}

void IsoChunkMap::LoadChunk(int wx, int wy, int x, int y)
{
//    qDebug() << "IsoChunkMap::LoadChunk" << x << y;
    IsoChunk *chunk = new IsoChunk(cell);
    setChunk(x, y, chunk);

    chunk->Load(wx, wy);
    chunk->recalcNeighboursNow();
}

void IsoChunkMap::LoadChunkForLater(int wx, int wy, int x, int y)
{
//    qDebug() << "IsoChunkMap::LoadChunkForLater" << wx << wy;
    IsoChunk *chunk = new IsoChunk(cell);
    setChunk(x, y, chunk);

    chunk->LoadForLater(wx, wy);
}

IsoChunk *IsoChunkMap::getChunkForGridSquare(int x, int y)
{
    x -= getWorldXMin() * isoConstants.SQUARES_PER_CHUNK;
    y -= getWorldYMin() * isoConstants.SQUARES_PER_CHUNK;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles())) {
        return 0;
    }
    IsoChunk *c = getChunk(x / isoConstants.SQUARES_PER_CHUNK, y / isoConstants.SQUARES_PER_CHUNK);

    return c;
}

void IsoChunkMap::setGridSquare(IsoGridSquare *square, int wx, int wy, int x, int y, int z)
{
    x -= wx * isoConstants.SQUARES_PER_CHUNK;
    y -= wy * isoConstants.SQUARES_PER_CHUNK;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles()) || (z < MIN_WORLD_LEVEL) || (z > MAX_WORLD_LEVEL))
        return;
    IsoChunk *c = getChunk(x / isoConstants.SQUARES_PER_CHUNK, y / isoConstants.SQUARES_PER_CHUNK);
    if (c == 0)
        return;
    c->setSquare(x % isoConstants.SQUARES_PER_CHUNK, y % isoConstants.SQUARES_PER_CHUNK, z, square);
}

void IsoChunkMap::setGridSquare(IsoGridSquare *square, int x, int y, int z) {
    x -= getWorldXMin() * isoConstants.SQUARES_PER_CHUNK;
    y -= getWorldYMin() * isoConstants.SQUARES_PER_CHUNK;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles()) || (z < MIN_WORLD_LEVEL) || (z > MAX_WORLD_LEVEL)) {
        return;
    }
    IsoChunk *c = getChunk(x / isoConstants.SQUARES_PER_CHUNK, y / isoConstants.SQUARES_PER_CHUNK);
    if (c == 0)
        return;
    c->setSquare(x % isoConstants.SQUARES_PER_CHUNK, y % isoConstants.SQUARES_PER_CHUNK, z, square);
}

IsoGridSquare *IsoChunkMap::getGridSquare(int x, int y, int z)
{
    x -= getWorldXMin() * isoConstants.SQUARES_PER_CHUNK;
    y -= getWorldYMin() * isoConstants.SQUARES_PER_CHUNK;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles()) || (z < MIN_WORLD_LEVEL) || (z > MAX_WORLD_LEVEL)) {
        return 0;
    }
    IsoChunk *c = getChunk(x / isoConstants.SQUARES_PER_CHUNK, y / isoConstants.SQUARES_PER_CHUNK);
    if (c == 0)
        return 0;
    return c->getGridSquare(x % isoConstants.SQUARES_PER_CHUNK, y % isoConstants.SQUARES_PER_CHUNK, z);
}

IsoChunk *IsoChunkMap::getChunk(int x, int y)
{
    if ((x < 0) || (x >= ChunkGridWidth) || (y < 0) || (y >= ChunkGridWidth)) {
        return 0;
    }
    return Chunks[x][y];
}

void IsoChunkMap::setChunk(int x, int y, IsoChunk *c)
{
    if (x < 0 || x >= ChunkGridWidth || y < 0 || y >= ChunkGridWidth) return;
    if (c && Chunks[x][y]) {
        int i = 0;
    }

    Q_ASSERT(!c || (Chunks[x][y] == 0));
    Chunks[x][y] = c;
}

void IsoChunkMap::UpdateCellCache()
{
    int i = getWidthInTiles();
    for (int x = 0; x < i; ++x) {
        for (int y = 0; y < i; ++y) {
            for (int z = MIN_WORLD_LEVEL; z <= MAX_WORLD_LEVEL; ++z) {
                IsoGridSquare *sq = getGridSquare(x + getWorldXMinTiles(), y + getWorldYMinTiles(), z);
                cell->setCacheGridSquareLocal(x, y, z, sq);
            }
        }
    }
}

int IsoChunkMap::getWorldXMin()
{
    return (WorldX - (ChunkGridWidth / 2));
}

int IsoChunkMap::getWorldYMin()
{
    return (WorldY - (ChunkGridWidth / 2));
}

int IsoChunkMap::getWorldXMinTiles()
{
    return getWorldXMin() * isoConstants.SQUARES_PER_CHUNK;
}

int IsoChunkMap::getWorldYMinTiles()
{
    return getWorldYMin() * isoConstants.SQUARES_PER_CHUNK;
}

/////

IsoRoom *LotHeader::getRoom(int roomID)
{
    RoomDef *r = Rooms[roomID];
    return 0;
}

int LotHeader::getRoomAt(int x, int y, int z)
{
    foreach (RoomDef *def, Rooms) {
        if (def->level != z) continue;
        foreach (RoomRect *r, def->rects) {
            if (r->contains(x, y))
                return def->ID;
        }
    }
    return -1;
}

/////

QMap<QString,LotHeader*> IsoLot::InfoHeaders;

IsoLot::IsoLot(QString directory, int cX, int cY, int wX, int wY, IsoChunk *ch)
{
    this->wx = wX;
    this->wy = wY;

    const IsoConstants isoConstants = ch->isoConstants;

    float fwx = wX;
    float fwy = wY;
    fwx /= isoConstants.CHUNKS_PER_CELL;
    fwy /= isoConstants.CHUNKS_PER_CELL;
    wX = qFloor(fwx);
    wY = qFloor(fwy);

    QString filenameheader = QString::fromLatin1("%1/%2_%3.lotheader").arg(directory).arg(wX).arg(wY);
    if (!InfoHeaders.contains(filenameheader)) {
        info = 0;
        return; // chunk not found on disk
    }
    Q_ASSERT(InfoHeaders.contains(filenameheader));
    info = InfoHeaders[filenameheader];

    ch->lotheader = info;

    data.resize(isoConstants.SQUARES_PER_CHUNK);
    for (int x = 0; x < isoConstants.SQUARES_PER_CHUNK; x++) {
        data[x].resize(isoConstants.SQUARES_PER_CHUNK);
        for (int y = 0; y < data[x].size(); y++) {
            data[x][y].resize(info->maxLevel - info->minLevel + 1);
        }
    }

    roomIDs.resize(isoConstants.SQUARES_PER_CHUNK);
    for (int x = 0; x < roomIDs.size(); x++) {
        roomIDs[x].resize(isoConstants.SQUARES_PER_CHUNK);
        for (int y = 0; y < roomIDs[x].size(); y++) {
            roomIDs[x][y].fill(-1, info->maxLevel - info->minLevel + 1);
        }
    }

    {
        QString filenamepack = QString::fromLatin1("%1/world_%2_%3.lotpack").arg(directory).arg(wX).arg(wY);
        QBuffer *fo = CellLoader::instance()->openLotPackFile(filenamepack);
        if (!fo)
            return; // exception!

        fo->seek(0);
        QDataStream in(fo);
        in.setByteOrder(QDataStream::LittleEndian);

//        qDebug() << "reading chunk" << wX << wY << "from" << filenamepack;

        int version = IsoLot::VERSION0;
        char magic[4] = { 0 };
        in.readRawData(magic, 4);
        if (magic[0] == 'L' && magic[1] == 'O' && magic[2] == 'T' && magic[3] == 'P') {
            version = IsoLot::readInt(in);
            if (version < IsoLot::VERSION0 || version > IsoLot::VERSION_LATEST) {
                return;
            }
        } else {
            fo->seek(0);
        }

        int skip = 0;

        int lwx = this->wx - (wX * isoConstants.CHUNKS_PER_CELL);
        int lwy = this->wy - (wY * isoConstants.CHUNKS_PER_CELL);
        int index = lwx * isoConstants.CHUNKS_PER_CELL + lwy;
        fo->seek((version >= IsoLot::VERSION1 ? 8 : 0) + 4 + index * 8);
        qint64 pos;
        in >> pos;
        fo->seek(pos);
        for (int z = info->minLevel; z <= info->maxLevel; ++z) {
            for (int x = 0; x < isoConstants.SQUARES_PER_CHUNK; ++x) {
                for (int y = 0; y < isoConstants.SQUARES_PER_CHUNK; ++y) {
                    if (skip > 0) {
                        --skip;
                    } else {
                        int count = readInt(in);
                        if (count == -1) {
                            skip = readInt(in);
                            if (skip > 0) {
                                --skip;
                            }
                        }  else {
#if 0
                            if (count <= 1)
                                continue;
#endif
                            int room = readInt(in);
                            roomIDs[x][y][z - info->minLevel] = room;

                            if (count <= 1) {
                                int brk = 1;
                            }

                            Q_ASSERT(count > 1 && count < 30);

                            for (int n = 1; n < count; ++n) {
                                int d = readInt(in);

                                this->data[x][y][z - info->minLevel] += d;
                            }
                        }
                    }
                }
            }
        }
    }
}

unsigned char IsoLot::readByte(QDataStream &in)
{
    quint8 b;
    in >> b;
    return b;
}

int IsoLot::readInt(QDataStream &in)
{
    qint32 ret;
    in >> ret;
    return ret;
}

QString IsoLot::readString(QDataStream &in)
{
    QString ret;
    quint8 ch;
    while (!in.atEnd()) {
        in >> ch;
        if (ch == '\n') break;
        ret += QLatin1Char(ch);
    }
    return ret;
}

/////

CellLoader *CellLoader::mInstance = 0;

CellLoader *CellLoader::instance()
{
    if (!mInstance)
        mInstance = new CellLoader;
    return mInstance;
}

void CellLoader::LoadCellBinaryChunk(IsoCell *cell, int wx, int wy, IsoChunk *chunk)
{
    instance()->isoConstants = cell->isoConstants;
    const IsoConstants& isoConstants = instance()->isoConstants;

    int WX = wx / isoConstants.CHUNKS_PER_CELL;
    int WY = wy / isoConstants.CHUNKS_PER_CELL;

    IsoLot *lot = new IsoLot(cell->World->Directory, WX, WY, wx, wy, chunk);

    cell->PlaceLot(lot, 0, 0, 0, chunk, wx, wy, false);

    delete lot;
}

void CellLoader::LoadCellBinaryChunkForLater(IsoCell *cell, int wx, int wy, IsoChunk *chunk)
{
    instance()->isoConstants = cell->isoConstants;
    const IsoConstants& isoConstants = instance()->isoConstants;

    int WX = wx / isoConstants.CHUNKS_PER_CELL;
    int WY = wy / isoConstants.CHUNKS_PER_CELL;

    IsoLot *lot = new IsoLot(cell->World->Directory, WX, WY, wx, wy, chunk);

    cell->PlaceLot(lot, 0, 0, lot->info->minLevel, chunk, wx, wy, true);

    delete lot;
}

IsoCell *CellLoader::LoadCellBinaryChunk(IsoWorld *world, int wx, int wy)
{
    instance()->isoConstants = world->isoConstants;
    const IsoConstants& isoConstants = instance()->isoConstants;

    IsoCell *cell = new IsoCell(world);

    int minwx = wx - (IsoChunkMap::ChunkGridWidth / 2);
    int minwy = wy - (IsoChunkMap::ChunkGridWidth / 2);
    int maxwx = wx + IsoChunkMap::ChunkGridWidth / 2;
    int maxwy = wy + IsoChunkMap::ChunkGridWidth / 2;

    cell->ChunkMap->WorldX = wx;
    cell->ChunkMap->WorldY = wy;

    for (int x = minwx; x < maxwx; ++x) {
        for (int y = minwy; y < maxwy; ++y) {
#if 1
            if (!cell->World->MetaGrid->chunkBounds().contains(x, y))
                continue;
#endif
            if ((x == wx) && (y == wy))
                cell->ChunkMap->LoadChunk(x, y, x - minwx, y - minwy);
            else
                cell->ChunkMap->LoadChunkForLater(x, y, x - minwx, y - minwy);
        }
    }
    cell->ChunkMap->UpdateCellCache();

    return cell;
}

QBuffer *CellLoader::openLotPackFile(const QString &name)
{
    if (BufferByName.contains(name))
        return BufferByName[name];

    while (OpenLotPackFiles.size() > 10) {
        BufferByName.remove(BufferByName.key(OpenLotPackFiles.first()));
        delete OpenLotPackFiles.takeFirst();
    }
    QFile f(name);
    if (!f.open(QFile::ReadOnly))
        return 0;

    QBuffer *b = new QBuffer();
    b->open(QBuffer::ReadWrite);
    b->write(f.readAll());
    b->seek(0);
    OpenLotPackFiles += b;
    BufferByName[name] = b;
    return b;
}

void CellLoader::reset()
{
    qDeleteAll(OpenLotPackFiles);
    OpenLotPackFiles.clear();
    BufferByName.clear();
}

/////

IsoCell::IsoCell(IsoWorld *world) :
    isoConstants(world->isoConstants),
    ChunkMap(new IsoChunkMap(this)),
    World(world)
{
    int squares_per_side = ChunkMap->getWidthInTiles();
    gridSquares.resize(squares_per_side);
    for (int x = 0; x < squares_per_side; x++) {
        gridSquares[x].resize(squares_per_side);
        for (int y = 0; y < squares_per_side; y++)
            gridSquares[x][y].resize(MAX_WORLD_LEVELS);
    }
}

IsoCell::~IsoCell()
{
    delete ChunkMap;
}

void IsoCell::PlaceLot(IsoLot *lot, int sx, int sy, int sz, bool bClearExisting)
{
}

void IsoCell::PlaceLot(IsoLot *lot, int sx, int sy, int sz, IsoChunk *ch, int WX, int WY, bool bForLater)
{
    WX *= isoConstants.SQUARES_PER_CHUNK;
    WY *= isoConstants.SQUARES_PER_CHUNK;

    if (lot->info) { /* try */
        int maxLevel = sz + (lot->info->maxLevel - lot->info->minLevel + 1);
        for (int x = WX + sx; x < WX + sx + isoConstants.SQUARES_PER_CHUNK; ++x) {
            for (int y = WY + sy; y < WY + sy + isoConstants.SQUARES_PER_CHUNK; ++y) {
                bool bDoIt = true;
                for (int z = sz; z < maxLevel; ++z) {
                    bDoIt = false;

                    if ((x >= WX + isoConstants.SQUARES_PER_CHUNK) || (y >= WY + isoConstants.SQUARES_PER_CHUNK) || (x < WX) || (y < WY))
                        continue;
                    if (z < MIN_WORLD_LEVEL || z > MAX_WORLD_LEVEL)
                        continue;

                    auto& v1 = lot->data[x - (WX + sx)];
                    auto& v2 = v1[y - (WY + sy)];
                    QList<int> ints = v2[z - sz];
                    IsoGridSquare *square = 0;
                    if (ints.isEmpty())
                        continue;
                    int s = ints.size();
                    int n = 0;

                    if (square == 0)  {
                        square = ch->getGridSquare(x - WX, y - WY, z);

                        if (square == 0) {
                            square = IsoGridSquare::getNew(this, 0, x, y, z);

                            ch->setSquare(x - WX, y - WY, z, square);

                            setCacheGridSquare(x, y, z, square);
                        }

                        for (int xx = -1; xx <= 1; ++xx)  {
                            for (int yy = -1; yy <= 1; ++yy) {
                                if ((xx != 0) || (yy != 0)) {
                                    if (xx + x - WX < 0) continue;
                                    if (xx + x - WX < isoConstants.SQUARES_PER_CHUNK) {
                                        if (yy + y - WY < 0)
                                            continue;
                                        if (yy + y - WY >= isoConstants.SQUARES_PER_CHUNK)
                                            continue;
                                        IsoGridSquare *square2 = ch->getGridSquare(x + xx - WX, y + yy - WY, z);

                                        if (square2 != 0)
                                            continue;
                                        square2 = IsoGridSquare::getNew(this, 0, x + xx, y + yy, z);

                                        ch->setSquare(x + xx - WX, y + yy - WY, z, square2);

                                        setCacheGridSquare(x + xx, y + yy, z, square2);
                                    }
                                }
                            }
                        }

                        square->setX(x);
                        square->setY(y);
                        square->setZ(z);

#if 1
                        int roomID = lot->roomIDs[(x - (WX + sx))][(y - (WY + sy))][(z - sz) - lot->info->minLevel];
#else
                        int roomID = ch->lotheader->getRoomAt(x, y, z);
#endif
                        square->setRoomID(roomID);
                    }

#if 1
#elif 0 // BUG IN LEMMY'S???
                    if ((s > 0) && (z > MaxHeight))
                        MaxHeight = z;
#else
                    if ((s > 1) && (z > MaxHeight))
                        MaxHeight = z;
#endif

#if 1
                    square->tiles.clear();
                    for (n = 0; n < s; ++n) {
                        QString tile = lot->info->tilesUsed[ints.at(n)];
                        square->tiles += tile;
#else
                    for (n = 0; n < s; ++n) {
                        QString tile = lot->info->tilesUsed[ints.at(n)];
                        IsoSprite spr = (IsoSprite)this.SpriteManager.NamedMap.get(tile);
                        if (spr == null) {
                            Logger.getLogger(GameApplet.class.getName()).log(Level.SEVERE, "Missing tile definition: " + tile);
                        } else {
                            if ((n == 0) && (spr.getProperties().Is(IsoFlagType.solidfloor)) &&
                                    (((!(spr.Properties.Is(IsoFlagType.hidewalls))) || (ints.size() > 1)))) {
                                bDoIt = true;
                            }
                            if ((bDoIt) && (n == 0)) {
                                square.getObjects().clear();
                            }

                            CellLoader.DoTileObjectCreation(spr, spr.getType(), this, x, y, z, BedList, false, tile);
                        }
#endif
                    }
                }
            }
        }
    }
#if 1
    else {
#else
      catch (Exception ex)
      {
          System.out.println("Failed to load chunk, blocking out area");
          ex.printStackTrace();
          for (int x = WX + sx; x < WX + sx + isoConstants.SQUARES_PER_CHUNK; ++x) {
              for (int y = WY + sy; y < WY + sy + isoConstants.SQUARES_PER_CHUNK; ++y) {
                  for (int z = sz; z < sz + IsoChunkMap::MaxLevels; ++z)
                  {
                      ch->setSquare(x - WX, y - WY, z, nullptr);
                      setCacheGridSquare(x, y, z, nullptr);
                  }
              }
          }
#endif
          return;
      }

      if (bForLater)
          return;

      int maxLevel = sz + (lot->info->maxLevel - lot->info->minLevel + 1);

      for (int x = sx + WX - 1; x < sx + WX + lot->info->width + 1; ++x)
          for (int y = sy + WY - 1; y < sy + WY + lot->info->height + 1; ++y)
              for (int z = sz; z < maxLevel; ++z) {
                  IsoGridSquare *square = getGridSquare(x, y, z);
                  if (square == 0)
                      continue;
                  square->RecalcAllWithNeighbours(false);
              }
}

void IsoCell::setCacheGridSquare(int x, int y, int z, IsoGridSquare *square)
{
    x -= ChunkMap->getWorldXMinTiles();
    y -= ChunkMap->getWorldYMinTiles();
    setCacheGridSquareLocal(x, y, z, square);
}

void IsoCell::setCacheGridSquare(int wx, int wy, int x, int y, int z, IsoGridSquare *square)
{
    x -= wx * isoConstants.SQUARES_PER_CHUNK;
    y -= wy * isoConstants.SQUARES_PER_CHUNK;
    setCacheGridSquareLocal(x, y, z, square);
}

void IsoCell::setCacheGridSquareLocal(int x, int y, int z, IsoGridSquare *square) {
    if ((z > MAX_WORLD_LEVEL) || (z < MIN_WORLD_LEVEL)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
        return;
//    if (square) qDebug() << "IsoCell::setCacheGridSquare" << x << y << z << square->tiles;
    gridSquares[x][y][z + WORLD_GROUND_LEVEL] = square;
}

IsoGridSquare *IsoCell::getGridSquare(int x, int y, int z)
{
    x -= ChunkMap->getWorldXMinTiles();
    y -= ChunkMap->getWorldYMinTiles();
    if ((z > MAX_WORLD_LEVEL) || (z < MIN_WORLD_LEVEL)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
    {
        return nullptr;
    }
    return gridSquares[x][y][z + WORLD_GROUND_LEVEL];
}

/////

IsoMetaGrid::IsoMetaGrid(const IsoConstants& isoConstants) :
    isoConstants(isoConstants),
    minx(100000),
    miny(100000),
    maxx(-100000),
    maxy(-100000)
{
}

void IsoMetaGrid::Create(const QString &directory)
{
    QDir fo(directory);
    QStringList filters(QString::fromLatin1("*.lotheader"));
    foreach (QFileInfo info, fo.entryInfoList(filters)) {
        QStringList split = info.baseName().split(QLatin1Char('_'));
        int x = split[0].toInt();
        int y = split[1].toInt();
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }

    int CHUNKS_PER_CELL = isoConstants.CHUNKS_PER_CELL;
    int SQUARES_PER_CHUNK = isoConstants.SQUARES_PER_CHUNK;
    int SQUARES_PER_CELL = isoConstants.SQUARES_PER_CELL;

    for (int wX = minx; wX <= maxx; ++wX) {
        for (int wY = miny; wY <= maxy; ++wY) {
            QString filenameheader = QString::fromLatin1("%1/%2_%3.lotheader").arg(directory).arg(wX).arg(wY);
            if (IsoLot::InfoHeaders.contains(filenameheader))
                continue;

            QFile fo(filenameheader);
            if (!fo.exists())
                continue;
            if (!fo.open(QFile::ReadOnly))
                continue;

            LotHeader *info = new LotHeader;

            QBuffer buffer;
            buffer.open(QBuffer::ReadWrite);
            buffer.write(fo.readAll());
            fo.close();
            buffer.seek(0);

            QDataStream in(&buffer);
            in.setByteOrder(QDataStream::LittleEndian);

            char magic[4] = { 0 };
            in.readRawData(magic, 4);
            if (magic[0] == 'L' && magic[1] == 'O' && magic[2] == 'T' && magic[3] == 'H') {

            } else {
                buffer.seek(0);
            }

            info->version = IsoLot::readInt(in);
            if (info->version < IsoLot::VERSION0 || info->version > IsoLot::VERSION_LATEST) {
                delete info;
                continue;
            }
            int tilecount = IsoLot::readInt(in);

            for (int n = 0; n < tilecount; ++n) {
                QString str = IsoLot::readString(in);
                str = str.trimmed();
                info->tilesUsed += str;

                QString tilesetName;
                int tileID;
                if (BuildingEditor::BuildingTilesMgr::parseTileName(str, tilesetName, tileID)) {
                    info->buildingTiles += BuildingEditor::BuildingTile(tilesetName, tileID);
                } else {
                    info->buildingTiles += BuildingEditor::BuildingTile(str, -1);
                }
            }

            if (info->version == LotHeader::VERSION0) {
                quint8 alwaysZero = IsoLot::readByte(in);
            }

            info->width = IsoLot::readInt(in);
            info->height = IsoLot::readInt(in);
            Q_ASSERT(info->width == SQUARES_PER_CHUNK);
            Q_ASSERT(info->height == SQUARES_PER_CHUNK);

            if (info->version == LotHeader::VERSION0) {
                info->minLevel = 0;
                info->maxLevel = IsoLot::readInt(in) - 1; // Was always 15 but only data for levels 0-14
                Q_ASSERT(info->maxLevel == 14);
            } else {
                info->minLevel = IsoLot::readInt(in);
                info->maxLevel = IsoLot::readInt(in);
            }

            int numRooms = IsoLot::readInt(in);

            for (int n = 0; n < numRooms; ++n) {
                QString str = IsoLot::readString(in);
                RoomDef *def = new RoomDef(n, str);
                def->level = IsoLot::readInt(in);

                int rects = IsoLot::readInt(in);
                for (int rc = 0; rc < rects; ++rc) {
                    int x = IsoLot::readInt(in);
                    int y = IsoLot::readInt(in);
                    int w = IsoLot::readInt(in);
                    int h = IsoLot::readInt(in);
                    RoomRect *rect = new RoomRect(x + wX * SQUARES_PER_CELL,
                                                  y + wY * SQUARES_PER_CELL,
                                                  w, h);

                    def->rects += rect;
                }

                def->CalculateBounds();

                info->Rooms[def->ID] = def;
                def->CalculateBounds();
                int nObjects = IsoLot::readInt(in);
                for (int m = 0; m < nObjects; ++m) {
                    int e = IsoLot::readInt(in);
                    int x = IsoLot::readInt(in);
                    int y = IsoLot::readInt(in);
                    Q_UNUSED(e) Q_UNUSED(x) Q_UNUSED(y)
#if 0
                    def->objects += new MetaObject(e,
                                                   x + wX * 300 - def->x,
                                                   y + wY * 300 - def->y,
                                                   def);
#endif
                }

            }

            int numBuildings = IsoLot::readInt(in);

            for (int n = 0; n < numBuildings; ++n) {
                BuildingDef *def = new BuildingDef(n);
                int numbRooms = IsoLot::readInt(in);
                for (int x = 0; x < numbRooms; ++x) {
                    RoomDef *rr = info->Rooms[IsoLot::readInt(in)];
                    rr->building = def;
                    def->rooms += rr;
                }

                def->CalculateBounds();
                info->Buildings += def;

            }

            for (int x = 0; x < 30; ++x) {
                for (int y = 0; y < 30; ++y) {
                    int zombieDensity = IsoLot::readByte(in);
                    Q_UNUSED(zombieDensity)
//                    ch.getChunk(x, y).setZombieIntensity(zombieDensity);
                }
            }
            IsoLot::InfoHeaders[filenameheader] = info;
        }
    }
}

/////

/////

IsoWorld::IsoWorld(const QString &path, const IsoConstants &isoConstants) :
    isoConstants(isoConstants),
    MetaGrid(new IsoMetaGrid(isoConstants)),
    CurrentCell(nullptr),
    Directory(path)
{
}

IsoWorld::~IsoWorld()
{
    delete MetaGrid;
    delete CurrentCell;
}

void IsoWorld::init()
{
    MetaGrid->Create(Directory);
    CurrentCell = CellLoader::LoadCellBinaryChunk(this,
                                                  MetaGrid->cellBounds().center().x() * isoConstants.CHUNKS_PER_CELL,
                                                  MetaGrid->cellBounds().center().y() * isoConstants.CHUNKS_PER_CELL);
}

