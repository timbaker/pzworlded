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

class WorldScript;

namespace WorldPath {
class Path;
}

namespace Tiled {
namespace Lua {

class LuaPath
{
public:
    LuaPath(WorldPath::Path *path, bool owner = false);
    ~LuaPath();

    LuaPath *stroke(double thickness);

    LuaRegion region();

    WorldPath::Path *mPath;
    bool mOwner;
};

class LuaWorldScript
{
public:
    LuaWorldScript(WorldScript *worldScript);
    ~LuaWorldScript();

    QList<LuaPath*> paths();

    WorldScript *mWorldScript;
    QList<LuaPath*> mPaths;
};

QPolygonF strokePath(LuaPath *path, qreal thickness);
LuaRegion polygonRegion(QPolygonF polygon);

} // namespace Lua
} // namespace Tiled

#endif // LUAWORLDED_H
