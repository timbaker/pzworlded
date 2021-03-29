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

#include "buildingcell.h"
#include "buildingobjects.h"
#include "maprotation.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QRegion>
#include <QString>
#include <QStringList>
#include <QVector>

namespace BuildingEditor {

class Building;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FloorType;
class FurnitureObject;
class FurnitureTile;
class RoofObject;
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

    const BuildingCell &at(int index) const;

    const BuildingCell &at(int x, int y) const
    {
        Q_ASSERT(contains(x, y));
        return at(x + y * mWidth);
    }

    void replace(int index, const BuildingCell &tile);
    void replace(int x, int y, const BuildingCell &tile);
    bool replace(const BuildingCell &tile);
    bool replace(const QRegion &rgn, const BuildingCell &tile);
    bool replace(const QRegion &rgn, const QPoint &p, const FloorTileGrid *other);
    bool replace(const QRect &r, const BuildingCell &tile);
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
    QHash<int,BuildingCell> mCells;
    QVector<BuildingCell> mCellsVector;
    bool mUseVector;
    const BuildingCell mEmptyCell;
};

class BuildingSquare
{
public:
    // The order must match buildingmap.cpp:gLayerNames[]
    enum SquareSection
    {
        SectionInvalid = -1,
        SectionFloor,
        SectionFloorGrime,
        SectionFloorGrime2,
        SectionFloorGrime3,
        SectionFloorGrime4,
        SectionWall,
        SectionWallTrim,
        SectionWall2,
        SectionWallTrim2,
        SectionWall3,
        SectionWallTrim3,
        SectionWall4,
        SectionWallTrim4,
        SectionRoofCap,
        SectionRoofCap2,
        SectionWallOverlay,
        SectionWallOverlay2,
        SectionWallOverlay3,
        SectionWallOverlay4,
        SectionWallGrime,
        SectionWallGrime2,
        SectionWallGrime3,
        SectionWallGrime4,
        SectionWallFurniture,
        SectionWallFurniture2,
        SectionWallFurniture3,
        SectionWallFurniture4,
        SectionFrame1,
        SectionFrame2,
        SectionFrame3,
        SectionFrame4,
        SectionDoor1,
        SectionDoor2,
        SectionDoor3,
        SectionDoor4,
        SectionWindow1,
        SectionWindow2,
        SectionWindow3,
        SectionWindow4,
        SectionCurtains1,
        SectionCurtains2,
        SectionCurtains3,
        SectionCurtains4,
        SectionFurniture,
        SectionFurniture2,
        SectionFurniture3,
        SectionFurniture4,
        SectionRoof,
        SectionRoof2,
        SectionRoofTop,
        MaxSection
    };

    enum class WallEdge
    {
        N,
        E,
        S,
        W
    };

    enum class WallType
    {
        Invalid = -1,
        Simple,
        TwoWallCorner,
        Pillar
    };

    BuildingSquare();
    ~BuildingSquare();

    QVector<BuildingTileEntry*> mEntries;
    QVector<int> mEntryEnum;
    bool mExterior;
    QVector<BuildingTile*> mTiles;
    QVector<Tiled::MapRotation> mRotation;
    QVector<BuildingObject*> mObjects; // cached for faster lookup

    struct WallInfo {
        WallInfo(WallEdge edge) :
            edge(edge),
            entryWall(nullptr),
            entryTrim(nullptr),
            door(nullptr),
            window(nullptr),
            furniture(nullptr),
            furnitureBldgTile(nullptr),
            type(WallType::Invalid)
        {

        }
        WallEdge edge;
        BuildingTileEntry *entryWall;
        BuildingTileEntry *entryTrim;
        Door *door;
        Window *window;
        FurnitureTile *furniture;
        BuildingTile *furnitureBldgTile;
        WallType type;
    };

    WallInfo mWallN = WallInfo(WallEdge::N);
    WallInfo mWallW = WallInfo(WallEdge::W);
    WallInfo mWallE = WallInfo(WallEdge::E);
    WallInfo mWallS = WallInfo(WallEdge::S);

    void SetWallN(BuildingTileEntry *tile);
    void SetWallW(BuildingTileEntry *tile);
    void SetWallN(FurnitureTile *ftile, BuildingTile *btile);
    void SetWallW(FurnitureTile *ftile, BuildingTile *btile);

    void SetWallTrimN(BuildingTileEntry *tile);
    void SetWallTrimW(BuildingTileEntry *tile);
    void SetWallTrimE(BuildingTileEntry *tile);
    void SetWallTrimS(BuildingTileEntry *tile);

    void SetWallS(BuildingTileEntry *tile);
    void SetWallE(BuildingTileEntry *tile);
    void SetWallS(FurnitureTile *ftile, BuildingTile *btile);
    void SetWallE(FurnitureTile *ftile, BuildingTile *btile);

    bool HasWallN() const;
    bool HasWallW() const;

    bool HasDoorN() const;
    bool HasDoorS() const;
    bool HasDoorW() const;
    bool HasDoorE() const;

    bool HasDoorFrameN() const;
    bool HasDoorFrameS() const;
    bool HasDoorFrameW() const;
    bool HasDoorFrameE() const;

    bool HasWindowN() const;
    bool HasWindowS() const;
    bool HasWindowW() const;
    bool HasWindowE() const;

    bool HasCurtainsN() const;
    bool HasCurtainsS() const;
    bool HasCurtainsW() const;
    bool HasCurtainsE() const;

    bool HasShuttersN() const;
    bool HasShuttersS() const;
    bool HasShuttersW() const;
    bool HasShuttersE() const;

    void ReplaceFloor(BuildingTileEntry *tile, int offset);
    void ReplaceWall(BuildingTileEntry *tile, SquareSection section, const WallInfo &wallInfo);
    void ReplaceDoor(BuildingTileEntry *tile, SquareSection section, int offset, Tiled::MapRotation rotation);
    void ReplaceFrame(BuildingTileEntry *tile, SquareSection section, int offset, Tiled::MapRotation rotation);
    void ReplaceWindow(BuildingTileEntry *tile, SquareSection section, int offset, Tiled::MapRotation rotation);
    void ReplaceCurtains(Window *window, SquareSection section, int offset, Tiled::MapRotation rotation);
    void ReplaceShutters(Window *window, int offset, Tiled::MapRotation rotation);
    void ReplaceFurniture(BuildingTileEntry *tile, int offset = 0);
    void ReplaceFurniture(BuildingTile *btile, SquareSection sectionMin, SquareSection sectionMax, Tiled::MapRotation rotation);
    void ReplaceRoof(BuildingTileEntry *tile, int offset = 0);
    void ReplaceRoofCap(BuildingTileEntry *tile, int offset = 0);
    void ReplaceRoofTop(BuildingTileEntry *tile, int offset);
    void ReplaceFloorGrime(BuildingTileEntry *grimeTile);
    void ReplaceFloorGrimeES(BuildingTileEntry *grimeTile);
    void ReplaceWallGrime(BuildingTileEntry *grimeTile, const BuildingCell &userTileWall, const BuildingCell &userTileWall2);
    void ReplaceWallGrimeES(BuildingTileEntry *grimeTile, const BuildingCell &userTileWall3, const BuildingCell &userTileWall4);
    void ReplaceWallTrim();
    void ReplaceWallTrimES();
};

class BuildingFloor
{
public:
    QVector<QVector<BuildingSquare> > squares;

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
    WallObject *GetWallObjectAt(int x, int y, BuildingObject::Direction dir);

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

    BuildingCell grimeAt(const QString &layerName, int x, int y) const;
    FloorTileGrid *grimeAt(const QString &layerName, const QRect &r);
    FloorTileGrid *grimeAt(const QString &layerName, const QRect &r, const QRegion &rgn);

    QMap<QString,FloorTileGrid*> grimeClone() const;

    QMap<QString,FloorTileGrid*> setGrime(const QMap<QString,FloorTileGrid*> &grime);
    void setGrime(const QString &layerName, int x, int y, const BuildingCell &tileName);
    void setGrime(const QString &layerName, const QPoint &p, const FloorTileGrid *other);
    void setGrime(const QString &layerName, const QRegion &rgn, const BuildingCell &tileName);
    void setGrime(const QString &layerName, const QRegion &rgn, const QPoint &pos, const FloorTileGrid *other);

    bool hasUserTiles() const;
    bool hasUserTiles(const QString &layerName);

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
    QList<RoofObject*> mFlatRoofsWithDepthThree;
    QList<Stairs*> mStairs;
};

} // namespace BuildingEditor

class TileDefFile;

namespace Tiled {
namespace Internal {
class FileSystemWatcher;

class TileDefWatcher : public QObject
{
        Q_OBJECT
public:
    TileDefWatcher();

    void check();

public slots:
    void fileChanged(const QString &path);

public:
    Tiled::Internal::FileSystemWatcher *mWatcher;
    TileDefFile *mTileDefFile;
    bool tileDefFileChecked;
    bool watching;
};

}
}

namespace BuildingEditor {
extern Tiled::Internal::TileDefWatcher *getTileDefWatcher();
}

#endif // BUILDINGFLOOR_H
