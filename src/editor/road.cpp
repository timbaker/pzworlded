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

#include "road.h"

Road::Road(World *world, int x1, int y1, int x2, int y2, int width, int style)
    : mWorld(world)
    , mStart(x1, y1)
    , mEnd(x2, y2)
    , mWidth(width)
    , mStyle(style)
    , mTrafficLines(0)
{
    Q_ASSERT(mWidth > 0);
    mTrafficLines = RoadTemplates::instance()->nullTrafficLines();
}

void Road::setCoords(const QPoint &start, const QPoint &end)
{
    mStart = start;
    mEnd = end;
}

void Road::setWidth(int newWidth)
{
    Q_ASSERT(newWidth > 0);
    mWidth = newWidth;
}

QRect Road::bounds() const
{
    return startBounds() | endBounds();
}

QRect Road::startBounds() const
{
    return QRect(x1() - mWidth / 2, y1() - mWidth / 2, mWidth, mWidth);
}

QRect Road::endBounds() const
{
    return QRect(x2() - mWidth / 2, y2() - mWidth / 2, mWidth, mWidth);
}

Road::RoadOrient Road::orient() const
{
    if (x1() == x2()) {
        if (y1() < y2())
            return NorthSouth;
        else
            return SouthNorth;
    } else {
        if (x1() < x2())
            return WestEast;
        else
            return EastWest;
    }
}

/////

RoadTemplates *RoadTemplates::mInstance = 0;

RoadTemplates *RoadTemplates::instance()
{
    if (!mInstance)
        mInstance = new RoadTemplates();
    return mInstance;
}

void RoadTemplates::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

TrafficLines *RoadTemplates::findLines(const QString &name)
{
    foreach (TrafficLines *lines, mTrafficLines) {
        if (lines->name == name)
            return lines;
    }

    return mNullTrafficLines;
}
