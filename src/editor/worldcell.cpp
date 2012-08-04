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

WorldCell::WorldCell(World *world, int x, int y)
    : mX(x)
    , mY(y)
    , mWorld(world)
{
}

WorldCell::~WorldCell()
{
    qDeleteAll(mLots);
    qDeleteAll(mObjects);
}

void WorldCell::setMapFilePath(const QString &path)
{
    mMapFilePath = path;
}

void WorldCell::insertLot(int index, WorldCellLot *lot)
{
    mLots.insert(index, lot);
}

WorldCellLot *WorldCell::removeLot(int index)
{
    return mLots.takeAt(index);
}

void WorldCell::insertObject(int index, WorldCellObject *obj)
{
    mObjects.insert(index, obj);
}

WorldCellObject *WorldCell::removeObject(int index)
{
    return mObjects.takeAt(index);
}

bool WorldCell::isEmpty() const
{
    if (!mMapFilePath.isEmpty()
        || !mLots.isEmpty()
        || !properties().isEmpty()
        || !templates().isEmpty())
        return false;
    return true;
}

/////

WorldObjectGroup::WorldObjectGroup()
{
}

WorldObjectGroup::WorldObjectGroup(const QString &name)
    : mName(name)
{
}

/////

ObjectType::ObjectType()
{
}

ObjectType::ObjectType(const QString &name)
    : mName(name)
{
}
