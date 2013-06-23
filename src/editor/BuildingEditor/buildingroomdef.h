/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGROOMDEF_H
#define BUILDINGROOMDEF_H

#include <QRegion>
#include <QVector>

namespace BuildingEditor {
class Building;
class BuildingFloor;
class Room;

class BuildingRoomDefecator;
class BuildingRoomDefecatorFill
{
public:
    BuildingRoomDefecatorFill(BuildingRoomDefecator *rd);
    void floodFillScanlineStack(int x, int y);
    QVector<QRect> cleanupRegion();
    bool shouldVisit(int x1, int y1, int x2, int y2);
    bool shouldVisit(int x1, int y1);
    bool push(int x, int y);
    bool pop(int &x, int &y);
    void emptyStack();

    BuildingRoomDefecator *mDefecator;
    QRegion mRegion;
    QVector<int> stack;
};

class BuildingRoomDefecator
{
public:
    BuildingRoomDefecator(BuildingFloor *floor, Room *room);

    void defecate();
    void initWallObjects();
    bool didTile(int x, int y);
    bool isInRoom(int x, int y);
    bool shouldVisit(int x1, int y1, int x2, int y2);
    void addTile(int x, int y);
    bool isValidPos(int x, int y);
    bool isWestWall(int x, int y);
    bool isNorthWall(int x, int y);

    BuildingFloor *mFloor;
    Room *mRoom;
    QRegion mRoomRegion;
    QList<QRegion> mRegions;
    QList<QRect> mWestWalls;
    QList<QRect> mNorthWalls;
};

} // namespace BuildingEditor

#endif // BUILDINGROOMDEF_H
