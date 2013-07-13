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

#include "mapbuildings.h"

#include "mapcomposite.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"

#include <qmath.h>

using namespace Tiled;

MapBuildings::MapBuildings()
{
}

MapBuildings::~MapBuildings()
{
    foreach (int level, mRoomRectsByLevel.keys())
        qDeleteAll(mRoomRectsByLevel[level]);
    qDeleteAll(mBuildings);
    qDeleteAll(mRooms);
}

void MapBuildings::calculate(MapComposite *mc)
{
    extractRoomRects(mc);

    qDeleteAll(mRooms);
    mRooms.clear();

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    foreach (int level, mRoomRectsByLevel.keys()) {
        QList<MapBuildingsNS::RoomRect*> rrList = mRoomRectsByLevel[level];
        foreach (MapBuildingsNS::RoomRect *rr, rrList) {
            if (rr->room == 0) {
                rr->room = new MapBuildingsNS::Room(rr->nameWithoutSuffix(),
                                                    rr->floor);
                rr->room->rects += rr;
                mRooms += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#')))
                continue;
            foreach (MapBuildingsNS::RoomRect *comp, rrList) {
                if (comp == rr)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != 0) {
                        MapBuildingsNS::Room *room = comp->room;
                        foreach (MapBuildingsNS::RoomRect *rr2, room->rects) {
                            Q_ASSERT(rr2->room == room);
                            Q_ASSERT(!rr->room->rects.contains(rr2));
                            rr2->room = rr->room;
                        }
                        rr->room->rects += room->rects;
                        mRooms.removeOne(room);
                        delete room;
                    } else {
                        comp->room = rr->room;
                        rr->room->rects += comp;
                        Q_ASSERT(rr->room->rects.count(comp) == 1);
                    }
                }
            }
        }
    }

    qDeleteAll(mBuildings);
    mBuildings.clear();

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    foreach (MapBuildingsNS::Room *r, mRooms) {
        if (r->building == 0) {
            r->building = new MapBuildingsNS::Building();
            mBuildings += r->building;
            r->building->RoomList += r;
        }
        foreach (MapBuildingsNS::Room *comp, mRooms) {
            if (comp == r)
                continue;
            if (r->building == comp->building)
                continue;

            if (r->inSameBuilding(comp)) {
                if (comp->building != 0) {
                    MapBuildingsNS::Building *b = comp->building;
                    foreach (MapBuildingsNS::Room *r2, b->RoomList) {
                        Q_ASSERT(r2->building == b);
                        Q_ASSERT(!r->building->RoomList.contains(r2));
                        r2->building = r->building;
                    }
                    r->building->RoomList += b->RoomList;
                    mBuildings.removeOne(b);
                    delete b;
                } else {
                    comp->building = r->building;
                    r->building->RoomList += comp;
                    Q_ASSERT(r->building->RoomList.count(comp) == 1);
                }
            }
        }
    }
}

void MapBuildings::extractRoomRects(MapComposite *mapComposite)
{
    foreach (int level, mRoomRectsByLevel.keys())
        qDeleteAll(mRoomRectsByLevel[level]);
    mRoomRectsByLevel.clear();

    foreach (MapComposite *mc, mapComposite->maps()) {
        int ox = mc->originRecursive().x();
        int oy = mc->originRecursive().y();
        int rootLevel = mc->levelRecursive();
        for (int level = 0; level < mc->maxLevel(); level++) {
            QString layerName = QString::fromLatin1("%1_RoomDefs").arg(level);
            int index = mc->map()->indexOfLayer(layerName, Layer::ObjectGroupType);
            if (index >= 0) {
                QList<MapObject*> mapObjects = mc->map()->layerAt(index)->asObjectGroup()->objects();
                foreach (MapObject *mapObject, mapObjects) {
                    int x = qFloor(mapObject->x());
                    int y = qFloor(mapObject->y());
                    int w = qCeil(mapObject->x() + mapObject->width()) - x;
                    int h = qCeil(mapObject->y() + mapObject->height()) - y;
                    x += mc->orientAdjustTiles().x() * level;
                    y += mc->orientAdjustTiles().y() * level;
                    mRoomRectsByLevel[level + rootLevel] +=
                            new MapBuildingsNS::RoomRect(
                                mapObject->name(),
                                x + ox, y + oy,
                                level + rootLevel,
                                w, h);
                }
            }
        }
    }
}

MapBuildingsNS::Room *MapBuildings::roomAt(const QPoint &pos, int level)
{
    foreach (MapBuildingsNS::RoomRect *rr, mRoomRectsByLevel[level]) {
        if (rr->bounds().contains(pos))
            return rr->room;
    }
    return 0;
}
