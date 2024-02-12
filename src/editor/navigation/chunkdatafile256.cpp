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

#include "chunkdatafile256.h"

#include "mapcomposite.h"
#include "world.h"

#include "isochunk256.h"
#include "isogridsquare256.h"

#include <QDataStream>
#include <QFile>

using namespace Navigate;

ChunkDataFile256::ChunkDataFile256()
{

}

void ChunkDataFile256::fromMap(CombinedCellMaps &combinedMaps, MapComposite *mapComposite, const LotFile::RectLookup<LotFile::RoomRect> &roomRectLookup, const GenerateLotsSettings &settings)
{
    QString lotsDirectory = settings.exportDir;
    QFile file(lotsDirectory + QString::fromLatin1("/chunkdata_%1_%2.bin")
               .arg(combinedMaps.mCell256X).arg(combinedMaps.mCell256Y));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QDataStream out(&file); // BigEndian by default, Java DataInputStream also BigEndian

    int FILE_VERSION = 1;
    out << qint16(FILE_VERSION);

    int BIT_SOLID = 1 << 0;
    int BIT_WALLN = 1 << 1;
    int BIT_WALLW = 1 << 2;
    int BIT_WATER = 1 << 3;
    int BIT_ROOM = 1 << 4;
    int BIT_NULL = 1 << 5;

    int EMPTY_CHUNK = 0;
    int SOLID_CHUNK = 1;
    int REGULAR_CHUNK = 2;
    int WATER_CHUNK = 3;
    int ROOM_CHUNK = 4;
    int NULL_CHUNK = 5;

    quint8 *bitsArray = new quint8[CHUNK_SIZE_256 * CHUNK_SIZE_256];

    int cellMinX256 = combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH;
    int cellMinY256 = combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT;

    for (int yy = 0; yy < CHUNKS_PER_CELL_256; yy++) {
        for (int xx = 0; xx < CHUNKS_PER_CELL_256; xx++) {
            QRect chunkRect(cellMinX256 + xx * CHUNK_SIZE_256, cellMinY256 + yy * CHUNK_SIZE_256, CHUNK_SIZE_256, CHUNK_SIZE_256);
            QList<LotFile::RoomRect*> roomRects;
            roomRectLookup.overlapping(QRect(xx * CHUNK_SIZE_256, yy * CHUNK_SIZE_256, CHUNK_SIZE_256, CHUNK_SIZE_256), roomRects);
            IsoChunk256 *chunk = new IsoChunk256(xx, yy, chunkRect.x(), chunkRect.y(), mapComposite, roomRects);
            int empty = 0, solid = 0, water = 0, room = 0, null = 0;
            for (int y = 0; y < CHUNK_SIZE_256; y++) {
                for (int x = 0; x < CHUNK_SIZE_256; x++) {
                    IsoGridSquare256 *sq = chunk->getGridSquare(x, y, 0);
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
                    if (isPositionNull(mapComposite, chunkRect.x() + x, chunkRect.y() + y))
                        bits |= BIT_NULL;
                    bitsArray[x + y * CHUNK_SIZE_256] = bits;
                    if (bits == 0)
                        empty++;
                    else if (bits == BIT_SOLID)
                        solid++;
                    else if (bits == BIT_WATER)
                        water++;
                    else if (bits == BIT_ROOM)
                        room++;
                    else if (bits == BIT_NULL)
                        null++;

                }
            }
            delete chunk;
            if (empty == CHUNK_SIZE_256 * CHUNK_SIZE_256)
                out << quint8(EMPTY_CHUNK);
            else if (solid == CHUNK_SIZE_256 * CHUNK_SIZE_256)
                out << quint8(SOLID_CHUNK);
            else if (water == CHUNK_SIZE_256 * CHUNK_SIZE_256)
                out << quint8(WATER_CHUNK);
            else if (room == CHUNK_SIZE_256 * CHUNK_SIZE_256)
                out << quint8(ROOM_CHUNK);
            else if (null == CHUNK_SIZE_256 * CHUNK_SIZE_256)
                out << quint8(NULL_CHUNK);
            else {
                out << quint8(REGULAR_CHUNK);
                for (int i = 0; i < CHUNK_SIZE_256 * CHUNK_SIZE_256; i++) {
                    out << bitsArray[i];
                }
            }
        }
    }

    delete[] bitsArray;

    file.close();
}

bool ChunkDataFile256::isPositionNull(MapComposite *mapComposite, int squareX, int squareY)
{
    QVector<const Tiled::Cell*> cells;
    for (int z = mapComposite->minLevel(); z <= mapComposite->maxLevel(); z++) {
        CompositeLayerGroup *layerGroup = mapComposite->layerGroupForLevel(z);
        layerGroup->prepareDrawing2();
        cells.resize(0);
        layerGroup->orderedCellsAt2(QPoint(squareX, squareY), cells);
        if (cells.isEmpty() == false) {
            return false;
        }
    }
    return true;
}
