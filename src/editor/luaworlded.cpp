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
    int i = 0;
    foreach (QPointF p, poly) {
        newPath->insertNode(i++, new WorldPath::Node(WorldPath::InvalidId, p));
    }

    return new LuaPath(newPath, true); // FIXME: never freed
}

LuaRegion LuaPath::region()
{
    return mPath->region();
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
