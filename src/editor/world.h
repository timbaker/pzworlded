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

#include "worldproperties.h"
#include "road.h"

#include <QRect>
#include <QSize>
#include <QStringList>
#include <QVector>

class BMPToTMXImages;
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

class BMPToTMXSettings
{
public:
    QString exportDir;
    QString rulesFile;
    QString blendsFile;
    QString mapbaseFile;
    bool assignMapsToWorld;
    bool warnUnknownColors;
    bool compress;

    BMPToTMXSettings() :
        assignMapsToWorld(false),
        warnUnknownColors(true),
        compress(true)
    {
    }

    bool operator == (const BMPToTMXSettings &other)
    {
        return exportDir == other.exportDir &&
                rulesFile == other.rulesFile &&
                blendsFile == other.blendsFile &&
                mapbaseFile == other.mapbaseFile &&
                assignMapsToWorld == other.assignMapsToWorld &&
                warnUnknownColors == other.warnUnknownColors &&
                compress == other.compress;
    }

    bool operator != (const BMPToTMXSettings &other)
    { return !operator==(other); }
};

class GenerateLotsSettings
{
public:
    QString exportDir;
    QString zombieSpawnMap;
    QPoint worldOrigin;

    bool operator == (const GenerateLotsSettings &other)
    {
        return exportDir == other.exportDir &&
                zombieSpawnMap == other.zombieSpawnMap &&
                worldOrigin == other.worldOrigin;
    }

    bool operator != (const GenerateLotsSettings &other)
    { return !operator==(other); }
};

class LuaSettings
{
public:
    QString spawnPointsFile;

    bool operator == (const LuaSettings &other)
    {
        return spawnPointsFile == other.spawnPointsFile;
    }

    bool operator != (const LuaSettings &other)
    { return !operator==(other); }
};

/**
  * This class represents a single .bmp file used by BMP -> TMX conversion.
  */
class WorldBMP
{
public:
    WorldBMP(World *world, int x, int y, int width, int height,
             const QString &filePath);

    World *world() const
    { return mWorld; }

    void setPos(int x, int y)
    { mX = x, mY = y; }

    void setPos(const QPoint &pos)
    { setPos(pos.x(), pos.y()); }

    QPoint pos() const
    { return QPoint(mX, mY); }

    int x() const { return mX; }
    int y() const { return mY; }

    void setFilePath(const QString &filePath)
    { mFilePath = filePath; }

    const QString &filePath() const
    { return mFilePath; }

    // The correct width and height is set when a BMP is first added to
    // a World.  If the .bmp file cannot be located later on we can still
    // display a placeholder of the correct size.
    void setSize(const QSize &size)
    { setSize(size.width(), size.height()); }
    void setSize(int width, int height)
    { mWidth = width, mHeight = height; }
    QSize size() const { return QSize(mWidth, mHeight); }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QRect bounds() const
    { return QRect(mX, mY, mWidth, mHeight); }

private:
    World *mWorld;
    int mX, mY;
    int mWidth, mHeight;
    QString mFilePath;
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

    void swapCells(QVector<WorldCell *> &cells);
    void setSize(const QSize &newSize);

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
    PropertyTemplate *propertyTemplate(const QString &name)
    { return mPropertyTemplates.find(name); }

    void addPropertyDefinition(int index, PropertyDef *pd)
    { mPropertyDefs.insert(index, pd); }
    PropertyDef *removePropertyDefinition(int index);
    PropertyDef *propertyDefinition(const QString &name)
    { return mPropertyDefs.findPropertyDef(name); }

    void insertPropertyEnum(int index, PropertyEnum *pe)
    { mPropertyEnums.insert(index, pe); }
    PropertyEnum *removePropertyEnum(int index)
    { return mPropertyEnums.takeAt(index); }
    const PropertyEnumList &propertyEnums() const
    { return mPropertyEnums; }

    void insertObjectGroup(int index, WorldObjectGroup *og);
    WorldObjectGroup *removeObjectGroup(int index);

    void insertObjectType(int index, ObjectType *ot);
    ObjectType *removeObjectType(int index);
    ObjectType *objectType(const QString &name)
    { return mObjectTypes.find(name); }

    void insertRoad(int index, Road *road);
    Road *removeRoad(int index);
    RoadList roadsInRect(const QRect &bounds);

    void insertBmp(int index, WorldBMP *bmp);
    WorldBMP *removeBmp(int index);

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
    const QList<WorldBMP*> bmps() const
    { return mBMPs; }

    void setBMPToTMXSettings(const BMPToTMXSettings &settings)
    { mBMPToTMXSettings = settings; }

    const BMPToTMXSettings &getBMPToTMXSettings() const
    { return mBMPToTMXSettings; }

    void setGenerateLotsSettings(const GenerateLotsSettings &settings)
    { mGenerateLotsSettings = settings; }

    const GenerateLotsSettings &getGenerateLotsSettings() const
    { return mGenerateLotsSettings; }

    void setLuaSettings(const LuaSettings &settings)
    { mLuaSettings = settings; }

    const LuaSettings &getLuaSettings() const
    { return mLuaSettings; }

    WorldObjectGroup *nullObjectGroup() const { return mNullObjectGroup; }
    ObjectType *nullObjectType() const { return mNullObjectType; }

    void setHeightMapFileName(const QString &fileName)
    { mHeightMapFileName = fileName; }
    const QString &hmFileName() const
    { return mHeightMapFileName; }

private:
    int mWidth;
    int mHeight;
    QVector<WorldCell*> mCells;
    ObjectTypeList mObjectTypes;
    ObjectType *mNullObjectType;
    ObjectGroupList mObjectGroups;
    WorldObjectGroup *mNullObjectGroup;
    PropertyEnumList mPropertyEnums;
    PropertyDefList mPropertyDefs;
    PropertyTemplateList mPropertyTemplates;
    RoadList mRoads;
    QList<WorldBMP*> mBMPs;
    BMPToTMXSettings mBMPToTMXSettings;
    GenerateLotsSettings mGenerateLotsSettings;
    LuaSettings mLuaSettings;
    QString mHeightMapFileName;
};

#endif // WORLD_H
