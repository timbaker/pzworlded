#include "chunkdatafile.h"

#include "lotfilesmanager.h"
#include "mapcomposite.h"
#include "world.h"

#include "isochunk.h"
#include "isogridsquare.h"

#include <QDataStream>
#include <QFile>

using namespace Navigate;

ChunkDataFile::ChunkDataFile()
{
}

void ChunkDataFile::fromMap(int cellX, int cellY, MapComposite *mapComposite, const QList<LotFile::RoomRect *> &roomRects, const GenerateLotsSettings &settings)
{

    QString lotsDirectory = settings.exportDir;
    QFile file(lotsDirectory + QString::fromLatin1("/chunkdata_%1_%2.bin")
               .arg(settings.worldOrigin.x() + cellX).arg(settings.worldOrigin.y() + cellY));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QDataStream out(&file); // BigEndian by default, Java DataInputStream also BigEndian
//    out.setByteOrder(QDataStream::LittleEndian);

    int FILE_VERSION = 1;
    out << qint16(FILE_VERSION);

    int BIT_SOLID = 1 << 0;
    int BIT_WALLN = 1 << 1;
    int BIT_WALLW = 1 << 2;
    int BIT_WATER = 1 << 3;
    int BIT_ROOM = 1 << 4;

    int EMPTY_CHUNK = 0;
    int SOLID_CHUNK = 1;
    int REGULAR_CHUNK = 2;
    int WATER_CHUNK = 3;
    int ROOM_CHUNK = 4;

    quint8 *bitsArray = new quint8[IsoChunk::WIDTH * IsoChunk::WIDTH];

    for (int yy = 0; yy < CHUNKS_PER_CELL; yy++) {
        for (int xx = 0; xx < CHUNKS_PER_CELL; xx++) {
            IsoChunk *chunk = new IsoChunk(xx, yy, mapComposite, roomRects);
            int empty = 0, solid = 0, water = 0, room = 0;
            for (int y = 0; y < IsoChunk::WIDTH; y++) {
                for (int x = 0; x < IsoChunk::WIDTH; x++) {
                    IsoGridSquare *sq = chunk->getGridSquare(x, y, 0);
                    quint8 bits = 0;
                    if (sq->isSolid())
                        bits |= BIT_SOLID;
                    if (sq->isBlockedNorth())
                        bits |= BIT_WALLN;
                    if (sq->isBlockedWest())
                        bits |= BIT_WALLW;
                    if (sq->isWater())
                        bits |= BIT_WATER;
                    if (sq->isRoom())
                        bits |= BIT_ROOM;
                    bitsArray[x + y * IsoChunk::WIDTH] = bits;
                    if (bits == 0)
                        empty++;
                    else if (bits == BIT_SOLID)
                        solid++;
                    else if (bits == BIT_WATER)
                        water++;
                    else if (bits == BIT_ROOM)
                        room++;
                }
            }
            delete chunk;
            if (empty == IsoChunk::WIDTH * IsoChunk::WIDTH)
                out << quint8(EMPTY_CHUNK);
            else if (solid == IsoChunk::WIDTH * IsoChunk::WIDTH)
                out << quint8(SOLID_CHUNK);
            else if (water == IsoChunk::WIDTH * IsoChunk::WIDTH)
                out << quint8(WATER_CHUNK);
            else if (room == IsoChunk::WIDTH * IsoChunk::WIDTH)
                out << quint8(ROOM_CHUNK);
            else {
                out << quint8(REGULAR_CHUNK);
                for (int i = 0; i < IsoChunk::WIDTH * IsoChunk::WIDTH; i++)
                    out << bitsArray[i];
            }
        }
    }

    delete[] bitsArray;

    file.close();
}
