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

#ifndef ROAD_H
#define ROAD_H

#include <QPoint>
#include <QRect>
#include <QList>

class World;

class Road
{
public:
    Road(World *world, int x1, int y1, int x2, int y2, int width, int style);

    World *world() const
    { return mWorld; }

    void setCoords(const QPoint &start, const QPoint &end);
    void setCoords(int x1, int y1, int x2, int y2);

    void setWidth(int newWidth);
    int width() const { return mWidth; }

    QPoint start() const { return mStart; }
    QPoint end() const { return mEnd; }

    int x1() const { return mStart.x(); }
    int y1() const { return mStart.y(); }
    int x2() const { return mEnd.x(); }
    int y2() const { return mEnd.y(); }

    QRect bounds() const;

    bool isVertical() const
    { return mStart.x() == mEnd.x(); }

private:
    World *mWorld;
    QPoint mStart;
    QPoint mEnd;
    int mWidth;
    int mStyle;
};

typedef QList<Road*> RoadList;

#endif // ROAD_H
