#include "chunkmap.h"

#include <qmath.h>
#include <QBuffer>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QFile>

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QVector<QVector<int> >;
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

IsoChunk::IsoChunk(IsoCell *cell) :
    wx(0),
    wy(0),
    Cell(cell)
{
    squares.resize(IsoChunkMap::ChunksPerWidth);
    for (int x = 0; x < squares.size(); x++) {
        squares[x].resize(IsoChunkMap::ChunksPerWidth);
        for (int y = 0; y < squares[x].size(); y++)
            squares[x][y].resize(IsoChunkMap::MaxLevels);
    }
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

void IsoChunk::setSquare(int x, int y, int z, IsoGridSquare *square)
{
    squares[x][y][z] = square;
    if (square)
        square->chunk = this;
}

IsoGridSquare *IsoChunk::getGridSquare(int x, int y, int z)
{
    if ((z >= IsoChunkMap::MaxLevels) || (z < 0)) {
        return 0;
    }
    return squares[x][y][z];
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
    for (int x = 0; x < squares.size(); x++) {
        for (int y = 0; y < squares[x].size(); y++) {
            for (int z = 0; z < squares[x][y].size(); z++) {
                IsoGridSquare *sq = squares[x][y][z];
                if (sq == 0)
                    continue;
                squares[x][y][z] = 0;
                sq->discard();
            }
        }
    }
}

void IsoChunk::Save(bool bSaveQuit)
{
    // save out to a .bin file here

    reuseGridsquares();
}

/////

IsoChunkMap::IsoChunkMap(IsoCell *cell) :
    cell(cell),
    WorldX(-1000),
    WorldY(-1000),
    XMinTiles(-1),
    XMaxTiles(-1),
    YMinTiles(-1),
    YMaxTiles(-1)
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
    x -= getWorldXMin() * ChunksPerWidth;
    y -= getWorldYMin() * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300)) {
        return 0;
    }
    IsoChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);

    return c;
}

void IsoChunkMap::setGridSquare(IsoGridSquare *square, int wx, int wy, int x, int y, int z)
{
    x -= wx * ChunksPerWidth;
    y -= wy * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels))
        return;
    IsoChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return;
    c->setSquare(x % ChunksPerWidth, y % ChunksPerWidth, z, square);
}

void IsoChunkMap::setGridSquare(IsoGridSquare *square, int x, int y, int z) {
    x -= getWorldXMin() * ChunksPerWidth;
    y -= getWorldYMin() * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels)) {
        return;
    }
    IsoChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return;
    c->setSquare(x % ChunksPerWidth, y % ChunksPerWidth, z, square);
}

IsoGridSquare *IsoChunkMap::getGridSquare(int x, int y, int z)
{
    x -= getWorldXMin() * ChunksPerWidth;
    y -= getWorldYMin() * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels)) {
        return 0;
    }
    IsoChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return 0;
    return c->getGridSquare(x % ChunksPerWidth, y % ChunksPerWidth, z);
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
            for (int z = 0; z < MaxLevels; ++z) {
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
    if (XMinTiles != -1) return XMinTiles;
    XMinTiles = (getWorldXMin() * ChunksPerWidth);
    return XMinTiles;
}

int IsoChunkMap::getWorldYMinTiles()
{
    if (YMinTiles != -1) return YMinTiles;
    YMinTiles = (getWorldYMin() * ChunksPerWidth);
    return YMinTiles;
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

    float fwx = wX;
    float fwy = wY;
    fwx /= IsoChunkMap::ChunkGridWidth;
    fwy /= IsoChunkMap::ChunkGridWidth;
    wX = qFloor(fwx);
    wY = qFloor(fwy);

    QString filenameheader = QString::fromLatin1("%1/%2_%3.lotheader").arg(directory).arg(wX).arg(wY);
    Q_ASSERT(InfoHeaders.contains(filenameheader));
    info = InfoHeaders[filenameheader];

    ch->lotheader = info;

    data.resize(IsoChunkMap::ChunksPerWidth);
    for (int x = 0; x < data.size(); x++) {
        data[x].resize(IsoChunkMap::ChunksPerWidth);
        for (int y = 0; y < data[x].size(); y++)
            data[x][y].resize(info->levels);
    }

    roomIDs.resize(IsoChunkMap::ChunksPerWidth);
    for (int x = 0; x < data.size(); x++) {
        roomIDs[x].resize(IsoChunkMap::ChunksPerWidth);
        for (int y = 0; y < data[x].size(); y++)
            roomIDs[x][y].fill(-1, info->levels);
    }

    {
        QString filenamepack = QString::fromLatin1("%1/world_%2_%3.lotpack").arg(directory).arg(wX).arg(wY);
        QBuffer *fo = CellLoader::instance()->openLotPackFile(filenamepack);
        if (!fo)
            return; // exception!

        QDataStream in(fo);
        in.setByteOrder(QDataStream::LittleEndian);

//        qDebug() << "reading chunk" << wX << wY << "from" << filenamepack;

        int skip = 0;

        int lwx = this->wx - (wX * IsoChunkMap::ChunkGridWidth);
        int lwy = this->wy - (wY * IsoChunkMap::ChunkGridWidth);
        int index = lwx * IsoChunkMap::ChunkGridWidth + lwy;
        fo->seek(4 + index * 8);
        qint64 pos;
        in >> pos;
        fo->seek(pos);
        for (int z = 0; z < info->levels; ++z) {
            for (int x = 0; x < IsoChunkMap::ChunksPerWidth; ++x) {
                for (int y = 0; y < IsoChunkMap::ChunksPerWidth; ++y) {
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
#if 1
                            int room = info->getRoomAt(x, y, z);
#else
                            int room = readInt(in);
#endif
                            roomIDs[x][y][z] = room;

                            int height = readByte(in);
                            Q_UNUSED(height)

                            if (count <= 1) {
                                int brk = 1;
                            }

                            Q_ASSERT(count > 1 && count < 30);

                            for (int n = 1; n < count; ++n) {
                                int d = readInt(in);

                                this->data[x][y][z] += d;
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
    int WX = wx / (cell->width / IsoChunkMap::ChunksPerWidth);
    int WY = wy / (cell->height / IsoChunkMap::ChunksPerWidth);

    IsoLot *lot = new IsoLot(cell->World->Directory, WX, WY, wx, wy, chunk);

    cell->PlaceLot(lot, 0, 0, 0, chunk, wx, wy, false);

    delete lot;
}

void CellLoader::LoadCellBinaryChunkForLater(IsoCell *cell, int wx, int wy, IsoChunk *chunk)
{
    int WX = wx / (cell->width / IsoChunkMap::ChunksPerWidth);
    int WY = wy / (cell->height / IsoChunkMap::ChunksPerWidth);

    IsoLot *lot = new IsoLot(cell->World->Directory, WX, WY, wx, wy, chunk);

    cell->PlaceLot(lot, 0, 0, 0, chunk, wx, wy, true);

    delete lot;
}

IsoCell *CellLoader::LoadCellBinaryChunk(IsoWorld *world, int wx, int wy)
{
    IsoCell *cell = new IsoCell(world/*spr*/, 300, 300);

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

int IsoCell::MaxHeight = -1; // FIXME: why static?

IsoCell::IsoCell(IsoWorld *world, /*IsoSpriteManager &spr, */int width, int height) :
    width(width),
    height(height),
    ChunkMap(new IsoChunkMap(this)),
    World(world)
{
    gridSquares.resize(IsoChunkMap::CellSize);
    for (int x = 0; x < IsoChunkMap::CellSize; x++) {
        gridSquares[x].resize(IsoChunkMap::CellSize);
        for (int y = 0; y < IsoChunkMap::CellSize; y++)
            gridSquares[x][y].resize(IsoChunkMap::MaxLevels);
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
    int NewMaxHeight = IsoChunkMap::MaxLevels - 1;
    WX *= IsoChunkMap::ChunksPerWidth;
    WY *= IsoChunkMap::ChunksPerWidth;

      /* try */
    {
        for (int x = WX + sx; x < WX + sx + IsoChunkMap::ChunksPerWidth; ++x) {
            for (int y = WY + sy; y < WY + sy + IsoChunkMap::ChunksPerWidth; ++y) {
                bool bDoIt = true;
                for (int z = sz; z < sz + lot->info->levels; ++z) {
                    bDoIt = false;

                    if ((x >= WX + IsoChunkMap::ChunksPerWidth) || (y >= WY + IsoChunkMap::ChunksPerWidth) || (x < WX) || (y < WY)) continue;
                    if (z < 0)
                        continue;

                    QList<int> ints = lot->data[(x - (WX + sx))][(y - (WY + sy))][(z - sz)];
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
                                    if (xx + x - WX < IsoChunkMap::ChunksPerWidth) {
                                        if (yy + y - WY < 0) continue;
                                        if (yy + y - WY >= IsoChunkMap::ChunksPerWidth)
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
                        int roomID = lot->roomIDs[x - WX][y - WY][z];
#else
                        int roomID = ch->lotheader->getRoomAt(x, y, z);
#endif
                        square->setRoomID(roomID);
                    }

#if 1 // BUG IN LEMMY'S???
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
#if 0
      catch (Exception ex)
      {
          System.out.println("Failed to load chunk, blocking out area");
          ex.printStackTrace();
          for (int x = WX + sx; x < WX + sx + IsoChunkMap.ChunksPerWidth; ++x) {
              for (int y = WY + sy; y < WY + sy + IsoChunkMap.ChunksPerWidth; ++y) {
                  for (int z = sz; z < sz + lot.info.levels; ++z)
                  {
                      ch.setSquare(x - WX, y - WY, z, null);

                      setCacheGridSquare(x, y, z, null);
                  }
              }
          }
      }
#endif

      if (bForLater)
          return;

      for (int x = sx + WX - 1; x < sx + WX + lot->info->width + 1; ++x)
          for (int y = sy + WY - 1; y < sy + WY + lot->info->height + 1; ++y)
              for (int z = sz; z < sz + lot->info->levels; ++z) {
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
    if ((z >= IsoChunkMap::MaxLevels) || (z < 0)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
        return;
    gridSquares[x][y][z] = square;
}

void IsoCell::setCacheGridSquare(int wx, int wy, int x, int y, int z, IsoGridSquare *square)
{
    x -= wx * IsoChunkMap::ChunksPerWidth;
    y -= wy * IsoChunkMap::ChunksPerWidth;
    if ((z >= IsoChunkMap::MaxLevels) || (z < 0)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
        return;
//    if (square) qDebug() << "IsoCell::setCacheGridSquare" << x << y << z << square->tiles;
    gridSquares[x][y][z] = square;
}

void IsoCell::setCacheGridSquareLocal(int x, int y, int z, IsoGridSquare *square) {
    if ((z >= IsoChunkMap::MaxLevels) || (z < 0)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
        return;
//    if (square) qDebug() << "IsoCell::setCacheGridSquare" << x << y << z << square->tiles;
    gridSquares[x][y][z] = square;
}

IsoGridSquare *IsoCell::getGridSquare(int x, int y, int z)
{
    if (ChunkMap->XMinTiles == -1) {
        ChunkMap->getWorldXMinTiles();
        ChunkMap->getWorldYMinTiles();
    }
    x -= ChunkMap->XMinTiles;
    y -= ChunkMap->YMinTiles;
    if ((z >= IsoChunkMap::MaxLevels) || (z < 0)
            || (x < 0) || (x >= ChunkMap->getWidthInTiles())
            || (y < 0) || (y >= ChunkMap->getWidthInTiles()))
        return 0;
    return gridSquares[x][y][z];
}

/////

IsoMetaGrid::IsoMetaGrid() :
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

            QDataStream in(&fo);
            in.setByteOrder(QDataStream::LittleEndian);

            info->version = IsoLot::readInt(in);
            int tilecount = IsoLot::readInt(in);

            for (int n = 0; n < tilecount; ++n) {
                QString str = IsoLot::readString(in);
                info->tilesUsed += str.trimmed();
            }

            IsoLot::readByte(in);

            info->width = IsoLot::readInt(in);
            info->height = IsoLot::readInt(in);
            info->levels = IsoLot::readInt(in);

            Q_ASSERT(info->width == IsoChunkMap::ChunksPerWidth);
            Q_ASSERT(info->height == IsoChunkMap::ChunksPerWidth);
            Q_ASSERT(info->levels == 15);

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
                    RoomRect *rect = new RoomRect(x + wX * 300,
                                                  y + wY * 300,
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

IsoWorld::IsoWorld(const QString &path) :
    MetaGrid(new IsoMetaGrid),
    CurrentCell(0),
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
                                                  MetaGrid->cellBounds().center().x() * IsoChunkMap::ChunkGridWidth,
                                                  MetaGrid->cellBounds().center().y() * IsoChunkMap::ChunkGridWidth);
}

