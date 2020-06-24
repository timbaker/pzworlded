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
#include "mapmanager.h"

#include "BuildingEditor/roofhiding.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"

#include <qmath.h>
#include <QFileInfo>

using namespace Tiled;

MapBuildings::MapBuildings()
{
}

MapBuildings::~MapBuildings()
{
    qDeleteAll(mRoomRectsByLevel);
    qDeleteAll(mBuildings);
    qDeleteAll(mRooms);
}

#include <QDebug>
#include <QElapsedTimer>

void MapBuildings::calculate(MapComposite *mc)
{
    QElapsedTimer elapsed;

    elapsed.start();
    extractRoomRects(mc);
    qDebug() << "MapBuildings: extractRoomRects took" << elapsed.elapsed() << "ms";

    qDeleteAll(mRooms);
    mRooms.clear();

    elapsed.restart();

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    QList<MapBuildingsNS::RoomRect*> overlapping;
    for (MapBuildingsNS::RoomRectsForLevel* rr4L : mRoomRectsByLevel) {
        const QList<MapBuildingsNS::RoomRect*>& rrList = rr4L->mRects;
        for (MapBuildingsNS::RoomRect *rr : rrList) {
            if (rr->room == nullptr) {
                rr->room = new MapBuildingsNS::Room(rr->nameWithoutSuffix(),
                                                    rr->floor);
                rr->room->rects += rr;
                mRooms += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#')))
                continue;
            overlapping.clear();
            rr4L->overlapping(rr->bounds().adjusted(-1, -1, 1, 1), overlapping);
            for (MapBuildingsNS::RoomRect *comp : overlapping) {
                if (comp == rr)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != nullptr) {
                        MapBuildingsNS::Room *room = comp->room;
                        for (MapBuildingsNS::RoomRect *rr2 : room->rects) {
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

    qDebug() << "MapBuildings: merge took" << elapsed.elapsed() << "ms";
    elapsed.restart();

    mRoomLookup.clear();
    for (MapBuildingsNS::Room* r : mRooms) {
        mRoomLookup.addRoom(r);
    }

    qDebug() << "MapBuildings: mRoomLookup.addRoom() took" << elapsed.elapsed() << "ms";
    elapsed.restart();

    qDeleteAll(mBuildings);
    mBuildings.clear();

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    QList<MapBuildingsNS::Room*> overlappingRooms;
    for (MapBuildingsNS::Room *r : mRooms) {
        if (r->building == nullptr) {
            r->building = new MapBuildingsNS::Building();
            mBuildings += r->building;
            r->building->RoomList += r;
        }
        overlappingRooms.clear();
        mRoomLookup.overlapping(r->bounds().adjusted(-1, -1, 1, 1), overlappingRooms);
        for (MapBuildingsNS::Room *comp : overlappingRooms) {
            if (comp == r)
                continue;
            if (r->building == comp->building)
                continue;

            if (r->inSameBuilding(comp)) {
                if (comp->building != nullptr) {
                    MapBuildingsNS::Building *b = comp->building;
                    for (MapBuildingsNS::Room *r2 : b->RoomList) {
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

    qDebug() << "MapBuildings: merge rooms into buildings took" << elapsed.elapsed() << "ms";
    elapsed.restart();
}

namespace {
    bool isAdjacentMap(MapComposite* mc)
    {
        if (mc == nullptr)
            return false;
        if (mc->isAdjacentMap())
            return true;
        return isAdjacentMap(mc->parent());
    };
}

void MapBuildings::extractRoomRects(MapComposite *mapComposite)
{
    qDeleteAll(mRoomRectsByLevel);
    mRoomRectsByLevel.clear();

    for (MapComposite *mc : mapComposite->maps()) {
        if (isAdjacentMap(mc))
            continue;
        if (!mc->isGroupVisible() || !mc->isVisible())
            continue;
        int ox = mc->originRecursive().x();
        int oy = mc->originRecursive().y();
        int rootLevel = mc->levelRecursive();
        for (int level = 0; level <= mc->maxLevel(); level++) {
            QString layerName = QString::fromLatin1("%1_RoomDefs").arg(level);
            int index = mc->map()->indexOfLayer(layerName, Layer::ObjectGroupType);
            if (index >= 0) {
                const QList<MapObject*> mapObjects = mc->map()->layerAt(index)->asObjectGroup()->objects();
                for (MapObject *mapObject : mapObjects) {
                    if (BuildingEditor::RoofHiding::isEmptyOutside(mapObject->name()))
                        continue;
                    int x = qFloor(mapObject->x());
                    int y = qFloor(mapObject->y());
                    int w = qCeil(mapObject->x() + mapObject->width()) - x;
                    int h = qCeil(mapObject->y() + mapObject->height()) - y;
                    x += mc->orientAdjustTiles().x() * level;
                    y += mc->orientAdjustTiles().y() * level;
                    MapBuildingsNS::RoomRect *rr =
                            new MapBuildingsNS::RoomRect(
                                mapObject->name(),
                                x + ox, y + oy,
                                level + rootLevel,
                                w, h);
                    rr->buildingName = QFileInfo(mc->mapInfo()->path()).fileName();
                    if (mRoomRectsByLevel[level + rootLevel] == nullptr) {
                        mRoomRectsByLevel[level + rootLevel] = new MapBuildingsNS::RoomRectsForLevel();
                    }
                    mRoomRectsByLevel[level + rootLevel]->mRects += rr;
                }
            }
        }
    }

    for (MapBuildingsNS::RoomRectsForLevel* rr4L : mRoomRectsByLevel) {
        for (MapBuildingsNS::RoomRect *rr : rr4L->mRects) {
            rr4L->addRect(rr);
        }
    }
}

MapBuildingsNS::Room *MapBuildings::roomAt(const QPoint &pos, int level)
{
    int x = pos.x() / 10;
    int y = pos.y() / 10;
    x = clamp(x, 0, 30-1);
    y = clamp(y, 0, 30-1);

    for (MapBuildingsNS::Room* r : mRoomLookup.mGrid[x + y * 30]) {
        if ((r->floor == level) && r->contains(pos)) {
            return r;
        }
    }
    /*
    for (MapBuildingsNS::Room *r : mRooms) {
        if (r->floor != level) {
            continue;
        }
        for (MapBuildingsNS::RoomRect *rr : r->rects) {
            if (rr->bounds().contains(pos)) {
                return rr->room;
            }
        }
    }
    */
    /*
    MapBuildingsNS::RoomRectsForLevel* rr4L = mRoomRectsByLevel[level];
    if (rr4L == nullptr) {
        return nullptr;
    }

    for (MapBuildingsNS::RoomRect *rr : mRoomRectsByLevel[level]->mRects) {
        if (rr->bounds().contains(pos)) {
            return rr->room;
        }
    }
    */
    return nullptr;
}
