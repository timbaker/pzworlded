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

#include "isochunk256.h"

#include "mapcomposite.h"

#include "isogridsquare256.h"

using namespace Navigate;

IsoChunk256::IsoChunk256(int wx, int wy, MapComposite *mapComposite, const QList<LotFile::RoomRect *> &roomRects) :
    wx(wx),
    wy(wy),
    mMapComposite(mapComposite)
{
    int z = 0;
    for (int y = 0; y < CHUNK_SIZE_256; y++) {
        for (int x = 0; x < CHUNK_SIZE_256; x++) {
            squares[x][y] = new IsoGridSquare256(wx * CHUNK_SIZE_256 + x, wy * CHUNK_SIZE_256 + y, z, this);
        }
    }

    QRect bounds(worldXMin(), worldYMin(), CHUNK_SIZE_256, CHUNK_SIZE_256);
    for (LotFile::RoomRect *rect : roomRects) {
        if (rect->bounds().intersects(bounds)) {
            for (int y = rect->y; y < rect->y + rect->h; y++) {
                for (int x = rect->x; x < rect->x + rect->w; x++) {
                    if (containsWorldPos(x, y, 0)) {
                        squares[x - worldXMin()][y - worldYMin()]->mRoom = true;
                    }
                }
            }
        }
    }
}

IsoChunk256::~IsoChunk256()
{
    for (int y = 0; y < CHUNK_SIZE_256; y++) {
        for (int x = 0; x < CHUNK_SIZE_256; x++) {
            delete squares[x][y];
        }
    }
}

IsoGridSquare256 *IsoChunk256::getGridSquare(int x, int y, int z)
{
    if (contains(x, y, z)) {
        return squares[x][y];
    }
    return NULL;
}

IsoGridSquare256 *IsoChunk256::getGridSquareWorldPos(int x, int y, int z)
{
    if (containsWorldPos(x, y, z)) {
        return squares[x - worldXMin()][y - worldYMin()];
    }
    return nullptr;
}

bool IsoChunk256::contains(int x, int y, int z)
{
    return x >= 0 && x < CHUNK_SIZE_256 && y >= 0 && y < CHUNK_SIZE_256;
}

bool IsoChunk256::containsWorldPos(int x, int y, int z)
{
    return x >= worldXMin() && x < worldXMax() && y >= worldYMin() && y < worldYMax();
}

void IsoChunk256::orderedCellsAt(int x, int y, QVector<const Tiled::Cell *> &cells)
{
    mMapComposite->layerGroupForLevel(0)->orderedCellsAt2(QPoint(x, y), cells);
}
