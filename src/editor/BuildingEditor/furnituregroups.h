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

#ifndef FURNITUREGROUPS_H
#define FURNITUREGROUPS_H

#include <QCoreApplication>
#include <QList>
#include <QSize>
#include <QStringList>
#include <QVector>

class SimpleFileBlock;

namespace BuildingEditor {

class BuildingTile;
class FloorTileGrid;
class FurnitureGroup;
class FurnitureTiles;

class FurnitureTile
{
public:
    enum FurnitureOrientation {
        FurnitureUnknown = -1,
        FurnitureW,
        FurnitureN,
        FurnitureE,
        FurnitureS,
        FurnitureSW,
        FurnitureNW,
        FurnitureNE,
        FurnitureSE,
        OrientCount
    };

    FurnitureTile(FurnitureTiles *ftiles, FurnitureOrientation orient);

    FurnitureTiles *owner() const
    { return mOwner; }

    void clear();

    bool isEmpty() const;

    FurnitureOrientation orient() const
    { return mOrient; }

    bool isW() const { return mOrient == FurnitureW; }
    bool isN() const { return mOrient == FurnitureN; }
    bool isE() const { return mOrient == FurnitureE; }
    bool isS() const { return mOrient == FurnitureS; }

    bool isSW() const { return mOrient == FurnitureSW; }
    bool isNW() const { return mOrient == FurnitureNW; }
    bool isNE() const { return mOrient == FurnitureNE; }
    bool isSE() const { return mOrient == FurnitureSE; }

    QString orientToString() const
    {
        static const char *s[] = { "W", "N", "E", "S", "SW", "NW", "NE", "SE" };
        return QLatin1String(s[mOrient]);
    }

    QSize size() const;
    int width() const { return size().width(); }
    int height() const { return size().height(); }

    bool equals(FurnitureTile *other) const;

    void setTile(int x, int y, BuildingTile *btile);

//    BuildingTile *tile(int n) const;

    BuildingTile *tile(int x, int y) const;

    const QVector<BuildingTile*> &tiles() const
    { return mTiles; }

    FurnitureTile *resolved();

    static bool isCornerOrient(FurnitureOrientation orient);

    FloorTileGrid *toFloorTileGrid(QRegion &rgn);

private:
    void resize(int width, int height);
    bool columnEmpty(int x);
    bool rowEmpty(int y);

private:
    FurnitureTiles *mOwner;
    FurnitureOrientation mOrient;
    QSize mSize;
    QVector<BuildingTile*> mTiles;
};

class FurnitureTiles
{
public:
    enum FurnitureLayer {
        InvalidLayer = -1,
        LayerWalls,
        LayerRoofCap,
        LayerWallOverlay,
        LayerWallFurniture,
        LayerFurniture,
        LayerFrames,
        LayerDoors,
        LayerRoof,
        LayerCount
    };

    FurnitureTiles(bool corners);
    ~FurnitureTiles();

    void setGroup(FurnitureGroup *group)
    { mGroup = group; }

    FurnitureGroup *group() const
    { return mGroup; }

    bool isEmpty() const;

    bool hasCorners() const
    { return mCorners; }

    void toggleCorners()
    { mCorners = !mCorners; }

    void setTile(FurnitureTile *ftile);
    FurnitureTile *tile(FurnitureTile::FurnitureOrientation orient) const;
    FurnitureTile *tile(int orient) const;

    const QVector<FurnitureTile*> &tiles() const
    { return mTiles; }

    bool equals(const FurnitureTiles *other);

    void setLayer(FurnitureLayer layer)
    { mLayer = layer; }

    FurnitureLayer layer() const
    { return mLayer; }

    static int layerCount()
    { return LayerCount; }

    QString layerToString() const
    { return layerToString(mLayer); }

    static QString layerToString(FurnitureLayer layer);
    static FurnitureLayer layerFromString(const QString &s);

    static QStringList layerNames()
    {
        initNames();
        return mLayerNames;
    }

private:
    static void initNames();

private:
    FurnitureGroup *mGroup;
    QVector<FurnitureTile*> mTiles;
    bool mCorners;
    FurnitureLayer mLayer;
    static QStringList mLayerNames;
};

class FurnitureGroup
{
public:
    FurnitureTiles *findMatch(FurnitureTiles *ftiles) const;
    QString mLabel;
    QList<FurnitureTiles*> mTiles;
};

class FurnitureGroups : public QObject
{
    Q_OBJECT
public:
    static FurnitureGroups *instance();
    static void deleteInstance();

    FurnitureGroups();

    void addGroup(FurnitureGroup *group);
    void insertGroup(int index, FurnitureGroup *group);
    FurnitureGroup *removeGroup(int index);
    void removeGroup(FurnitureGroup *group);

    FurnitureTiles *findMatch(FurnitureTiles *other);

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();

    const QList<FurnitureGroup*> groups() const
    { return mGroups; }

    int groupCount() const
    { return mGroups.count(); }

    FurnitureGroup *group(int index) const
    {
        if (index < 0 || index >= mGroups.count())
            return 0;
        return mGroups[index];
    }

    int indexOf(FurnitureGroup *group) const
    { return mGroups.indexOf(group); }

    int indexOf(const QString &name) const;

    QString errorString()
    { return mError; }

    void tileChanged(FurnitureTile *ftile);
    void layerChanged(FurnitureTiles *ftiles);

    static FurnitureTile::FurnitureOrientation orientFromString(const QString &s);

    FurnitureTiles *furnitureTilesFromSFB(SimpleFileBlock &furnitureBlock, QString &error);
    SimpleFileBlock furnitureTilesToSFB(FurnitureTiles *ftiles);

signals:
    void furnitureLayerChanged(FurnitureTiles *ftiles);
    void furnitureTileChanged(FurnitureTile *ftile);

private:
    bool upgradeTxt();
    bool mergeTxt();

private:
    static FurnitureGroups *mInstance;
    QList<FurnitureGroup*> mGroups;
    int mRevision;
    int mSourceRevision;
    QString mError;
};

} // namespace BuildingEditor

#endif // FURNITUREGROUPS_H
