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

#ifndef BUILDINGFLOOR_H
#define BUILDINGFLOOR_H

#include <QHash>
#include <QList>
#include <QMap>
#include <QRegion>
#include <QString>
#include <QStringList>
#include <QVector>

namespace BuildingEditor {

class BuildingObject;
class Building;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FloorType;
class FurnitureObject;
class FurnitureTile;
class Room;
class Stairs;
class Window;

class FloorTileGrid
{
public:
    FloorTileGrid(int width, int height);

    int size() const
    { return mWidth * mHeight; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QRect bounds() const
    { return QRect(0, 0, mWidth, mHeight); }

    const QString &at(int index) const;

    const QString &at(int x, int y) const
    {
        Q_ASSERT(contains(x, y));
        return at(x + y * mWidth);
    }

    void replace(int index, const QString &tile);
    void replace(int x, int y, const QString &tile);
    bool replace(const QString &tile);
    bool replace(const QRegion &rgn, const QString &tile);
    bool replace(const QRegion &rgn, const QPoint &p, const FloorTileGrid *other);
    bool replace(const QRect &r, const QString &tile);
    bool replace(const QPoint &p, const FloorTileGrid *other);

    bool isEmpty() const
    { return !mCount; }

    void clear();

    bool contains(int x, int y) const
    { return x >= 0 && x < mWidth && y >= 0 && y < mHeight; }

    FloorTileGrid *clone();
    FloorTileGrid *clone(const QRect &r);
    FloorTileGrid *clone(const QRect &r, const QRegion &rgn);

private:
    void swapToVector();

    int mWidth, mHeight;
    int mCount;
    QHash<int,QString> mCells;
    QVector<QString> mCellsVector;
    bool mUseVector;
    QString mEmptyCell;
};

class BuildingFloor
{
public:

    class Square
    {
    public:
        enum SquareSection
        {
            SectionInvalid = -1,
            SectionFloor,
            SectionFloorGrime,
            SectionFloorGrime2,
            SectionWall,
            SectionWall2,
            SectionRoofCap,
            SectionRoofCap2,
            SectionWallOverlay,
            SectionWallOverlay2,
            SectionWallGrime,
            SectionWallGrime2,
            SectionWallFurniture,
            SectionWallFurniture2,
            SectionFrame,
            SectionDoor,
            SectionWindow,
            SectionCurtains, // West, North windows
            SectionFurniture,
            SectionFurniture2,
            SectionFurniture3,
            SectionFurniture4,
            SectionCurtains2, // East, South windows
            SectionRoof,
            SectionRoof2,
            SectionRoofTop,
            MaxSection
        };

        enum WallOrientation
        {
            WallOrientInvalid = -1,
            WallOrientN,
            WallOrientW,
            WallOrientNW,
            WallOrientSE
        };

        Square();
        ~Square();

        QVector<BuildingTileEntry*> mEntries;
        QVector<int> mEntryEnum;
        WallOrientation mWallOrientation;
        bool mExterior;
        QVector<BuildingTile*> mTiles;

        struct WallInfo {
            WallInfo() :
                entry(0), furniture(0)
            {}
            BuildingTileEntry *entry;
            FurnitureTile *furniture;
        } mWallN, mWallW;

        void SetWallN(BuildingTileEntry *tile);
        void SetWallW(BuildingTileEntry *tile);
        void SetWallN(FurnitureTile *ftile);
        void SetWallW(FurnitureTile *ftile);

        bool IsWallOrient(WallOrientation orient);

        void ReplaceFloor(BuildingTileEntry *tile, int offset);
        void ReplaceWall(BuildingTileEntry *tile, WallOrientation orient);
        void ReplaceDoor(BuildingTileEntry *tile, int offset);
        void ReplaceFrame(BuildingTileEntry *tile, int offset);
        void ReplaceWindow(BuildingTileEntry *tile, int offset);
        void ReplaceCurtains(Window *window, bool exterior);
        void ReplaceFurniture(BuildingTileEntry *tile, int offset = 0);
        void ReplaceFurniture(BuildingTile *btile, SquareSection sectionMin,
                              SquareSection sectionMax);
        void ReplaceRoof(BuildingTileEntry *tile, int offset = 0);
        void ReplaceRoofCap(BuildingTileEntry *tile, int offset = 0);
        void ReplaceRoofTop(BuildingTileEntry *tile, int offset);
        void ReplaceFloorGrime(BuildingTileEntry *grimeTile);
        void ReplaceWallGrime(BuildingTileEntry *grimeTile);

        int getWallOffset();
    };

    QVector<QVector<Square> > squares;

    BuildingFloor(Building *building, int level);
    ~BuildingFloor();

    void setLevel(int level)
    { mLevel = level; }

    Building *building() const
    { return mBuilding; }

    BuildingFloor *floorAbove() const;
    BuildingFloor *floorBelow() const;

    bool isTopFloor() const;
    bool isBottomFloor() const;

    void insertObject(int index, BuildingObject *object);
    BuildingObject *removeObject(int index);

    int indexOf(BuildingObject *object)
    { return mObjects.indexOf(object); }

    BuildingObject *object(int index) const
    { return mObjects.at(index); }

    const QList<BuildingObject*> &objects() const
    { return mObjects; }

    int objectCount() const
    { return mObjects.size(); }

    BuildingObject *objectAt(int x, int y);

    inline BuildingObject *objectAt(const QPoint &pos)
    { return objectAt(pos.x(), pos.y()); }

    Door *GetDoorAt(int x, int y);
    Window *GetWindowAt(int x, int y);
    Stairs *GetStairsAt(int x, int y);
    FurnitureObject *GetFurnitureAt(int x, int y);

    void SetRoomAt(int x, int y, Room *room);

    inline void SetRoomAt(const QPoint &pos, Room *room)
    { SetRoomAt(pos.x(), pos.y(), room); }

    Room *GetRoomAt(const QPoint &pos);
    Room *GetRoomAt(int x, int y)
    { return GetRoomAt(QPoint(x, y)); }

    void setGrid(const QVector<QVector<Room*> > &grid);

    const QVector<QVector<Room*> > &grid() const
    { return mRoomAtPos; }

    void LayoutToSquares();

    int width() const;
    int height() const;

    QRect bounds(int dw = 0, int dh = 0) const;

    int level() const
    { return mLevel; }

    bool contains(int x, int y, int dw = 0, int dh = 0);
    bool contains(const QPoint &p, int dw = 0, int dh = 0);

    QVector<QRect> roomRegion(Room *room);

    QVector<QVector<Room*> > resizeGrid(const QSize &newSize) const;
    QMap<QString,FloorTileGrid*> resizeGrime(const QSize &newSize) const;

    void rotate(bool right);
    void flip(bool horizontal);

    BuildingFloor *clone();

    const QMap<QString,FloorTileGrid*> &grime() const
    { return mGrimeGrid; }

    QStringList grimeLayers() const
    { return mGrimeGrid.keys(); }

    QString grimeAt(const QString &layerName, int x, int y) const;
    FloorTileGrid *grimeAt(const QString &layerName, const QRect &r);
    FloorTileGrid *grimeAt(const QString &layerName, const QRect &r, const QRegion &rgn);

    QMap<QString,FloorTileGrid*> grimeClone() const;

    QMap<QString,FloorTileGrid*> setGrime(const QMap<QString,FloorTileGrid*> &grime);
    void setGrime(const QString &layerName, int x, int y, const QString &tileName);
    void setGrime(const QString &layerName, const QPoint &p, const FloorTileGrid *other);
    void setGrime(const QString &layerName, const QRegion &rgn, const QString &tileName);
    void setGrime(const QString &layerName, const QRegion &rgn, const QPoint &pos, const FloorTileGrid *other);

    void setLayerOpacity(const QString &layerName, qreal opacity)
    { mLayerOpacity[layerName] = opacity; }

    qreal layerOpacity(const QString &layerName) const
    {
        if (mLayerOpacity.contains(layerName))
            return mLayerOpacity[layerName];
        return 1.0f;
    }

    void setLayerVisibility(const QString &layerName, bool visible)
    { mLayerVisibility[layerName] = visible; }

    bool layerVisibility(const QString &layerName) const
    {
        if (mLayerVisibility.contains(layerName))
            return mLayerVisibility[layerName];
        return 1.0f;
    }

private:
    Building *mBuilding;
    QVector<QVector<Room*> > mRoomAtPos;
    QVector<QVector<int> > mIndexAtPos;
    int mLevel;
    QList<BuildingObject*> mObjects;
    QMap<QString,FloorTileGrid*> mGrimeGrid;
    QMap<QString,qreal> mLayerOpacity;
    QMap<QString,bool> mLayerVisibility;
};

} // namespace BuildingEditor

#endif // BUILDINGFLOOR_H
