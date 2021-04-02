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

#include "buildingroomdef.h"

#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtiles.h"

using namespace BuildingEditor;

BuildingRoomDefecator::BuildingRoomDefecator(BuildingFloor *floor, Room *room) :
    mFloor(floor),
    mRoom(room)
{
    for (int y = 0; y < floor->height(); y++) {
        for (int x = 0; x < floor->width(); x++) {
            int wid = 0;
            while (floor->GetRoomAt(x + wid, y) == room)
                wid++;
            if (wid > 0) {
                mRoomRegion += QRect(x, y, wid, 1);
                x += wid;
            }
        }
    }

    initWallObjects();
}

void BuildingRoomDefecator::defecate()
{
    for (const QRect &r : mRoomRegion) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (!isValidPos(x, y))
                    continue;
                if (!didTile(x, y))
                    addTile(x, y);
            }
        }
    }
}

void BuildingRoomDefecator::initWallObjects()
{
    foreach (BuildingObject *object, mFloor->objects()) {
        if (WallObject *wall = object->asWall()) {
            if (wall->tile(WallObject::TileInterior)->isNone())
                continue;
            if (wall->isN())
                mNorthWalls += wall->bounds();
            else
                mWestWalls += wall->bounds();
        }
    }
}

bool BuildingRoomDefecator::didTile(int x, int y)
{
    foreach (QRegion rgn, mRegions) {
        if (rgn.contains(QPoint(x, y)))
            return true;
    }
    return false;
}

bool BuildingRoomDefecator::isInRoom(int x, int y)
{
    return mFloor->GetRoomAt(x, y) == mRoom;
//    return mRoomRegion.contains(QPoint(x, y));
}

bool BuildingRoomDefecator::shouldVisit(int x1, int y1, int x2, int y2)
{
    if (isValidPos(x1, y1) && isValidPos(x2, y2) && !didTile(x2, y2)) {
        if ((x2 < x1) && isWestWall(x1, y1))
            return false; // west wall between
        if ((x2 > x1) && isWestWall(x2, y2))
            return false; // west wall between
        if ((y2 < y1) && isNorthWall(x1, y1))
            return false; // north wall between
        if ((y2 > y1) && isNorthWall(x2, y2))
            return false; // north wall between
        return true;
    }
    return false;
}

void BuildingRoomDefecator::addTile(int x, int y)
{
    BuildingRoomDefecatorFill fill(this);
    fill.floodFillScanlineStack(x, y);
    if (!fill.mRegion.isEmpty())
        mRegions += fill.mRegion;
}

bool BuildingRoomDefecator::isValidPos(int x, int y)
{
    return mFloor->GetRoomAt(x, y) == mRoom;
//    return mRoomRegion.contains(QPoint(x, y));
}

bool BuildingRoomDefecator::isWestWall(int x, int y)
{
    if (!isValidPos(x, y)) return false;
    if (isInRoom(x, y) && !isInRoom(x-1,y))
//    if (mRoomRegion.contains(QPoint(x, y)) && !mRoomRegion.contains(QPoint(x-1,y)))
        return true;
    foreach (QRect wallRect, mWestWalls) {
        if (wallRect.contains(x, y))
            return true;
    }
    return false;
}

bool BuildingRoomDefecator::isNorthWall(int x, int y)
{
    if (!isValidPos(x, y)) return false;
    if (isInRoom(x, y) && !isInRoom(x, y-1))
//    if (mRoomRegion.contains(QPoint(x, y)) && !mRoomRegion.contains(QPoint(x,y-1)))
        return true;
    foreach (QRect wallRect, mNorthWalls) {
        if (wallRect.contains(x, y))
            return true;
    }
    return false;
}

/////

BuildingRoomDefecatorFill::BuildingRoomDefecatorFill(BuildingRoomDefecator *rd) :
    mDefecator(rd)
{

}

// Taken from http://lodev.org/cgtutor/floodfill.html#Recursive_Scanline_Floodfill_Algorithm
// Copyright (c) 2004-2007 by Lode Vandevenne. All rights reserved.
void BuildingRoomDefecatorFill::floodFillScanlineStack(int x, int y)
{
    emptyStack();

    int y1;
    bool spanLeft, spanRight;

    if (!push(x, y)) return;

    while (pop(x, y)) {
        y1 = y;
        while (shouldVisit(x, y1, x, y1 - 1))
            y1--;
        spanLeft = spanRight = false;
        QRect r;
        int py = y;
        while (shouldVisit(x, py, x, y1)) {
            mRegion += QRect(x, y1, 1, 1);
            if (!spanLeft && x > 0 && shouldVisit(x, y1, x - 1, y1)) {
                if (!push(x - 1, y1)) return;
                spanLeft = true;
            }
            else if (spanLeft && x > 0 && !shouldVisit(x, y1, x - 1, y1)) {
                spanLeft = false;
            } else if (spanLeft && x > 0 && !shouldVisit(x - 1, y1 - 1, x - 1, y1)) {
                // North wall splits the span.
                if (!push(x - 1, y1)) return;
            }
            if (!spanRight && (x < mDefecator->mFloor->width() - 1) && shouldVisit(x, y1, x + 1, y1)) {
                if (!push(x + 1, y1)) return;
                spanRight = true;
            }
            else if (spanRight && (x < mDefecator->mFloor->width() - 1) && !shouldVisit(x, y1, x + 1, y1)) {
                spanRight = false;
            } else if (spanRight && (x < mDefecator->mFloor->width() - 1) && !shouldVisit(x + 1, y1 - 1, x + 1, y1)) {
                // North wall splits the span.
                if (!push(x + 1, y1)) return;
            }
            py = y1;
            y1++;
        }
        if (!r.isEmpty())
            mRegion += r;
    }
}

bool BuildingRoomDefecatorFill::shouldVisit(int x1, int y1, int x2, int y2)
{
    return mDefecator->shouldVisit(x1, y1, x2, y2)
            && !mRegion.contains(QPoint(x2, y2));
}

bool BuildingRoomDefecatorFill::push(int x, int y)
{
    if (stack.size() < 300 * 300) {
        stack += mDefecator->mFloor->height() * x + y;
        return true;
    } else {
        return false;
    }
}

bool BuildingRoomDefecatorFill::pop(int &x, int &y)
{
    if (stack.size() > 0) {
        int p = stack.last();
        x = p / mDefecator->mFloor->height();
        y = p % mDefecator->mFloor->height();
        stack.pop_back();
        return true;
    } else {
        return false;
    }
}

void BuildingRoomDefecatorFill::emptyStack()
{
    stack.resize(0);
}
