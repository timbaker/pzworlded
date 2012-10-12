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

#include "world.h"

#include "bmptotmx.h" // FIXME: remove from this file
#include "worldcell.h"

#include <QStringList>

World::World(int width, int height)
    : mWidth(width)
    , mHeight(height)
    , mNullObjectType(new ObjectType())
    , mNullObjectGroup(new WorldObjectGroup(this))
{
    mCells.resize(mWidth * mHeight);

    for (int y = 0; y < mHeight; y++) {
        for (int x = 0; x < mWidth; x++) {
            mCells[y * mWidth + x] = new WorldCell(this, x, y);
        }
    }

    // The nameless default group for WorldCellObjects
    mObjectGroups.append(mNullObjectGroup);

    // The nameless default type for WorldCellObjects
    mObjectTypes.append(mNullObjectType);

#if 1
    QString bmpPath = QLatin1String("C:\\Programming\\ProjectZomboid\\Maps\\muldraugh.bmp");
    BMPToTMXImages *images = BMPToTMX::instance()->getImages(bmpPath, QPoint(10, 10));
    if (images)
        mBMPImages += images;
#endif
}

World::~World()
{
    qDeleteAll(mCells);
    qDeleteAll(mPropertyTemplates);
    qDeleteAll(mPropertyDefs);
    qDeleteAll(mObjectTypes);
}

PropertyDef *World::removePropertyDefinition(int index)
{
    return mPropertyDefs.takeAt(index);
}

void World::insertObjectGroup(int index, WorldObjectGroup *og)
{
    mObjectGroups.insert(index, og);
}

WorldObjectGroup *World::removeObjectGroup(int index)
{
    return mObjectGroups.takeAt(index);
}

void World::insertObjectType(int index, ObjectType *ot)
{
    mObjectTypes.insert(index, ot);
}

ObjectType *World::removeObjectType(int index)
{
    return mObjectTypes.takeAt(index);
}

void World::insertRoad(int index, Road *road)
{
    mRoads.insert(index, road);
}

Road *World::removeRoad(int index)
{
    return mRoads.takeAt(index);
}

void World::insertBmp(int index, BMPToTMXImages *images)
{
    mBMPImages.insert(index, images);
}

BMPToTMXImages *World::removeBmpImages(int index)
{
    return mBMPImages.takeAt(index);
}

/////

bool ObjectGroupList::contains(const QString &name) const
{
    return find(name) != 0;
}

WorldObjectGroup *ObjectGroupList::find(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->name() == name)
            return *it;
        it++;
    }
    return 0;
}

QStringList ObjectGroupList::names() const
{
    QStringList result;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        result += (*it)->name();
        ++it;
    }
    return result;
}

/////

bool ObjectTypeList::contains(const QString &name) const
{
    return find(name) != 0;
}

ObjectType *ObjectTypeList::find(const QString &name) const
{
    const_iterator it = constBegin();
    while (it != constEnd()) {
        if ((*it)->name() == name)
            return *it;
        it++;
    }
    return 0;
}

QStringList ObjectTypeList::names() const
{
    QStringList result;
    const_iterator it = constBegin();
    while (it != constEnd()) {
        result += (*it)->name();
        ++it;
    }
    return result;
}
