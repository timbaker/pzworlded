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

#ifndef MAPBUILDINGS_H
#define MAPBUILDINGS_H

#include <QList>
#include <QMap>
#include <QRegion>
#include <qmath.h>

class MapComposite;

template<typename T>
T clamp(T v, T min, T max)
{
    return qMin(qMax(v, min), max);
};

namespace MapBuildingsNS {

class Building;
class Room;

class RoomRect
{
public:
    RoomRect(const QString &name, int x, int y, int level, int w, int h)
        : x(x)
        , y(y)
        , w(w)
        , h(h)
        , floor(level)
        , name(name)
        , room(nullptr)
    {
    }

    QRect bounds() const
    {
        return QRect(x, y, w, h);
    }

    QPoint topLeft() const { return QPoint(x, y); }
    QPoint topRight() const { return QPoint(x + w, y); }
    QPoint bottomLeft() const { return QPoint(x, y + h); }
    QPoint bottomRight() const { return QPoint(x + w, y + h); }

    bool isAdjacent(RoomRect *comp) const
    {
        QRect a(x - 1, y - 1, w + 2, h + 2);
        QRect b(comp->x, comp->y, comp->w, comp->h);
        return a.intersects(b);
    }

    bool isTouchingCorners(RoomRect *comp) const
    {
        return topLeft() == comp->bottomRight() ||
                topRight() == comp->bottomLeft() ||
                bottomLeft() == comp->topRight() ||
                bottomRight() == comp->topLeft();
    }

    bool inSameRoom(RoomRect *comp) const
    {
        if (floor != comp->floor) return false;
        if (name != comp->name) return false;
        if (!name.contains(QLatin1Char('#'))) return false;
        return isAdjacent(comp) && !isTouchingCorners(comp);
    }

    QString nameWithoutSuffix() const
    {
        int pos = name.indexOf(QLatin1Char('#'));
        if (pos == -1) return name;
        return name.left(pos);
    }

    int x;
    int y;
    int w;
    int h;
    int floor;
    QString name;
    QString buildingName;
    Room *room;
};

class Room
{
public:
    Room(const QString &name, int level)
        : floor(level)
        , name(name)
        , building(nullptr)
    {

    }

    ~Room()
    {
    }

    bool inSameBuilding(Room *comp)
    {
        for (RoomRect *rr : rects) {
            for (RoomRect *rr2 : comp->rects) {
                if (rr->isAdjacent(rr2))
                    return true;
            }
        }
        return false;
    }

    QRect bounds()
    {
        QRect r;
        for (RoomRect *rr : rects) {
            r |= rr->bounds();
        }
        return r;
    }

    QRegion region()
    {
        QRegion ret;
        for (RoomRect *rr : rects) {
            ret += rr->bounds();
        }
        return ret;
    }

    bool contains(const QPoint& pos)
    {
        for (RoomRect *rr : rects) {
            if (rr->bounds().contains(pos)) {
                return true;
            }
        }
        return false;
    }

    int floor;
    QString name;
    Building *building;
    QList<RoomRect*> rects;
};

class Building
{
public:
    ~Building()
    {
    }

    QRegion region()
    {
        QRegion ret;
        for (Room *room : RoomList) {
            for (RoomRect *rr : room->rects)
                ret += rr->bounds().adjusted(0, 0, 1, 1);
        }
        return ret;
    }

    QList<Room*> RoomList;
};

class RoomRectsForLevel
{
public:
    void addRect(RoomRect* rr)
    {
        int xMin = rr->x / 10;
        int yMin = rr->y / 10;
        int xMax = qCeil((rr->x + rr->w) / 10.0);
        int yMax = qCeil((rr->y + rr->h) / 10.0);
        xMin = clamp(xMin, 0, 30-1);
        yMin = clamp(yMin, 0, 30-1);
        xMax = clamp(xMax, 0, 30-1);
        yMax = clamp(yMax, 0, 30-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                mLookup[x + y * 30] += rr;
            }
        }
    }

    void overlapping(const QRect& rect, QList<RoomRect*>& rects) const
    {
        int xMin = rect.x() / 10;
        int yMin = rect.y() / 10;
        int xMax = qCeil((rect.x() + rect.width()) / 10.0);
        int yMax = qCeil((rect.y() + rect.height()) / 10.0);
        xMin = clamp(xMin, 0, 30-1);
        yMin = clamp(yMin, 0, 30-1);
        xMax = clamp(xMax, 0, 30-1);
        yMax = clamp(yMax, 0, 30-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                for (RoomRect* rr : mLookup[x + y * 30]) {
                    if (rects.contains(rr) == false) {
                        rects += rr;
                    }
                }
            }
        }
    }

    QList<RoomRect*> mRects;
    QList<RoomRect*> mLookup[30 * 30];
};

class RoomLookup
{
public:
    void clear()
    {
        for (int i = 0; i < 30 * 30; i++) {
            mGrid[i].clear();
        }
    }

    void addRoom(Room* r)
    {
        QRect bounds = r->bounds();
        int xMin = bounds.x() / 10;
        int yMin = bounds.y() / 10;
        int xMax = qCeil((bounds.x() + bounds.width()) / 10.0);
        int yMax = qCeil((bounds.y() + bounds.height()) / 10.0);
        xMin = clamp(xMin, 0, 30-1);
        yMin = clamp(yMin, 0, 30-1);
        xMax = clamp(xMax, 0, 30-1);
        yMax = clamp(yMax, 0, 30-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                mGrid[x + y * 30] += r;
            }
        }
    }

    void overlapping(const QRect& rect, QList<Room*>& rooms)
    {
        int xMin = rect.x() / 10;
        int yMin = rect.y() / 10;
        int xMax = qCeil((rect.x() + rect.width()) / 10.0);
        int yMax = qCeil((rect.y() + rect.height()) / 10.0);
        xMin = clamp(xMin, 0, 30-1);
        yMin = clamp(yMin, 0, 30-1);
        xMax = clamp(xMax, 0, 30-1);
        yMax = clamp(yMax, 0, 30-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                for (Room* r : mGrid[x + y * 30]) {
                    if (rooms.contains(r) == false) {
                        rooms += r;
                    }
                }
            }
        }
    }

    QList<MapBuildingsNS::Room*> mGrid[30 * 30];
};

} // MapBuildingsNS

class MapBuildings
{
public:
    MapBuildings();
    ~MapBuildings();

    void calculate(MapComposite *mc);
    void extractRoomRects(MapComposite *mapComposite);

    const QList<MapBuildingsNS::Building*> &buildings()
    { return mBuildings; }

    MapBuildingsNS::Room *roomAt(const QPoint &pos, int level);

private:
    QList<MapBuildingsNS::Building*> mBuildings;
    QList<MapBuildingsNS::Room*> mRooms;
    MapBuildingsNS::RoomLookup mRoomLookup;
    QMap<int, MapBuildingsNS::RoomRectsForLevel*> mRoomRectsByLevel;
};

#endif // MAPBUILDINGS_H
