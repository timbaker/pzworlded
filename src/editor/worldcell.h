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

#ifndef WORLDCELL_H
#define WORLDCELL_H

#include "properties.h"

#include <QColor>
#include <QPoint>
#include <QList>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QString>

class World;
class WorldCell;
class WorldCellContents;

/**
  * This class represents a single "lot" or sub-map in a WorldCell.
  * Each WorldCell has its own list of these.
  */
class WorldCellLot
{
public:
    WorldCellLot(WorldCell *cell, const QString &name, int x, int y, int z, int width, int height);
    WorldCellLot(WorldCell *cell, WorldCellLot *other);

    void setMapName(const QString &mapName) { mName = mapName; }
    const QString &mapName() const { return mName; }

    int x() const { return mX; }
    int y() const { return mY; }

    void setLevel(int level) { mZ = level; }
    int level() const { return mZ; }

    void setPos(int x, int y) { mX = x, mY = y; }
    void setPos(const QPoint &pos) { setPos(pos.x(), pos.y()); }
    QPoint pos() const { return QPoint(mX, mY); }

    void setCell(WorldCell *cell) { mCell = cell; }
    WorldCell *cell() const { return mCell; }

    /**
      * The correct map dimensions are set when a lot is first
      * added to a cell.  If the map cannot be located later on,
      * we can still display a placeholder of the appropriate
      * size by saving the height and width to the project file.
      */
    void setWidth(int width) { mWidth = width; }
    void setHeight(int height) { mHeight = height; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QRect bounds() const
    { return QRect(mX, mY, mWidth, mHeight); }

private:
    QString mName;
    int mX, mY, mZ;
    int mWidth, mHeight;
    WorldCell *mCell;
};

class WorldCellLotList : public QList<WorldCellLot*>
{
public:
    WorldCellLotList clone(WorldCell *cell) const
    {
        WorldCellLotList copy;
        const_iterator it = constBegin();
        while (it != constEnd()) {
            WorldCellLot *lot = (*it);
            copy += new WorldCellLot(cell, lot->mapName(), lot->x(), lot->y(), lot->level(),
                                     lot->width(), lot->height());
            it++;
        }
        return copy;
    }

    const WorldCellLotList &setCell(WorldCell *cell) const
    {
        const_iterator it = constBegin();
        while (it != constEnd()) {
            WorldCellLot *lot = (*it);
            lot->setCell(cell);
            it++;
        }
        return *this;
    }
};

/**
  * This class represents the type of a WorldCellObject.
  * New types are added via the ObjectTypesDialog.
  * Each World has its own list of these.
  */
class ObjectType
{
public:
    ObjectType();
    ObjectType(const QString &name);
    ObjectType(World *world, ObjectType *other);

    void setName(const QString &name) { mName = name; }
    const QString &name() const { return mName; }

    bool isNull() { return mName.isEmpty(); }

    bool operator ==(const ObjectType &other) const;
    bool operator !=(const ObjectType &other) const
    { return !(*this == other); }

private:
    QString mName;
};

/**
  * This class represents the group a WorldCellObject belongs to.
  * An object belongs to exactly one group.
  * New groups are added via the ObjectGroupsDialog.
  * Each World has its own list of these.
  */
class WorldObjectGroup
{
public:
    WorldObjectGroup(World *world);
    WorldObjectGroup(World *world, const QString &name);
    WorldObjectGroup(World *world, const QString &name, const QColor &color);
    WorldObjectGroup(World *world, WorldObjectGroup *other);

    void setName(const QString &name) { mName = name; }
    const QString &name() const { return mName; }

    bool isNull() { return mName.isEmpty(); }

    void setColor(const QColor &color) { mColor = color; }
    QColor color() const
    { return mColor.isValid() ? mColor : defaultColor(); }

    static QColor defaultColor() { return Qt::darkGray; }

    /**
      * This is the default type for objects created in this group.
      */
    void setType(ObjectType *type) { mType = type; }
    ObjectType *type() { return mType; }

    bool operator ==(const WorldObjectGroup &other) const;
    bool operator !=(const WorldObjectGroup &other) const
    { return !(*this == other); }

private:
    QString mName;
    QColor mColor;
    ObjectType *mType;
};

/**
  * This class represents a single object in a WorldCell.
  * Each WorldCell has its own list of these.
  */
class WorldCellObject : public PropertyHolder
{
public:
    WorldCellObject(WorldCell *cell,
                    const QString &name, ObjectType *type,
                    WorldObjectGroup *group,
                    qreal x, qreal y, int level,
                    qreal width, qreal height);

    WorldCellObject(WorldCell *cell, WorldCellObject *other);

    void setName(const QString &name) { mName = name; }
    const QString &name() const { return mName; }

    void setGroup(WorldObjectGroup *group) { mGroup = group; }
    WorldObjectGroup *group() const { return mGroup; }

    void setType(ObjectType *type) { mType = type; }
    ObjectType *type() const { return mType; }

    void setPos(const QPointF &pos) { mX = pos.x(), mY = pos.y(); }
    QPointF pos() const { return QPointF(mX, mY); }
    qreal x() const { return mX; }
    qreal y() const { return mY; }

    void setWidth(qreal width) { mWidth = width; }
    qreal width() const { return mWidth; }

    void setHeight(qreal height) { mHeight = height; }
    qreal height() const { return mHeight; }

    QSizeF size() const { return QSizeF(mWidth, mHeight); }

    QRectF bounds() const { return QRectF(mX, mY, mWidth, mHeight); }

    void setLevel(int level) { mZ = level; }
    int level() const { return mZ; }

    void setCell(WorldCell *cell) { mCell = cell; }
    WorldCell *cell() const { return mCell; }

    void setVisible(bool visible) { mVisible = visible; }
    bool isVisible() const { return mVisible; }

private:
    QString mName;
    WorldObjectGroup *mGroup;
    ObjectType *mType;
    qreal mX, mY;
    int mZ;
    qreal mWidth, mHeight;
    WorldCell *mCell;
    bool mVisible;
};

class WorldCellObjectList : public QList<WorldCellObject*>
{
public:
    WorldCellObjectList clone(WorldCell *cell) const
    {
        WorldCellObjectList copy;
        const_iterator it = constBegin();
        while (it != constEnd()) {
            WorldCellObject *obj = (*it);
            copy += new WorldCellObject(cell, obj->name(), obj->type(),
                                        obj->group(), obj->x(), obj->y(),
                                        obj->level(), obj->width(), obj->height());
            it++;
        }
        return copy;
    }

    const WorldCellObjectList &setCell(WorldCell *cell) const
    {
        const_iterator it = constBegin();
        while (it != constEnd()) {
            WorldCellObject *obj = (*it);
            obj->setCell(cell);
            it++;
        }
        return *this;
    }
};

/**
  * This class represents a single cell in a World.
  */
class WorldCell : public PropertyHolder
{
public:
    WorldCell(World *world, int x, int y);
    ~WorldCell();

    int x() const { return mX; }
    int y() const { return mY; }

    QPoint pos() const { return QPoint(mX, mY); }

    World *world() const { return mWorld; }

    void setMapFilePath(const QString &path);
    QString mapFilePath() const { return mMapFilePath; }

    void addLot(const QString &name, int x, int y, int z, int width, int height)
    {
        mLots.append(new WorldCellLot(this, name, x, y, z, width, height));
    }

    void insertLot(int index, WorldCellLot *lot);
    WorldCellLot *removeLot(int index);
    const WorldCellLotList &lots() const { return mLots; }
    int indexOf(WorldCellLot *lot) { return mLots.indexOf(lot); }

    void insertObject(int index, WorldCellObject *obj);
    WorldCellObject *removeObject(int index);
    const WorldCellObjectList &objects() const { return mObjects; }

    bool isEmpty() const;

private:
    int mX, mY;
    World *mWorld;
    QString mMapFilePath;
    WorldCellLotList mLots;
    WorldCellObjectList mObjects;

    friend class WorldCellContents;
};

/**
  * This class is used to hold the complete contents of a WorldCell.
  * It is used when copying/pasting cells, and by the undo/redo system.
  */
class WorldCellContents
{
public:
    WorldCellContents(WorldCell *newOwner)
    {
        Q_UNUSED(newOwner)
    }

    WorldCellContents(WorldCell *cell, bool takeOwnership)
    {
        mMapFilePath = cell->mapFilePath();
        mTemplates = cell->templates();
        mPos = cell->pos();
        if (takeOwnership) {
            mProperties = cell->properties();
            mLots = cell->lots();
            mObjects = cell->objects();
            cell->mMapFilePath.clear();
            cell->mTemplates.clear();
            cell->mProperties.clear();
            cell->mLots.clear();
            cell->mObjects.clear();
        } else {
            WorldCell *newOwner = cell;
            mProperties = cell->properties().clone();
            mLots = cell->lots().clone(newOwner);
            mObjects = cell->objects().clone(newOwner);
        }
    }

    void setCellContents(WorldCell *cell, bool transferOwnership = true)
    {
        Q_ASSERT(cell->isEmpty());
        cell->mTemplates = mTemplates;
        cell->mMapFilePath = mMapFilePath;
        if (transferOwnership) {
            cell->mProperties = mProperties;
            cell->mLots = mLots.setCell(cell);
            cell->mObjects = mObjects.setCell(cell);
            mMapFilePath.clear();
            mTemplates.clear();
            mProperties.clear();
            mLots.clear();
            mObjects.clear();
        } else {
            cell->mProperties = mProperties.clone();
            cell->mLots = mLots.clone(cell);
            cell->mObjects = mObjects.clone(cell);
        }
    }

    WorldCellContents(const WorldCellContents *other, WorldCell *newOwner)
    {
        mTemplates = other->templates();
        mProperties = other->properties().clone();
        mMapFilePath = other->mapFilePath();
        mLots = other->lots().clone(newOwner);
        mObjects = other->objects().clone(newOwner);
        mPos = other->pos();
    }

    ~WorldCellContents()
    {
        qDeleteAll(mProperties);
        qDeleteAll(mLots);
        qDeleteAll(mObjects);
    }

    QString mapFilePath() const
    { return mMapFilePath; }

    const PropertyTemplateList &templates() const
    { return mTemplates; }

    const PropertyList &properties() const
    { return mProperties; }

    const WorldCellLotList &lots() const
    { return mLots; }

    const WorldCellObjectList &objects() const
    { return mObjects; }

    /**
      * This is for the convenience of the cell clipboard.
      * For pasting cells, at least the relative positions of
      * cells in the clipboard is needed.
      */
    void setPos(const QPoint &pos)
    { mPos = pos; }
    QPoint pos() const
    { return mPos; }

    void swapWorld(World *world);

    void mergeOnto(WorldCell *cell);

private:
    PropertyTemplateList mTemplates;
    PropertyList mProperties;
    QString mMapFilePath;
    WorldCellLotList mLots;
    WorldCellObjectList mObjects;
    QPoint mPos;
};

#endif // WORLDCELL_H
