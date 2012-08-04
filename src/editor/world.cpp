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

#include "worldcell.h"

#include <QStringList>

World::World(int width, int height)
    : mWidth(width)
    , mHeight(height)
    , mNullObjectGroup(new WorldObjectGroup())
    , mNullObjectType(new ObjectType())
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

#if 0
    mObjectTypes.append(new ObjectType(QLatin1String("zone")));
    mObjectTypes.append(new ObjectType(QLatin1String("roomDef")));
    mObjectTypes.append(new ObjectType(QLatin1String("ZombieSpawn")));
#endif

#if 0
    PropertyDef *pd = new PropertyDef;
    pd->mName = QLatin1String("zombie-attraction");
    pd->mDefaultValue = QLatin1String("0.1");
    pd->mDescription = QLatin1String("Range 0.0 to 1.0.");
    mPropertyDefs += pd;

    pd = new PropertyDef;
    pd->mName = QLatin1String("start-zombies-indoor");
    pd->mDefaultValue = QLatin1String("2");
    pd->mDescription = QLatin1String("Range 1 to 100.");
    mPropertyDefs += pd;

    PropertyTemplate *pt = new PropertyTemplate;
    pt->mName = QLatin1String("LargeUrbanCenter");
    pt->addProperty( 0, new Property(pd, QLatin1String("1.0")) );
    pt->mDescription = QLatin1String("Typical zombie/survivor spawn rates in a major urban center.");
    mPropertyTemplates += pt;

    cellAt(50, 50)->addTemplate(0, pt);

    pd = mPropertyDefs.findPropertyDef(QLatin1String("zombie-attraction"));
    cellAt(50, 50)->addProperty(0, new Property(pd, QLatin1String("1000000")));

    pd = mPropertyDefs.findPropertyDef(QLatin1String("start-zombies-indoor"));
    cellAt(50, 50)->addProperty(1, new Property(pd, QLatin1String("80")));
#endif

#if 0
    cellAt(52, 50)->insertObject(0, new WorldCellObject(cellAt(52,50), QLatin1String("Name"), QLatin1String("Type"), 10.5, 10.5, 0, 1, 1));
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
