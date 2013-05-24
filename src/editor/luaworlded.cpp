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

#include "luaworlded.h"

#include "mapmanager.h"
#include "path.h"
#include "pathworld.h"
#include "tilepainter.h"
#include "worldchanger.h"

#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QFileInfo>

using namespace Tiled::Lua;

/////

LuaNode::LuaNode(LuaWorld *world, WorldNode *node, bool owner) :
    mWorld(world),
    mNode(node),
    mOwner(owner)
{
}

LuaNode::~LuaNode()
{
}

qreal LuaNode::x() const
{
    return mNode->pos().x();
}

qreal LuaNode::y() const
{
    return mNode->pos().y();
}

void LuaNode::setX(qreal x)
{
    mWorld->mChanger->doMoveNode(mNode, QPointF(x, y()));
}

void LuaNode::setY(qreal y)
{
    mWorld->mChanger->doMoveNode(mNode, QPointF(x(), y));
}

/////

LuaPath::LuaPath(LuaWorld *world, WorldPath *path, bool owner) :
    mWorld(world),
    mPath(path),
    mOwner(owner)
{
}

LuaPath::~LuaPath()
{
    if (mOwner) {
        qDeleteAll(mPath->nodes);
        delete mPath;
    }
}

LuaPathLayer *LuaPath::layer()
{
    return mWorld->luaPathLayer(mPath->layer());
}

LuaPath *LuaPath::stroke(double thickness)
{
    QPolygonF poly = ::strokePath(mPath, thickness);
    WorldPath *newPath = new WorldPath;
    for (int i = 0; i < poly.size(); i++) {
        newPath->insertNode(i, new WorldNode(InvalidId, poly[i]));
    }
    if (newPath->nodes.size() && !mPath->isClosed()) {
        newPath->insertNode(newPath->nodes.size(), newPath->nodes[0]);
    }

    return new LuaPath(mWorld, newPath, true); // FIXME: never freed
}

LuaRegion LuaPath::region()
{
    return mPath->region();
}

int LuaPath::nodeCount()
{
    return mPath->nodeCount();
}

QList<LuaNode *> LuaPath::nodes()
{
    QList<LuaNode*> ret;
    foreach (WorldNode *node, mPath->nodes)
        ret += mWorld->luaNode(node);
    return ret;
}

LuaNode *LuaPath::nodeAt(int index)
{
    return mWorld->luaNode(mPath->nodeAt(index));
}

LuaNode *LuaPath::first()
{
    return mWorld->luaNode(mPath->first());
}

LuaNode *LuaPath::last()
{
    return mWorld->luaNode(mPath->last());
}

void LuaPath::addNode(LuaNode *node)
{
    if (mPath->mLayer != node->mNode->layer) return; // error!
    mWorld->mChanger->doAddNodeToPath(mPath, mPath->nodeCount(), node->mNode);
}

/////

LuaPathLayer::LuaPathLayer(LuaWorld *world, WorldPathLayer *layer) :
    mWorld(world),
    mLayer(layer)
{
}

LuaPathLayer::~LuaPathLayer()
{
}

LuaPath *LuaPathLayer::newPath()
{
    WorldPath *path = mWorld->mWorld->allocPath();
    mWorld->mChanger->doAddPath(mLayer, mLayer->pathCount(), path);
    return mWorld->luaPath(path);
}

void LuaPathLayer::removePath(LuaPath *path)
{
    if (path->mPath->layer() != mLayer) return; // error!
    mWorld->mChanger->doRemovePath(mLayer, mLayer->indexOf(path->mPath), path->mPath);
}

/////

LuaWorldScript::LuaWorldScript(LuaWorld *world, WorldScript *worldScript) :
    mWorld(world),
    mWorldScript(worldScript)
{
}

LuaWorldScript::~LuaWorldScript()
{
}

QList<LuaNode *> LuaWorldScript::nodes()
{
    QList<LuaNode *> nodes;
    foreach (WorldNode *node, mWorldScript->mNodes)
        if (LuaNode *lnode = mWorld->luaNode(node))
            nodes += lnode;
    return nodes;
}

QList<LuaPath *> LuaWorldScript::paths()
{
    QList<LuaPath *> paths;
    foreach (WorldPath *path, mWorldScript->mPaths)
        if (LuaPath *lpath = mWorld->luaPath(path))
            paths += lpath;
    return paths;
}

LuaPathLayer *LuaWorldScript::currentPathLayer()
{
    return mWorld->luaPathLayer(mWorldScript->mCurrentPathLayer);
}

const char *LuaWorldScript::value(const char *key)
{
    if (mWorldScript->mParams.contains(QLatin1String(key)))
        return cstring(mWorldScript->mParams[QLatin1String(key)]);
    return 0;
}

/////

LuaWorld::LuaWorld(PathWorld *world, WorldChanger *changer) :
    mWorld(world),
    mChanger(changer)
{
}

LuaWorld::~LuaWorld()
{
    qDeleteAll(mPathLayers);
    qDeleteAll(mPaths);
    qDeleteAll(mNodes);
}

WorldTile *LuaWorld::tile(const char *tilesetName, int id)
{
    return mWorld->tile(QLatin1String(tilesetName), id);
}

WorldTileLayer *LuaWorld::tileLayer(const char *layerName)
{
    return mWorld->tileLayer(QLatin1String(layerName));
}

LuaNode *LuaWorld::luaNode(WorldNode *node)
{
    if (!node) return 0;
    if (mNodes.contains(node))
        return mNodes[node];
    mNodes[node] = new LuaNode(this, node);
    return mNodes[node];
}

LuaPath *LuaWorld::luaPath(WorldPath *path)
{
    if (!path) return 0;
    if (mPaths.contains(path))
        return mPaths[path];
    mPaths[path] = new LuaPath(this, path);
    return mPaths[path];
}

LuaPathLayer *LuaWorld::luaPathLayer(WorldPathLayer *layer)
{
    if (!layer) return 0;
    if (mPathLayers.contains(layer))
        return mPathLayers[layer];
    mPathLayers[layer] = new LuaPathLayer(this, layer);
    return mPathLayers[layer];
}

/////

LuaTilePainter::LuaTilePainter(TilePainter *painter) :
    mPainter(painter)
{

}

void LuaTilePainter::setLayer(WorldTileLayer *layer)
{
    mPainter->setLayer(layer);
}

void LuaTilePainter::setTile(WorldTile *tile)
{
    mPainter->setTile(tile);
}

void LuaTilePainter::setTileAlias(const char *name)
{
    if (!name || !name[0])
        mPainter->setTileAlias(0);
    else
        mPainter->setTileAlias(mPainter->world()->tileAlias(QLatin1String(name)));
}

void LuaTilePainter::setTileRule(const char *name)
{
    if (!name || !name[0])
        mPainter->setTileRule(0);
    else
        mPainter->setTileRule(mPainter->world()->tileRule(QLatin1String(name)));
}

void LuaTilePainter::erase(LuaPath *path)
{
    mPainter->erase(path->mPath);
}

void LuaTilePainter::erase(const QRect &r)
{
    mPainter->erase(r);
}

void LuaTilePainter::fill(LuaPath *path)
{
    mPainter->fill(path->mPath);
}

void LuaTilePainter::fill(const QRect &r)
{
    mPainter->fill(r);
}

void LuaTilePainter::strokePath(LuaPath *path, qreal thickness)
{
    mPainter->strokePath(path->mPath, thickness);
}

void LuaTilePainter::tracePath(LuaPath *path, qreal offset)
{
    mPainter->tracePath(path->mPath, offset);
}

void LuaTilePainter::drawMap(int wx, int wy, LuaMapInfo *mapInfo)
{
    if (mapInfo && !mapInfo->mMapInfo->map()) {
        MapManager::instance()->loadMap(mapInfo->mMapInfo->path());
    }
#if 1
    mPainter->drawMap(wx, wy, mapInfo->mMapInfo);
#else
    if (!mapInfo || !mapInfo->mMapInfo->map() || !mapInfo->mMapInfo->map()->tileLayerCount())
        return;

    foreach (TileLayer *tl, mapInfo->mMapInfo->map()->tileLayers()) {
        if (WorldTileLayer *wtl = mPainter->world()->tileLayer(tl->name())) {
            mPainter->setLayer(wtl);
            for (int y = 0; y < tl->height(); y++) {
                for (int x = 0; x < tl->width(); x++) {
                    Tiled::Tile *tile = tl->cellAt(x, y).tile;
                    if (!tile) continue;
                    if (WorldTileset *wts = mPainter->world()->tileset(tile->tileset()->name())) { // FIXME: slow
                        mPainter->setTile(wts->tileAt(tile->id())); // FIXME: assumes valid tile id
                        mPainter->fill(wx + x, wy + y, 1, 1);
                    }
                }
            }
        }
    }
#endif
}

/////

LuaMapInfo::LuaMapInfo(MapInfo *mapInfo) :
    mMapInfo(mapInfo)
{
}

LuaMapInfo *LuaMapInfo::get(const char *path)
{
    QFileInfo info;
    info.setFile(QLatin1String(path));
    if (!info.exists())
        return 0;
    if (MapInfo *mapInfo = MapManager::instance()->mapInfo(info.canonicalFilePath()))
        return new LuaMapInfo(mapInfo); // FIXME: never freed
    return 0;
}

const char *LuaMapInfo::path()
{
    return cstring(mMapInfo->path());
}

QRect LuaMapInfo::bounds()
{
    return mMapInfo->bounds();
}

LuaRegion LuaMapInfo::region()
{
    return LuaRegion(QRect(mMapInfo->bounds()));
}


/////

#if 0
namespace Tiled {
namespace Lua {
QPolygonF strokePath(LuaPath *path, qreal thickness)
{
    return ::strokePath(path->mPath, thickness);
}


LuaRegion polygonRegion(QPolygonF polygon)
{
    return ::polygonRegion(polygon);
}

}
}

#endif

