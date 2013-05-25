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

#ifndef WORLDCHUNKMAP_H
#define WORLDCHUNKMAP_H

#include <QList>
#include <QRectF>
#include <QRegion>
#include <QVector>

#include "global.h"

class PathDocument;
class PathWorld;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;
class WorldTile;

class WorldChunk;
class WorldChunkMap;

// A map location on a level.  Has a list of tiles per layer on that level.
class WorldChunkSquare
{
public:
    WorldChunkSquare(int x, int y, int z);

    static WorldChunkSquare *getNew(int x, int y, int z);

    void discard();

//    void RecalcAllWithNeighbours(bool bDoReverse);

    void setX(int x) { this->x = x; }
    void setY(int y) { this->y = y; }
    void setZ(int z) { this->z = z; }

//    void setRoomID(int roomID);

//    int roomID;
    int ID;
    int x;
    int y;
    int z;

    QVector<WorldTile*> tiles;

    WorldChunk *chunk;
//    IsoRoom *room;

    static int IDMax;
    static QList<WorldChunkSquare*> WorldChunkSquareCache;
};

#include <QHash>
class SparseSquareGrid
{
public:
    SparseSquareGrid(int width, int height)
        : mWidth(width)
        , mHeight(height)
        , mUseVector(false)
        , mEmptySquare(0)
    {
    }

    int size() const
    { return mWidth * mHeight; }

    const WorldChunkSquare *at(int index) const
    {
        if (mUseVector)
            return mVector[index];
        HashType::const_iterator it = mHash.find(index);
        if (it != mHash.end())
            return *it;
        return mEmptySquare;
    }

    const WorldChunkSquare *at(int x, int y) const
    {
        return at(y * mWidth + x);
    }

    void replace(int index, const WorldChunkSquare *sq)
    {
        if (mUseVector) {
            mVector[index] = sq;
            return;
        }
        HashType::iterator it = mHash.find(index);
        if (it == mHash.end()) {
            if (sq == mEmptySquare)
                return;
            mHash.insert(index, sq);
        } else if (sq != mEmptySquare)
            (*it) = sq;
        else
            mHash.erase(it);
        if (mHash.size() > 300 * 300 / 3)
            swapToVector();
    }

    void replace(int x, int y, WorldChunkSquare *sq)
    {
        int index = y * mWidth + x;
        replace(index, sq);
    }

    bool isEmpty() const
    { return !mUseVector && mHash.isEmpty(); }

    void clear()
    {
        if (mUseVector)
            mVector.fill(mEmptySquare);
        else
            mHash.clear();
    }

    void beginIterate()
    {
        mIterator = mHash.begin();
        mIteratorEnd = mHash.end();
        mIteratorIndex = 0;
    }

    WorldChunkSquare *nextIterate()
    {
        if (mUseVector) {
            if (mIteratorIndex >= mVector.size())
                return mEmptySquare;
            return (WorldChunkSquare*) at(mIteratorIndex++);
        } else {
            if (mIterator == mIteratorEnd)
                return mEmptySquare;
            return (WorldChunkSquare*) *mIterator++;
        }
        return mEmptySquare;
    }

private:
    void swapToVector()
    {
        Q_ASSERT(!mUseVector);
        mVector.resize(size());
        HashType::const_iterator it = mHash.begin();
        while (it != mHash.end()) {
            mVector[it.key()] = (*it);
            ++it;
        }
        mHash.clear();
        mUseVector = true;
    }

    int mWidth, mHeight;
    typedef QHash<int,const WorldChunkSquare*> HashType;
    HashType mHash;
    QVector<const WorldChunkSquare*> mVector;
    bool mUseVector;
    WorldChunkSquare *mEmptySquare;

    HashType::iterator mIterator;
    HashType::iterator mIteratorEnd;
    int mIteratorIndex;
};

// An XxY piece of the world.
class WorldChunk
{
public:
    WorldChunk(WorldChunkMap *chunkMap);

    void LoadForLater(int wx, int wy);

    void recalcNeighboursNow();
    void update();
    void RecalcSquaresTick();

    void setSquare(int x, int y, int z, WorldChunkSquare *square);
    WorldChunkSquare *getGridSquare(int x, int y, int z);
//    IsoRoom *getRoom(int roomID);
    void ClearGridsquares();
    void reuseGridsquares();

    void Save(bool bSaveQuit);

    static WorldChunk *getNew(WorldChunkMap *chunkMap);
    void discard();

    QVector<SparseSquareGrid*> squares;
//    LotHeader *lotheader;
    int wx;
    int wy;

    WorldChunkMap *mChunkMap;
//    IsoCell *Cell;

    static QList<WorldChunk*> WorldChunkCache;
};

class WorldChunkMap : public QObject
{
    Q_OBJECT
public:
    WorldChunkMap(PathDocument *doc);

    void LoadChunkForLater(int wx, int wy, int x, int y);

    WorldChunkSquare *getGridSquare(int x, int y, int z);
    WorldChunk *getChunkForWorldPos(int x, int y);
    WorldChunk *getChunk(int x, int y);
    void setChunk(int x, int y, WorldChunk *c);

    void UpdateCellCache();

    int getWorldXMin();
    int getWorldYMin();

    int getWidthInTiles() const { return CellSize; }

    int getWorldXMinTiles();
    int getWorldYMinTiles();

    void setCenter(int x, int y);

signals:
    void chunksUpdated();

public slots:
    void nodeMoved(WorldNode *node, const QPointF &prev);
    void afterRemoveNode(WorldPathLayer *laye, int index, WorldNode *node);

    void afterAddScriptToPath(WorldPath *path, int index, WorldScript *script);
    void afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script);
    void afterChangeScriptParameters(WorldScript *script);

    void afterAddPath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path);

    void afterAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);

    void nodesMoved();
    void processScriptRegionChanges();

public:
    //    static const int ChunkDiv = 10;
    static const int TilesPerChunk = 100; // Number of tiles per chunk.
    static const int ChunkGridWidth = 6; // Columns/Rows of chunks displayed.
    static const int CellSize = TilesPerChunk * ChunkGridWidth; // Columns/Rows of tiles displayed.
    static const int MaxLevels = 16;

    PathDocument *mDocument;
    PathWorld *mWorld;

//    IsoCell *cell;

    int WorldCellX;
    int WorldCellY;
    int WorldX;
    int WorldY;

    QVector<QVector<WorldChunk*> > Chunks;

    int XMinTiles;
    int XMaxTiles;
    int YMinTiles;
    int YMaxTiles;

private:
    QRectF mMovedNodeArea;
    QRegion mScriptChangeArea;
    ScriptSet mScriptsThatChanged;
};

#endif // WORLDCHUNKMAP_H
