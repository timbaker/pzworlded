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

#ifndef ISOCHUNK256_H
#define ISOCHUNK256_H

#include "lotfilesmanager256.h"

namespace Tiled {
class Cell;
}

class MapComposite;

namespace Navigate
{
class IsoGridSquare256;

class IsoChunk256
{
public:
    IsoChunk256(int xInCell, int yInCell, int minSquareX, int minSquareY, MapComposite *mapComposite, const QList<LotFile::RoomRect*> &roomRects);
    ~IsoChunk256();

    IsoGridSquare256 *getGridSquare(int x, int y, int z);
    IsoGridSquare256 *getGridSquareWorldPos(int x, int y, int z);
    bool contains(int x, int y, int z);
    bool containsWorldPos(int x, int y, int z);
    int worldXMin() { return mMinSquareX; }
    int worldYMin() { return mMinSquareY; }
    int worldXMax() { return mMinSquareX + CHUNK_SIZE_256; }
    int worldYMax() { return mMinSquareY + CHUNK_SIZE_256; }

    void orderedCellsAt(int x, int y, QVector<const Tiled::Cell *> &cells);

    int mMinSquareX;
    int mMinSquareY;
    IsoGridSquare256 *squares[CHUNK_SIZE_256][CHUNK_SIZE_256];

    MapComposite *mMapComposite;
};

} // namespace Navigate

#endif // ISOCHUNK256_H
