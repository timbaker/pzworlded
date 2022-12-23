/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "worldcell.h"

#include "world.h"

WorldCellObject::WorldCellObject(WorldCell *cell, const QString &name, ObjectType *type,
                                 WorldObjectGroup *group, qreal x, qreal y,
                                 int level, qreal width, qreal height)
    : mName(name)
    , mGroup(group)
    , mType(type)
    , mX(x)
    , mY(y)
    , mZ(level)
    , mWidth(width)
    , mHeight(height)
    , mGeometryType(ObjectGeometryType::INVALID)
    , mPolylineWidth(0)
    , mCell(cell)
    , mVisible(true)
{
}

WorldCellObject::WorldCellObject(WorldCell *cell, WorldCellObject *other)
    : mName(other->name())
    , mX(other->x())
    , mY(other->y())
    , mZ(other->level())
    , mWidth(other->width())
    , mHeight(other->height())
    , mGeometryType(other->geometryType())
    , mPolylineWidth(0)
    , mCell(cell)
    , mVisible(other->isVisible())
{
    World *world = cell->world();

    WorldObjectGroup *og = world->objectGroups().find(other->group()->name());
    mGroup = og ? og : world->nullObjectGroup();

    ObjectType *ot = world->objectTypes().find(other->type()->name());
    mType = ot ? ot : world->nullObjectType();
}

int WorldCellObject::index()
{
    return mCell->indexOf(this);
}

bool WorldCellObject::isBasement() const
{
    return mType->name() == QLatin1String("Basement");
}

bool WorldCellObject::isRoomTone() const
{
    return mType->name() == QLatin1String("RoomTone");
}

bool WorldCellObject::isSpawnPoint() const
{
    return mType->name() == QLatin1String("SpawnPoint");
}

void WorldCellObject::calculateBounds()
{
    int minX = std::numeric_limits<int>::max();
    int minY = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int maxY = std::numeric_limits<int>::min();
    for (const auto& point : qAsConst(mPoints)) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    mX = minX;
    mY = minY;
    mWidth = maxX - minX + 1;
    mHeight = maxY - minY + 1;
}
