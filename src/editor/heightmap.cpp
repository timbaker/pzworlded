/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "heightmap.h"

#include "heightmapfile.h"

#include <QDebug>
#include <QRegion>
#include <QVector>

class HeightMapGrid
{
public:
    HeightMapGrid(HeightMapFile *hmFile, int gridSize);
    ~HeightMapGrid();

    void update();
    void LoadChunk(int wx, int wy, int x, int y);
    void LoadChunkForLater(int wx, int wy, int x, int y);

    HeightMapChunk *getChunkForGridSquare(int x, int y);
    HeightMapChunk *getChunk(int x, int y);
    void setChunk(int x, int y, HeightMapChunk *c);

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

    void setCenter(int x, int y);

    int getWidthInTiles() const { return CellSize; }

    int getWorldXMinTiles();
    int getWorldYMinTiles();

//    static const int ChunkDiv = 10;
    static const int TilesPerChunk = 50; // Must match HeightMapFile::chunkDim()
    int CellSize; // Columns/Rows of tiles displayed.
    int ChunkGridWidth; // Columns/Rows of chunks displayed.

    HeightMapFile *mFile;

    int WorldX;
    int WorldY;

    QVector<QVector<HeightMapChunk*> > Chunks;

    int XMinTiles;
    int YMinTiles;
};

/////

HeightMapGrid::HeightMapGrid(HeightMapFile *hmFile, int gridSize) :
    mFile(hmFile),
    CellSize(gridSize * 50),
    ChunkGridWidth(gridSize),
    WorldX(-1),
    WorldY(-1),
    XMinTiles(-1),
    YMinTiles(-1)
{
    Chunks.resize(ChunkGridWidth);
    for (int x = 0; x < ChunkGridWidth; x++)
        Chunks[x].resize(ChunkGridWidth);
}

HeightMapGrid::~HeightMapGrid()
{
    for (int x = 0; x < Chunks.size(); x++) {
        foreach (HeightMapChunk *chunk, Chunks[x]) {
            if (chunk)
                mFile->releaseChunk(chunk);
        }
    }
}

void HeightMapGrid::update()
{
}

void HeightMapGrid::LoadChunk(int wx, int wy, int x, int y)
{
    HeightMapChunk *chunk = mFile->requestChunk(wx * TilesPerChunk, wy * TilesPerChunk);
    setChunk(x, y, chunk);
}

HeightMapChunk *HeightMapGrid::getChunkForGridSquare(int x, int y)
{
    x -= getWorldXMinTiles();
    y -= getWorldYMinTiles();

    if ((x < 0) || (y < 0) || (x >= CellSize) || (y >= CellSize)) {
        return 0;
    }
    HeightMapChunk *c = getChunk(x / TilesPerChunk, y / TilesPerChunk);

    return c;
}

#if 0
void HeightMapGrid::setGridSquare(IsoGridSquare *square, int wx, int wy, int x, int y, int z)
{
    x -= wx * ChunksPerWidth;
    y -= wy * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels))
        return;
    HeightMapChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return;
    c->setSquare(x % ChunksPerWidth, y % ChunksPerWidth, z, square);
}

void HeightMapGrid::setGridSquare(IsoGridSquare *square, int x, int y, int z) {
    x -= getWorldXMin() * ChunksPerWidth;
    y -= getWorldYMin() * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels)) {
        return;
    }
    HeightMapChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return;
    c->setSquare(x % ChunksPerWidth, y % ChunksPerWidth, z, square);
}

IsoGridSquare *HeightMapGrid::getGridSquare(int x, int y, int z)
{
    x -= getWorldXMin() * ChunksPerWidth;
    y -= getWorldYMin() * ChunksPerWidth;

    if ((x < 0) || (y < 0) || (x >= 300) || (y >= 300) || (z < 0) || (z > MaxLevels)) {
        return 0;
    }
    HeightMapChunk *c = getChunk(x / ChunksPerWidth, y / ChunksPerWidth);
    if (c == 0)
        return 0;
    return c->getGridSquare(x % ChunksPerWidth, y % ChunksPerWidth, z);
}
#endif

HeightMapChunk *HeightMapGrid::getChunk(int x, int y)
{
    if ((x < 0) || (x >= ChunkGridWidth) || (y < 0) || (y >= ChunkGridWidth)) {
        return 0;
    }
    return Chunks[x][y];
}

void HeightMapGrid::setChunk(int x, int y, HeightMapChunk *c)
{
    if (x < 0 || x >= ChunkGridWidth || y < 0 || y >= ChunkGridWidth) return;
    if (c && Chunks[x][y]) {
        int i = 0;
    }

    Q_ASSERT(!c || (Chunks[x][y] == 0));
    Chunks[x][y] = c;
}

void HeightMapGrid::UpdateCellCache()
{
#if 0
    int i = getWidthInTiles();
    for (int x = 0; x < i; ++x) {
        for (int y = 0; y < i; ++y) {
            for (int z = 0; z < MaxLevels; ++z) {
                IsoGridSquare *sq = getGridSquare(x + getWorldXMinTiles(), y + getWorldYMinTiles(), z);
                mFile->setCacheGridSquareLocal(x, y, z, sq);
            }
        }
    }
#endif
}

int HeightMapGrid::getWorldXMin()
{
    return (WorldX - (ChunkGridWidth / 2));
}

int HeightMapGrid::getWorldYMin()
{
    return (WorldY - (ChunkGridWidth / 2));
}

int HeightMapGrid::getWorldXMinTiles()
{
    if (XMinTiles != -1) return XMinTiles;
    XMinTiles = (getWorldXMin() * TilesPerChunk);
    return XMinTiles;
}

int HeightMapGrid::getWorldYMinTiles()
{
    if (YMinTiles != -1) return YMinTiles;
    YMinTiles = (getWorldYMin() * TilesPerChunk);
    return YMinTiles;
}

void HeightMapGrid::setCenter(int x, int y)
{
    int wx = x / TilesPerChunk;
    int wy = y / TilesPerChunk;
    wx = qBound(ChunkGridWidth / 2, wx, (mFile->width()) / TilesPerChunk - ChunkGridWidth / 2);
    wy = qBound(ChunkGridWidth / 2, wy, (mFile->height()) / TilesPerChunk - ChunkGridWidth / 2);

    if (wx != WorldX || wy != WorldY) {
        qDebug() << "HeightMapGrid::setCenter" << wx << wy;
        QRegion current = QRect(getWorldXMin(), getWorldYMin(),
                                ChunkGridWidth, ChunkGridWidth);
        if (WorldX == -1) current = QRegion(); // initial state
        QRegion updated = QRect(wx - ChunkGridWidth / 2, wy - ChunkGridWidth / 2,
                                ChunkGridWidth, ChunkGridWidth);

        // Discard old chunks.
        foreach (QRect r, (current - updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (HeightMapChunk *c = getChunk(x - getWorldXMin(), y - getWorldYMin())) {
                        setChunk(x - getWorldXMin(), y - getWorldYMin(), 0);
                        mFile->releaseChunk(c);
                    }
                }
            }
        }

        // Shift preserved chunks.
        QVector<HeightMapChunk*> preserved;
        foreach (QRect r, (current & updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (HeightMapChunk *c = getChunk(x - getWorldXMin(), y - getWorldYMin())) {
                        setChunk(x - getWorldXMin(), y - getWorldYMin(), 0);
                        preserved += c;
                    }
                }
            }
        }

        WorldX = wx;
        WorldY = wy;
        XMinTiles = YMinTiles = -1;

        foreach (HeightMapChunk *c, preserved) {
            setChunk(c->x() / TilesPerChunk - getWorldXMin(), c->y() / TilesPerChunk - getWorldYMin(), c);
        }

        // Load new chunks;
        foreach (QRect r, (updated - current).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
//                    qDebug() << "Load chunk" << x << "," << y;
                    LoadChunk(x, y, x - getWorldXMin(), y - getWorldYMin());
                }
            }
        }

//            UpdateCellCache();
    }
}

/////

class HeightMapPrivate
{
public:
    HeightMapPrivate(HeightMapFile *hmFile, int gridSize) :
        g(hmFile, gridSize)
    {}

    HeightMapGrid g;
};

/////

HeightMap::HeightMap(HeightMapFile *hmFile, int gridSize) :
    mFile(hmFile),
    d(new HeightMapPrivate(mFile, gridSize))
{
}

HeightMap::~HeightMap()
{
    delete d;
}

int HeightMap::width() const
{
    return mFile->width(); // #tiles in world
}

int HeightMap::height() const
{
    return mFile->height(); // #tiles in world
}

void HeightMap::setHeightAt(const QPoint &p, int h)
{
    return setHeightAt(p.x(), p.y(), h);
}

void HeightMap::setHeightAt(int x, int y, int h)
{
    if (HeightMapChunk *c = d->g.getChunkForGridSquare(x, y))
        c->setHeightAt(x, y, h);
}

int HeightMap::heightAt(const QPoint &p)
{
    return heightAt(p.x(), p.y());
}

int HeightMap::heightAt(int x, int y)
{
    if (HeightMapChunk *c = d->g.getChunkForGridSquare(x, y))
        return c->heightAt(x, y);
    return 0;
}

void HeightMap::setCenter(int x, int y)
{
    d->g.setCenter(x, y);
}

