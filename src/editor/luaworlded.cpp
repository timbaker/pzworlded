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
#include "tilepainter.h"
#include "world.h"

using namespace Tiled::Lua;


LuaPath::LuaPath(WorldPath::Path *path, bool owner) :
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

LuaPath *LuaPath::stroke(double thickness)
{
    QPolygonF poly = strokePath(mPath, thickness);
    WorldPath::Path *newPath = new WorldPath::Path;
    for (int i = 0; i < poly.size() - 1; i++) {
        newPath->insertNode(i, new WorldPath::Node(WorldPath::InvalidId, poly[i]));
    }
    if (newPath->nodes.size())
        newPath->insertNode(newPath->nodes.size(), newPath->nodes[0]);

    return new LuaPath(newPath, true); // FIXME: never freed
}

LuaRegion LuaPath::region()
{
    return mPath->region();
}

/////

LuaWorldScript::LuaWorldScript(WorldScript *worldScript) :
    mWorldScript(worldScript)
{
    foreach (WorldPath::Path *path, worldScript->mPaths)
        mPaths += new LuaPath(path, false);
}

LuaWorldScript::~LuaWorldScript()
{
    qDeleteAll(mPaths);
}

QList<LuaPath *> LuaWorldScript::paths()
{
    return mPaths;
}

/////

LuaWorld::LuaWorld(World *world) :
    mWorld(world)
{
}

WorldTile *LuaWorld::tile(const char *tilesetName, int id)
{
    return mWorld->tile(QLatin1String(tilesetName), id);
}

WorldTileLayer *LuaWorld::tileLayer(const char *layerName)
{
    return mWorld->tileLayer(QLatin1String(layerName));
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

void LuaTilePainter::strokePath(LuaPath *path, qreal thickness)
{
    mPainter->strokePath(path->mPath, thickness);
}

/////

namespace Tiled {
namespace Lua {
QPolygonF strokePath(LuaPath *path, qreal thickness)
{
    return WorldPath::strokePath(path->mPath, thickness);
}


LuaRegion polygonRegion(QPolygonF polygon)
{
    return WorldPath::polygonRegion(polygon);
}


}
}
