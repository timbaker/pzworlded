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

#include "path.h"
#include "pathworld.h"
#include "tilepainter.h"

#include "mapmanager.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QFileInfo>

using namespace Tiled::Lua;

/////

LuaNode::LuaNode(LuaPathLayer *layer, WorldNode *node, bool owner) :
    mLayer(layer),
    mClone(0),
    mNode(node),
    mOwner(owner)
{
}

LuaNode::~LuaNode()
{
    delete mClone;
}

qreal LuaNode::x() const
{
    return mClone ? mClone->pos().x() : mNode->pos().x();
}

qreal LuaNode::y() const
{
    return mClone ? mClone->pos().y() : mNode->pos().y();
}

void LuaNode::setX(qreal x)
{
    initClone();
    mClone->setX(x);
}

void LuaNode::setY(qreal y)
{
    initClone();
    mClone->setY(y);
}

void LuaNode::initClone()
{
    if (mClone) return;
    mClone = mNode->clone();
    if (mLayer && mLayer->mWorld)
        mLayer->mWorld->mModifiedNodes += this;
}

/////

LuaPath::LuaPath(LuaPathLayer *layer, WorldPath *path, bool owner) :
    mLayer(layer),
    mClone(0),
    mPath(path),
    mOwner(owner)
{
    foreach (WorldNode *node, mPath->nodes)
        mNodes += new LuaNode(layer, node, false); // FIXME: point to 'path' too?
}

LuaPath::~LuaPath()
{
    qDeleteAll(mNodes);
    if (mOwner) {
        qDeleteAll(mPath->nodes);
        delete mPath;
    }
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

    return new LuaPath(0, newPath, true); // FIXME: never freed
}

LuaRegion LuaPath::region()
{
    return mPath->region();
}

int LuaPath::nodeCount()
{
    return mNodes.size();
}

LuaNode *LuaPath::nodeAt(int index)
{
    return (index >= 0 && index < mNodes.size()) ? mNodes[index] : 0;
}

/////

LuaPathLayer::LuaPathLayer(LuaWorld *world, WorldPathLayer *layer) :
    mWorld(world),
    mLayer(layer)
{
    foreach (WorldPath *path, layer->paths())
        mPaths += new LuaPath(this, path);
}

LuaPathLayer::~LuaPathLayer()
{
    qDeleteAll(mPaths);
}

LuaPath *LuaPathLayer::luaPath(WorldPath *path)
{
    // FIXME: hash table lookup
    foreach (LuaPath *lpath, mPaths) {
        if (lpath->mPath == path)
            return lpath;
    }
    return 0;
}

/////

LuaWorldScript::LuaWorldScript(LuaWorld *world, WorldScript *worldScript) :
    mWorld(world),
    mWorldScript(worldScript)
{
    foreach (WorldPath *path, worldScript->mPaths) {
        if (LuaPath *lpath = mWorld->luaPath(path))
            mPaths += lpath;
    }
}

LuaWorldScript::~LuaWorldScript()
{
}

QList<LuaPath *> LuaWorldScript::paths()
{
    return mPaths;
}

const char *LuaWorldScript::value(const char *key)
{
    if (mWorldScript->mParams.contains(QLatin1String(key)))
        return cstring(mWorldScript->mParams[QLatin1String(key)]);
    return 0;
}

/////

LuaWorld::LuaWorld(PathWorld *world) :
    mWorld(world)
{
    foreach (WorldPathLayer *wpl, world->pathLayers())
        mPathLayers += new LuaPathLayer(this, wpl);
}

LuaWorld::~LuaWorld()
{
    qDeleteAll(mPathLayers);
}

WorldTile *LuaWorld::tile(const char *tilesetName, int id)
{
    return mWorld->tile(QLatin1String(tilesetName), id);
}

WorldTileLayer *LuaWorld::tileLayer(const char *layerName)
{
    return mWorld->tileLayer(QLatin1String(layerName));
}

LuaPath *LuaWorld::luaPath(WorldPath *path)
{
    foreach (LuaPathLayer *lpl, mPathLayers) {
        if (LuaPath *lpath = lpl->luaPath(path))
            return lpath;
    }
    return 0;
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

void LuaTilePainter::drawMap(int wx, int wy, LuaMapInfo *mapInfo)
{
    if (mapInfo && !mapInfo->mMapInfo->map()) {
        MapManager::instance()->loadMap(mapInfo->mMapInfo->path());
    }
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

