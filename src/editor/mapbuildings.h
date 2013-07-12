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

class MapComposite;

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
        , room(0)
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
    Room *room;
};

class Room
{
public:
    Room(const QString &name, int level)
        : floor(level)
        , name(name)
        , building(0)
    {

    }

    ~Room()
    {
    }

    bool inSameBuilding(Room *comp)
    {
        foreach (RoomRect *rr, rects) {
            foreach (RoomRect *rr2, comp->rects) {
                if (rr->isAdjacent(rr2))
                    return true;
            }
        }
        return false;
    }

    QRegion region()
    {
        QRegion ret;
        foreach (RoomRect *rr, rects)
            ret += rr->bounds();
        return ret;
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
        foreach (Room *room, RoomList) {
            foreach (RoomRect *rr, room->rects)
                ret += rr->bounds().adjusted(0, 0, 1, 1);
        }
        return ret;
    }

    QList<Room*> RoomList;
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
    QMap<int,QList<MapBuildingsNS::RoomRect*> > mRoomRectsByLevel;
};

#endif // MAPBUILDINGS_H
