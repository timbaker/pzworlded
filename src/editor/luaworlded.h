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

#ifndef LUAWORLDED_H
#define LUAWORLDED_H

#include "luatiled.h"

#include <QPolygonF>
#include <QSet>

class MapInfo;
class PathWorld;
class TilePainter;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;
class WorldTile;
class WorldTileLayer;

namespace Tiled {
namespace Lua {

class LuaPathLayer;
class LuaWorld;

class LuaNode
{
public:
    LuaNode(LuaPathLayer *layer, WorldNode *node, bool owner = false);
    ~LuaNode();

    qreal x() const;
    qreal y() const;
    void setX(qreal x);
    void setY(qreal y);

    void initClone();

    LuaPathLayer *mLayer;
    WorldNode *mClone;
    WorldNode *mNode;
    bool mOwner;
};

class LuaPath
{
public:
    LuaPath(LuaPathLayer *layer, WorldPath *path, bool owner = false);
    ~LuaPath();

    LuaPath *stroke(double thickness);
    LuaRegion region();

    int nodeCount();
    LuaNode *nodeAt(int index);

    LuaPathLayer *mLayer;
    WorldPath *mClone;
    WorldPath *mPath;
    bool mOwner;
    QList<LuaNode*> mNodes;
};

class LuaPathLayer
{
public:
    LuaPathLayer(LuaWorld *world, WorldPathLayer *layer);
    ~LuaPathLayer();

    LuaPath *luaPath(WorldPath *path);

    LuaWorld *mWorld;
    WorldPathLayer *mLayer;
    QList<LuaPath*> mPaths;
};

class LuaWorldScript
{
public:
    LuaWorldScript(LuaWorld *world, WorldScript *worldScript);
    ~LuaWorldScript();

    QList<LuaPath*> paths();

    const char *value(const char *key);

    LuaWorld *mWorld;
    WorldScript *mWorldScript;
    QList<LuaPath*> mPaths;
};

class LuaWorld
{
public:
    LuaWorld(PathWorld *world);
    ~LuaWorld();

    WorldTile *tile(const char *tilesetName, int id);
    WorldTileLayer *tileLayer(const char *name);

    LuaPath *luaPath(WorldPath *path);

    PathWorld *mWorld;
    QList<LuaPathLayer*> mPathLayers;
    QSet<LuaNode*> mModifiedNodes;
};

class LuaMapInfo;

class LuaTilePainter
{
public:
    LuaTilePainter(TilePainter *painter);

    void setLayer(WorldTileLayer *layer);
    void setTile(WorldTile *tile);
    void fill(LuaPath *path);
    void fill(const QRect &r);
    void strokePath(LuaPath *path, qreal thickness);
    void drawMap(int wx, int wy, LuaMapInfo *mapInfo);

    TilePainter *mPainter;
};

QPolygonF strokePath(LuaPath *path, qreal thickness);
LuaRegion polygonRegion(QPolygonF polygon);

class LuaMapInfo
{
public:
    LuaMapInfo(MapInfo *mapInfo);

    static LuaMapInfo *get(const char *path);

    const char *path();
    QRect bounds();
    LuaRegion region();

    MapInfo *mMapInfo;
};

} // namespace Lua
} // namespace Tiled

#endif // LUAWORLDED_H
