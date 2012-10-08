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

#ifndef WORLD_H
#define WORLD_H

#include "properties.h"
#include "road.h"

#include <QRect>
#include <QSize>
#include <QVector>

class WorldObjectGroup;
class ObjectType;
class WorldCell;

class ObjectGroupList : public QList<WorldObjectGroup*>
{
public:
    bool contains(const QString &name) const;
    WorldObjectGroup *find(const QString &name) const;

    QStringList names() const;
};

class ObjectTypeList : public QList<ObjectType*>
{
public:
    bool contains(const QString &name) const;
    ObjectType *find(const QString &name) const;

    QStringList names() const;
};

class World
{
public:
    World(int width, int height);
    ~World();

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    QSize size() const { return QSize(mWidth, mHeight); }
    QRect bounds() const { return QRect(0, 0, mWidth, mHeight); }

    const QVector<WorldCell*> &cells() const { return mCells; }
    WorldCell *cellAt(int x, int y)
    {
        return contains(x, y) ? mCells[y * mWidth + x] : 0;
    }
    WorldCell *cellAt(const QPoint &pos)
    { return cellAt(pos.x(), pos.y()); }

    bool contains(int x, int y)
    { return x >= 0 && x < mWidth && y >= 0 && y < mHeight; }
    bool contains(const QPoint &pos)
    { return contains(pos.x(), pos.y()); }

    void addPropertyTemplate(int index, PropertyTemplate *pt)
    { mPropertyTemplates.insert(index, pt); }
    PropertyTemplate *removeTemplate(int index)
    { return mPropertyTemplates.takeAt(index); }

    void addPropertyDefinition(int index, PropertyDef *pd)
    { mPropertyDefs.insert(index, pd); }
    PropertyDef *removePropertyDefinition(int index);

    void insertObjectGroup(int index, WorldObjectGroup *og);
    WorldObjectGroup *removeObjectGroup(int index);

    void insertObjectType(int index, ObjectType *ot);
    ObjectType *removeObjectType(int index);

    void insertRoad(int index, Road *road);
    Road *removeRoad(int index);
    RoadList roadsInRect(const QRect &bounds);

    const ObjectGroupList &objectGroups() const
    { return mObjectGroups; }
    const ObjectTypeList &objectTypes() const
    { return mObjectTypes; }
    const PropertyDefList &propertyDefinitions() const
    { return mPropertyDefs; }
    const PropertyTemplateList &propertyTemplates() const
    { return mPropertyTemplates; }
    const RoadList &roads() const
    { return mRoads; }

    WorldObjectGroup *nullObjectGroup() const { return mNullObjectGroup; }
    ObjectType *nullObjectType() const { return mNullObjectType; }

private:
    int mWidth;
    int mHeight;
    QVector<WorldCell*> mCells;
    ObjectTypeList mObjectTypes;
    ObjectType *mNullObjectType;
    ObjectGroupList mObjectGroups;
    WorldObjectGroup *mNullObjectGroup;
    PropertyDefList mPropertyDefs;
    PropertyTemplateList mPropertyTemplates;
    RoadList mRoads;
};

#endif // WORLD_H
