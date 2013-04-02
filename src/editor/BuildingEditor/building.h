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

#ifndef BUILDING_H
#define BUILDING_H

#include <QList>
#include <QRect>
#include <QString>
#include <QVector>

namespace BuildingEditor {

class BuildingFloor;
class BuildingObject;
class BuildingTemplate;
class BuildingTileEntry;
class FurnitureTiles;
class Room;

class Building
{
public:
    enum Tiles {
        ExteriorWall,
        Door,
        DoorFrame,
        Window,
        Curtains,
        Stairs,
        RoofCap,
        RoofSlope,
        RoofTop,
        GrimeWall,
        TileCount
    };

    Building(int width, int height, BuildingTemplate *btemplate = 0);

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QSize size() const { return QSize(mWidth, mHeight); }

    QRect bounds() const { return QRect(0, 0, mWidth, mHeight); }

    const QList<BuildingFloor*> &floors() const
    { return mFloors; }

    BuildingFloor *floor(int index);

    int floorCount() const
    { return mFloors.count(); }

    void insertFloor(int index, BuildingFloor *floor);
    BuildingFloor *removeFloor(int index);

    QList<BuildingObject*> objects() const;

    const QList<Room*> &rooms() const
    { return mRooms; }

    Room *room(int index) const
    { return mRooms.at(index); }

    int roomCount() const
    { return mRooms.count(); }

    int indexOf(Room *room) const
    { return mRooms.indexOf(room); }

    void insertRoom(int index, Room *room);
    Room *removeRoom(int index);

    void setTile(int n, BuildingTileEntry *entry);
    void setTiles(const QVector<BuildingTileEntry*> &tiles);

    const QVector<BuildingTileEntry*> &tiles() const
    { return mTiles; }

    BuildingTileEntry *tile(int n) const
    { return mTiles[n]; }

    void setUsedTiles(const QList<BuildingTileEntry*> &tiles)
    { mUsedTiles = tiles; }

    const QList<BuildingTileEntry*> &usedTiles() const
    { return mUsedTiles; }

    void setUsedFurniture(const QList<FurnitureTiles*> &tiles)
    { mUsedFurniture = tiles; }

    const QList<FurnitureTiles*> &usedFurniture() const
    { return mUsedFurniture; }

    QString enumToString(int n);
    int categoryEnum(int n);

    BuildingTileEntry *exteriorWall() const
    { return mTiles[ExteriorWall]; }

    void setExteriorWall(BuildingTileEntry *entry)
    { mTiles[ExteriorWall] = entry; }

    BuildingTileEntry *doorTile() const
    { return mTiles[Door]; }

    void setDoorTile(BuildingTileEntry *entry)
    { mTiles[Door] = entry; }

    BuildingTileEntry *doorFrameTile() const
    { return mTiles[DoorFrame]; }

    void setDoorFrameTile(BuildingTileEntry *entry)
    { mTiles[DoorFrame] = entry; }

    BuildingTileEntry *windowTile() const
    { return mTiles[Window]; }

    void setWindowTile(BuildingTileEntry *entry)
    { mTiles[Window] = entry; }

    BuildingTileEntry *curtainsTile() const
    { return mTiles[Curtains]; }

    void setCurtainsTile(BuildingTileEntry *entry)
    { mTiles[Curtains] = entry; }

    BuildingTileEntry *stairsTile() const
    { return mTiles[Stairs]; }

    void setStairsTile(BuildingTileEntry *entry)
    { mTiles[Stairs] = entry; }

    BuildingTileEntry *roofCapTile() const
    { return mTiles[RoofCap]; }

    void setRoofCapTile(BuildingTileEntry *entry)
    { mTiles[RoofCap] = entry; }

    BuildingTileEntry *roofSlopeTile() const
    { return mTiles[RoofSlope]; }

    void setRoofSlopeTile(BuildingTileEntry *entry)
    { mTiles[RoofSlope] = entry; }

    BuildingTileEntry *roofTopTile() const
    { return mTiles[RoofTop]; }

    void setRoofTopTile(BuildingTileEntry *entry)
    { mTiles[RoofTop] = entry; }

    void resize(const QSize &newSize);
    void rotate(bool right);
    void flip(bool horizontal);

    QStringList tilesetNames() const;

private:
    int mWidth, mHeight;
    QList<BuildingFloor*> mFloors;
    QList<Room*> mRooms;
    QVector<BuildingTileEntry*> mTiles;
    QList<BuildingTileEntry*> mUsedTiles;
    QList<FurnitureTiles*> mUsedFurniture;
};

} // namespace BuildingEditor

#endif // BUILDING_H
