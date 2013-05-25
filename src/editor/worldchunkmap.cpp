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

#include "worldchunkmap.h"

#include "luaworlded.h"
#include "pathdocument.h"
#include "path.h"
#include "pathworld.h"
#include "worldchanger.h"
#include "worldlookup.h"

#include <QRect>
#include <QRegion>

int WorldChunkSquare::IDMax = -1;
QList<WorldChunkSquare*> WorldChunkSquare::WorldChunkSquareCache;

WorldChunkSquare::WorldChunkSquare(int x, int y, int z) :
    ID(++IDMax),
    x(x),
    y(y),
    z(z),
    tiles(1)
{
}

WorldChunkSquare *WorldChunkSquare::getNew(int x, int y, int z)
{
    if (WorldChunkSquareCache.isEmpty())
        return new WorldChunkSquare(x, y, z);

    WorldChunkSquare *sq = WorldChunkSquareCache.takeFirst();
    sq->ID = ++IDMax;
    sq->x = x;
    sq->y = y;
    sq->z = z;

    return sq;
}

void WorldChunkSquare::discard()
{
    chunk = 0;
//    roomID = -1;
//    room = 0;
    ID = 0;

    tiles.fill(0);

    WorldChunkSquareCache += this;
}

/////

QList<WorldChunk*> WorldChunk::WorldChunkCache;

WorldChunk::WorldChunk(WorldChunkMap *chunkMap) :
    wx(0),
    wy(0),
    mChunkMap(chunkMap)
{
    squares.resize(WorldChunkMap::MaxLevels);
    for (int z = 0; z < squares.size(); z++)
        squares[z] = new SparseSquareGrid(mChunkMap->TilesPerChunk, mChunkMap->TilesPerChunk);
}

#include <QDebug>
#include "progress.h"
void WorldChunk::LoadForLater(int wx, int wy)
{
    this->wx = wx;
    this->wy = wy;

    QRectF bounds = QRectF(/*mChunkMap->getWorldXMinTiles() +*/ this->wx * mChunkMap->TilesPerChunk,
                           /*mChunkMap->getWorldYMinTiles() +*/ this->wy * mChunkMap->TilesPerChunk,
                           mChunkMap->TilesPerChunk, mChunkMap->TilesPerChunk);

//    qDebug() << "WorldChunk::LoadForLater" << bounds;

    // No event processing allowed during these scripts!
    ScriptList scripts;
    foreach (WorldLevel *wlevel, mChunkMap->mDocument->world()->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            scripts += layer->lookup()->scripts(bounds);

//    QList<WorldPath::Path*> paths = mChunkMap->mDocument->lookupPaths(bounds);
    qDebug() << QString::fromLatin1("Running scripts (%1) #paths=%2").arg(scripts.size())/*.arg(paths.size())*/;

    foreach (WorldScript *ws, scripts) {
        Tiled::Lua::LuaScript ls(mChunkMap->mWorld, ws);
        ls.runFunction("run");
    }
}

void WorldChunk::setSquare(int x, int y, int z, WorldChunkSquare *square)
{
    squares[z]->replace(x, y, square);
    if (square)
        square->chunk = this;
}

WorldChunkSquare *WorldChunk::getGridSquare(int x, int y, int z)
{
    if ((z >= WorldChunkMap::MaxLevels) || (z < 0)) {
        return 0;
    }
    return const_cast<WorldChunkSquare*>(squares[z]->at(x, y));
}

void WorldChunk::reuseGridsquares()
{
    for (int z = 0; z < squares.size(); z++) {
        squares[z]->beginIterate();
        while (WorldChunkSquare *sq = squares[z]->nextIterate()) {
            sq->discard();
        }
        squares[z]->clear();
    }
}

void WorldChunk::Save(bool bSaveQuit)
{
    // save out to a .bin file here

    reuseGridsquares();
}

WorldChunk *WorldChunk::getNew(WorldChunkMap *chunkMap)
{
    if (WorldChunkCache.isEmpty())
        return new WorldChunk(chunkMap);

    WorldChunk *chunk = WorldChunkCache.takeFirst();
    chunk->mChunkMap = chunkMap;
    return chunk;
}

void WorldChunk::discard()
{
    wx = -1, wy = -1;
    WorldChunkCache += this;
}

/////

class CMTileSink : public WorldTileLayer::TileSink
{
public:
    CMTileSink(WorldChunkMap *chunkMap, WorldTileLayer *wtl) :
        mChunkMap(chunkMap),
        mLayer(wtl),
        mLayerIndex(wtl->wlevel()->indexOf(wtl))
    {

    }

    QRect bounds()
    {
        return QRect(mChunkMap->getWorldXMinTiles(),
                     mChunkMap->getWorldYMinTiles(),
                     mChunkMap->getWidthInTiles(),
                     mChunkMap->getWidthInTiles());
    }

    void putTile(int x, int y, WorldTile *tile)
    {
#if 1
        WorldChunk *chunk = mChunkMap->getChunkForWorldPos(x, y);
        if (!chunk) return;
        if (mChunkMap->mLoadingChunk && (chunk != mChunkMap->mLoadingChunk)) return;
        WorldChunkSquare *sq = chunk->getGridSquare(x - chunk->wx * mChunkMap->TilesPerChunk,
                                                    y - chunk->wy * mChunkMap->TilesPerChunk,
                                                    mLayer->level());
        if (!sq && !tile) return;
        if (!sq) {
            sq = WorldChunkSquare::getNew(x, y, mLayer->level());
            chunk->setSquare(x - chunk->wx * mChunkMap->TilesPerChunk,
                             y - chunk->wy * mChunkMap->TilesPerChunk,
                             mLayer->level(), sq);

        }
#else
        WorldChunkSquare *sq = mChunkMap->getGridSquare(x, y, mLayer->level());
        if (!sq && !tile) return;
        if (!sq && QRect(mChunkMap->getWorldXMinTiles(),
                         mChunkMap->getWorldYMinTiles(),
                         mChunkMap->getWidthInTiles(),
                         mChunkMap->getWidthInTiles()).contains(x,y)) {
            if (WorldChunk *chunk = mChunkMap->getChunkForWorldPos(x, y)) {
                sq = WorldChunkSquare::getNew(x, y, mLayer->level());
                chunk->setSquare(x - chunk->wx * mChunkMap->TilesPerChunk,
                                 y - chunk->wy * mChunkMap->TilesPerChunk,
                                 mLayer->level(), sq);
            }
        }
#endif
        if (sq) {
            if (sq->tiles.size() < mLayerIndex + 1)
                sq->tiles.resize(mLayerIndex + 1);
            sq->tiles[mLayerIndex] = tile;
        }

    }

    WorldTile *getTile(int x, int y)
    {
        WorldChunkSquare *sq = mChunkMap->getGridSquare(x, y, mLayer->level());
        if (sq && (mLayerIndex < sq->tiles.size()))
            return sq->tiles[mLayerIndex];
        return 0;
    }

    WorldChunkMap *mChunkMap;
    WorldTileLayer *mLayer;
    int mLayerIndex;
};

WorldChunkMap::WorldChunkMap(PathDocument *doc) :
    mDocument(doc),
    mWorld(doc->world()),
    WorldX(5),
    WorldY(5),
    mLoadingChunk(0),
    XMinTiles(-1),
    XMaxTiles(-1),
    YMinTiles(-1),
    YMaxTiles(-1)
{
    Chunks.resize(ChunkGridWidth);
    for (int x = 0; x < ChunkGridWidth; x++)
        Chunks[x].resize(ChunkGridWidth);

    foreach (WorldLevel *wlevel, mWorld->levels()) {
        foreach (WorldTileLayer *wtl, wlevel->tileLayers())
            wtl->addSink(new CMTileSink(this, wtl));
    }

    connect(mDocument->changer(), SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(nodeMoved(WorldNode*,QPointF)));
    connect(mDocument->changer(), SIGNAL(afterRemoveNodeSignal(WorldPathLayer*,int,WorldNode*)),
            SLOT(afterRemoveNode(WorldPathLayer*,int,WorldNode*)));

    connect(mDocument->changer(), SIGNAL(afterAddScriptToPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterAddScriptToPath(WorldPath*,int,WorldScript*)));
    connect(mDocument->changer(), SIGNAL(afterRemoveScriptFromPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterRemoveScriptFromPath(WorldPath*,int,WorldScript*)));
    connect(mDocument->changer(), SIGNAL(afterChangeScriptParametersSignal(WorldScript*)),
            SLOT(afterChangeScriptParameters(WorldScript*)));

    connect(mDocument->changer(), SIGNAL(afterAddPathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterAddPath(WorldPathLayer*,int,WorldPath*)));
    connect(mDocument->changer(), SIGNAL(afterRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterRemovePath(WorldPathLayer*,int,WorldPath*)));

    connect(mDocument->changer(), SIGNAL(afterAddNodeToPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterAddNodeToPath(WorldPath*,int,WorldNode*)));
    connect(mDocument->changer(), SIGNAL(afterRemoveNodeFromPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterRemoveNodeFromPath(WorldPath*,int,WorldNode*)));
}

void WorldChunkMap::LoadChunkForLater(int wx, int wy, int x, int y)
{
    WorldChunk *chunk = WorldChunk::getNew(this);
    setChunk(x, y, chunk);

    mLoadingChunk = chunk;
    chunk->LoadForLater(wx, wy);
    mLoadingChunk = 0;
}

WorldChunkSquare *WorldChunkMap::getGridSquare(int x, int y, int z)
{
    x -= getWorldXMin() * TilesPerChunk;
    y -= getWorldYMin() * TilesPerChunk;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles()) || (z < 0) || (z > MaxLevels)) {
        return 0;
    }

    WorldChunk *c = getChunk(x / TilesPerChunk, y / TilesPerChunk);
    if (c == 0)
        return 0;
    return c->getGridSquare(x % TilesPerChunk, y % TilesPerChunk, z);
}

WorldChunk *WorldChunkMap::getChunkForWorldPos(int x, int y)
{
    x -= getWorldXMin() * TilesPerChunk;
    y -= getWorldYMin() * TilesPerChunk;

    if ((x < 0) || (y < 0) || (x >= getWidthInTiles()) || (y >= getWidthInTiles()) /*|| (z < 0) || (z > MaxLevels)*/) {
        return 0;
    }

    return getChunk(x / TilesPerChunk, y / TilesPerChunk);
}

WorldChunk *WorldChunkMap::getChunk(int x, int y)
{
    if ((x < 0) || (x >= ChunkGridWidth) || (y < 0) || (y >= ChunkGridWidth)) {
        return 0;
    }
    return Chunks[x][y];
}

void WorldChunkMap::setChunk(int x, int y, WorldChunk *c)
{
    if (x < 0 || x >= ChunkGridWidth || y < 0 || y >= ChunkGridWidth) return;
    if (c && Chunks[x][y]) {
        int i = 0;
    }

    Q_ASSERT(!c || (Chunks[x][y] == 0));
    Chunks[x][y] = c;
}

void WorldChunkMap::UpdateCellCache()
{
#if 0
    int i = getWidthInTiles();
    for (int x = 0; x < i; ++x) {
        for (int y = 0; y < i; ++y) {
            for (int z = 0; z < MaxLevels; ++z) {
                WorldChunkSquare *sq = getGridSquare(x + getWorldXMinTiles(),
                                                     y + getWorldYMinTiles(), z);
                cell->setCacheGridSquareLocal(x, y, z, sq);
            }
        }
    }
#endif
}

int WorldChunkMap::getWorldXMin()
{
    return (WorldX - (ChunkGridWidth / 2));
}

int WorldChunkMap::getWorldYMin()
{
    return (WorldY - (ChunkGridWidth / 2));
}

int WorldChunkMap::getWorldXMinTiles()
{
    if (XMinTiles != -1) return XMinTiles;
    XMinTiles = (getWorldXMin() * TilesPerChunk);
    return XMinTiles;
}

int WorldChunkMap::getWorldYMinTiles()
{
    if (YMinTiles != -1) return YMinTiles;
    YMinTiles = (getWorldYMin() * TilesPerChunk);
    return YMinTiles;
}

void WorldChunkMap::setCenter(int x, int y)
{
    int wx = x / TilesPerChunk;
    int wy = y / TilesPerChunk;
    wx = qBound(ChunkGridWidth / 2, wx, (mWorld->width()) / TilesPerChunk - ChunkGridWidth / 2);
    wy = qBound(ChunkGridWidth / 2, wy, (mWorld->height()) / TilesPerChunk - ChunkGridWidth / 2);
//    qDebug() << "WorldChunkMap::setCenter x,y=" << x << "," << y << " wx,wy=" << wx << "," << wy;
    if (wx != WorldX || wy != WorldY) {
        QRegion current = QRect(getWorldXMin(), getWorldYMin(),
                                ChunkGridWidth, ChunkGridWidth);
        QRegion updated = QRect(wx - ChunkGridWidth / 2, wy - ChunkGridWidth / 2,
                                ChunkGridWidth, ChunkGridWidth);

        // Discard old chunks.
        foreach (QRect r, (current - updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (WorldChunk *c = getChunk(x - getWorldXMin(), y - getWorldYMin())) {
                        c->Save(false);
                        setChunk(x - getWorldXMin(), y - getWorldYMin(), 0);
                        c->discard();
                    }
                }
            }
        }

        // Shift preserved chunks.
        QVector<WorldChunk*> preserved;
        foreach (QRect r, (current & updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (WorldChunk *c = getChunk(x - getWorldXMin(), y - getWorldYMin())) {
                        setChunk(x - getWorldXMin(), y - getWorldYMin(), 0);
                        preserved += c;
                    }
                }
            }
        }

        WorldX = wx;
        WorldY = wy;
        XMinTiles = YMinTiles = -1;

        foreach (WorldChunk *c, preserved) {
            setChunk(c->wx - getWorldXMin(), c->wy - getWorldYMin(), c);
        }

        // Load new chunks;
        foreach (QRect r, (updated - current).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
//                    qDebug() << "Load chunk" << x << "," << y;
                    LoadChunkForLater(x, y, x - getWorldXMin(), y - getWorldYMin());
                }
            }
        }

        UpdateCellCache();
#if 0
        for (int x = 0; x < Chunks.size(); x++) {
            for (int y = 0; y < Chunks[x].size(); y++) {
                if (IsoChunk *chunk = Chunks[x][y]) {
                    if (!mScene->mHeadersExamined.contains(chunk->lotheader)) {
                        mScene->mHeadersExamined += chunk->lotheader;
                        foreach (QString tileName, chunk->lotheader->tilesUsed) {
                            if (!mScene->mTileByName.contains(tileName)) {
                                if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName)) {
                                    mScene->mTileByName[tileName] = tile;
                                }
                            }
                        }
                    }
                }
            }
        }

        mScene->setMaxLevel(mWorld->CurrentCell->MaxHeight);
#endif
    }
}

void WorldChunkMap::nodeMoved(WorldNode *node, const QPointF &prev)
{
    foreach (WorldPath *path, node->mPaths.keys()) {
        if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
            QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
        mScriptsThatChanged += path->scripts().toSet();
    }
}

void WorldChunkMap::afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    Q_UNUSED(node)
    // Removing nodes from paths is what matters
}

#include "tilepainter.h"
void WorldChunkMap::nodesMoved()
{
    ScriptList scripts = mScriptsThatChanged.toList();
    mScriptsThatChanged.clear();
    qDebug() << QString::fromLatin1("%1 scripts in nodesMoved").arg(scripts.size());

    QRegion dirty = mScriptChangeArea;
    mScriptChangeArea = QRegion();

    // Accumulate the old regions, and calculate the new region for each script.
    foreach (WorldScript *ws, scripts) {
        if (dirty.isEmpty())
            dirty = ws->mRegion;
        else
            dirty |= ws->mRegion;
        Tiled::Lua::LuaScript ls(mWorld, ws);
        if (ls.runFunction("region")) {
            QRegion rgn;
            if (ls.getResultRegion(rgn)) {
                if (rgn != ws->mRegion) {
                    ws->mRegion = rgn;
                    if (ws->mPaths.size())
                        ws->mPaths.first()->layer()->lookup()->scriptRegionChanged(ws);
                    if (!ws->mRegion.isEmpty()) {
                        if (dirty.isEmpty())
                            dirty = ws->mRegion;
                        else
                            dirty |= ws->mRegion;
                    }
                }
            }
        }
    }

    dirty &= QRect(getWorldXMinTiles(), getWorldYMinTiles(), getWidthInTiles(), getWidthInTiles());

    TilePainter painter(mWorld);
    foreach (WorldLevel *wlevel, mWorld->levels()) {
        foreach (WorldTileLayer *wtl, wlevel->tileLayers()) {
            painter.setLayer(wtl);
            painter.erase(dirty);
        }
    }

    scripts.clear();
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            scripts += layer->lookup()->scripts(dirty.boundingRect());

    int runN = 0;
    foreach (WorldScript *ws, scripts) {
        if (!ws->mRegion.intersects(dirty)) continue;
        Tiled::Lua::LuaScript ls(mWorld, ws);
        ls.runFunction("run");
        runN++;
    }
    qDebug() << QString::fromLatin1("%1 (down from %2) scripts in nodesMoved area")
                .arg(runN).arg(scripts.size());

    emit chunksUpdated(); // for the GUI
#if 0
    ///////////

    ScriptList scripts;
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            scripts += layer->lookup()->scripts(area.adjusted(-1,-1,1,1));
    qDebug() << QString::fromLatin1("%1 scripts in nodesMoved area #1").arg(scripts.size());

    // erase tile layers where scripts used to be
    TilePainter painter(mWorld);
    QRegion dirty;
    foreach (WorldScript *ws, scripts) {
        if (!ws->mRegion.isEmpty()) {
            if (dirty.isEmpty())
                dirty = ws->mRegion;
            else
                dirty |= ws->mRegion;
        }
    }

    foreach (WorldScript *ws, scripts) {
        Tiled::Lua::LuaScript ls(mWorld, ws);
        if (ls.runFunction("region")) {
            QRegion rgn;
            if (ls.getResultRegion(rgn)) {
                if (rgn != ws->mRegion) {
//                    mDocument->scriptRegionChanged(ws);
                    ws->mRegion = rgn;
                    if (ws->mPaths.size())
                        ws->mPaths.first()->layer()->lookup()->scriptRegionChanged(ws);
                    if (!ws->mRegion.isEmpty()) {
                        if (dirty.isEmpty())
                            dirty = ws->mRegion;
                        else
                            dirty |= ws->mRegion;
                    }
                }
            }
        }
    }

    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldTileLayer *wtl, wlevel->tileLayers()) {
            painter.setLayer(wtl);
            painter.erase(dirty);
        }

    dirty &= QRect(getWorldXMinTiles(), getWorldYMinTiles(), getWidthInTiles(), getWidthInTiles());
    scripts.clear();
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            scripts += layer->lookup()->scripts(dirty.boundingRect());

    int runN = 0;
    foreach (WorldScript *ws, scripts) {
        if (!ws->mRegion.intersects(dirty)) continue;
        Tiled::Lua::LuaScript ls(mWorld, ws);
        ls.runFunction("run");
        runN++;
    }
    qDebug() << QString::fromLatin1("%1 (down from %2) scripts in nodesMoved area #2")
                .arg(runN).arg(scripts.size());

    emit chunksUpdated(); // for the GUI
#endif
}

void WorldChunkMap::afterAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    Q_UNUSED(path)
    Q_UNUSED(index)
    if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
        QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
    mScriptsThatChanged += script;
}

void WorldChunkMap::afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script)
{
    Q_UNUSED(path)
    Q_UNUSED(index)
    if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
        QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
    mScriptChangeArea |= script->mRegion;
}

void WorldChunkMap::afterChangeScriptParameters(WorldScript *script)
{
    if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
        QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
    mScriptsThatChanged += script;
}

void WorldChunkMap::afterAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    // Regenerate tiles where the path's scripts are.
    foreach (WorldScript *script, path->scripts()) {
        if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
            QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
        mScriptChangeArea |= script->mRegion;
    }
}

void WorldChunkMap::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    // Regenerate tiles where the path's scripts used to be.
    foreach (WorldScript *script, path->scripts()) {
        if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
            QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
        mScriptChangeArea |= script->mRegion;
    }
}

void WorldChunkMap::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
        QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
    mScriptsThatChanged += path->scripts().toSet();
}

void WorldChunkMap::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    // Removed a node from a path.  Do script:region() and script:run() on every
    // script used by the path, and update lookup for the script.
    if (mScriptChangeArea.isEmpty() && mScriptsThatChanged.isEmpty())
        QMetaObject::invokeMethod(this, "nodesMoved", Qt::QueuedConnection);
    mScriptsThatChanged += path->scripts().toSet();
}

void WorldChunkMap::processScriptRegionChanges()
{
    QRegion dirty = mScriptChangeArea;
    mScriptChangeArea = QRegion();

    TilePainter painter(mWorld);

    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldTileLayer *wtl, wlevel->tileLayers()) {
            painter.setLayer(wtl);
            painter.erase(dirty);
        }

    dirty &= QRect(getWorldXMinTiles(), getWorldYMinTiles(), getWidthInTiles(), getWidthInTiles());
    ScriptList scripts;
    foreach (WorldLevel *wlevel, mWorld->levels())
        foreach (WorldPathLayer *layer, wlevel->pathLayers())
            scripts += layer->lookup()->scripts(dirty.boundingRect());

    int runN = 0;
    foreach (WorldScript *ws, scripts) {
        if (!ws->mRegion.intersects(dirty)) continue;
        Tiled::Lua::LuaScript ls(mWorld, ws);
        ls.runFunction("run");
        runN++;
    }
    qDebug() << QString::fromLatin1("%1 (down from %2) scripts in processScriptRegionChanges area")
                .arg(runN).arg(scripts.size());

    emit chunksUpdated(); // for the GUI
}
