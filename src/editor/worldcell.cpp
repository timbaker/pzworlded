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

WorldCellLot::WorldCellLot(WorldCell *cell, const QString &name, int x, int y,
                           int z, int width, int height)
    : mName(name)
    , mX(x)
    , mY(y)
    , mZ(z)
    , mWidth(width)
    , mHeight(height)
    , mCell(cell)
{
}

WorldCellLot::WorldCellLot(WorldCell *cell, WorldCellLot *other)
    : mName(other->mapName())
    , mX(other->x())
    , mY(other->y())
    , mZ(other->level())
    , mWidth(other->width())
    , mHeight(other->height())
    , mCell(cell)
{
}

/////

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
            || !mObjects.isEmpty()
            || !properties().isEmpty()
            || !templates().isEmpty())
        return false;
    return true;
}

/////

WorldObjectGroup::WorldObjectGroup(World *world)
    : mType(world->nullObjectType())
{
    Q_ASSERT(mType);
}

WorldObjectGroup::WorldObjectGroup(World *world, const QString &name)
    : mName(name)
    , mType(world->nullObjectType())
{
    Q_ASSERT(!name.isEmpty());
}

WorldObjectGroup::WorldObjectGroup(World *world, const QString &name, const QColor &color)
    : mName(name)
    , mColor(color)
    , mType(world->nullObjectType())
{
    Q_ASSERT(!name.isEmpty());
}

WorldObjectGroup::WorldObjectGroup(World *world, WorldObjectGroup *other)
    : mName(other->name())
    , mColor(other->color())
{
    mType = world->objectTypes().find(other->type()->name());
    if (!mType)
        mType = world->nullObjectType();
}

bool WorldObjectGroup::operator ==(const WorldObjectGroup &other) const
{
    return mName == other.mName &&
            mColor == other.mColor &&
            mType->name() == other.mType->name();
}

/////

ObjectType::ObjectType()
{
}

ObjectType::ObjectType(const QString &name)
    : mName(name)
{
    Q_ASSERT(!name.isEmpty());
}

ObjectType::ObjectType(World *world, ObjectType *other)
    : mName(other->name())
{
    Q_UNUSED(world)
}

bool ObjectType::operator ==(const ObjectType &other) const
{
    return mName == other.name();
}

/////

void WorldCellContents::swapWorld(World *world)
{
    foreach (Property *p, mProperties) {
        PropertyDef *pd = world->propertyDefinitions().findPropertyDef(p->mDefinition->mName);
        Q_ASSERT(pd);
        p->mDefinition = pd;
    }

    for (int i = 0; i < mTemplates.size(); i++) {
        PropertyTemplate *pt = world->propertyTemplates().find(mTemplates.at(i)->mName);
        Q_ASSERT(pt);
        mTemplates[i] = pt;
    }

    foreach (WorldCellObject *obj, mObjects) {
        WorldObjectGroup *og = world->objectGroups().find(obj->group()->name());
        obj->setGroup(og ? og : world->nullObjectGroup());

        ObjectType *ot = world->objectTypes().find(obj->type()->name());
        obj->setType(ot ? ot : world->nullObjectType());
    }
}

void WorldCellContents::mergeOnto(WorldCell *cell)
{
    World *world = cell->world();
    foreach (Property *pCell, cell->properties()) {
        Property *p = mProperties.find(pCell->mDefinition);
        if (!p)
            mProperties += new Property(world, pCell);
    }
    foreach (PropertyTemplate *ptCell, cell->templates()) {
        PropertyTemplate *pt = mTemplates.find(ptCell->mName);
        if (!pt)
            mTemplates += new PropertyTemplate(world, ptCell);
    }
    int index = 0;
    foreach (WorldCellLot *lotCell, cell->lots()) {
        WorldCellLot *lot = new WorldCellLot(cell, lotCell);
        mLots.insert(index++, lot);
    }
    index = 0;
    foreach (WorldCellObject *objCell, cell->objects()) {
        WorldCellObject *obj = new WorldCellObject(cell, objCell);
        mObjects.insert(index++, obj);
    }
    if (mMapFilePath.isEmpty())
        mMapFilePath = cell->mapFilePath();
}
