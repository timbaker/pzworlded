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

#include "building.h"

#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <QSet>

using namespace BuildingEditor;

Building::Building(int width, int height, BuildingTemplate *btemplate) :
    mWidth(width),
    mHeight(height),
    mTiles(TileCount)
{
    Q_ASSERT(TileCount == BuildingTemplate::TileCount);

    if (btemplate) {
        mTiles = btemplate->tiles();
        foreach (Room *room, btemplate->rooms())
            insertRoom(mRooms.count(), new Room(room));
        mUsedTiles = btemplate->usedTiles();
        mUsedFurniture = btemplate->usedFurniture();
    } else {
        for (int i = 0; i < TileCount; i++)
            mTiles[i] = BuildingTilesMgr::instance()->defaultCategoryTile(categoryEnum(i));
    }
}

BuildingFloor *Building::floor(int index)
{
    if (index >= 0 && index < mFloors.size())
        return mFloors.at(index);
    return 0;
}

void Building::insertFloor(int index, BuildingFloor *floor)
{
    mFloors.insert(index, floor);
    for (int i = index; i < mFloors.count(); i++)
        mFloors[i]->setLevel(i);
}

BuildingFloor *Building::removeFloor(int index)
{
    BuildingFloor *floor = mFloors.takeAt(index);
    for (int i = index; i < mFloors.count(); i++)
        mFloors[i]->setLevel(i);
    return floor;
}

QList<BuildingObject *> Building::objects() const
{
    QList<BuildingObject *> ret;
    foreach (BuildingFloor *floor, mFloors)
        ret += floor->objects();
    return ret;
}

void Building::insertRoom(int index, Room *room)
{
    mRooms.insert(index, room);
}

Room *Building::removeRoom(int index)
{
    return mRooms.takeAt(index);
}

void Building::setTile(int n, BuildingTileEntry *entry)
{
    if (entry)
        entry = entry->asCategory(categoryEnum(n));

    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();

    mTiles[n] = entry;
}

void Building::setTiles(const QVector<BuildingTileEntry *> &tiles)
{
    for (int i = 0; i < tiles.size(); i++)
        setTile(i, tiles[i]);
}

QString Building::enumToString(int n)
{
    return BuildingTemplate::enumToString(n);
}

int Building::categoryEnum(int n)
{
    return BuildingTemplate::categoryEnum(n);
}

void Building::resize(const QSize &newSize)
{
    mWidth = newSize.width();
    mHeight = newSize.height();
}

void Building::rotate(bool right)
{
    Q_UNUSED(right)
    qSwap(mWidth, mHeight);
}

void Building::flip(bool horizontal)
{
    Q_UNUSED(horizontal)
}

QStringList Building::tilesetNames() const
{
    QSet<BuildingTileEntry*> entries;
    foreach (BuildingTileEntry *entry, tiles()) {
        if (entry && !entry->isNone())
            entries += entry;
    }
    foreach (Room *room, rooms()) {
        foreach (BuildingTileEntry *entry, room->tiles()) {
            if (entry && !entry->isNone())
                entries += entry;
        }
    }
    QSet<BuildingTile*> btiles;

    foreach (BuildingTileEntry *entry, usedTiles()) {
        if (entry && !entry->isNone())
            entries += entry;
    }
    foreach (FurnitureTiles *ftiles, usedFurniture()) {
        foreach (FurnitureTile *ftile, ftiles->tiles()) {
            if (!ftile || ftile->isEmpty())
                continue;
            foreach (BuildingTile *btile, ftile->tiles()) {
                if (btile && !btile->isNone() && !btile->mTilesetName.isEmpty())
                    btiles += btile;
            }
        }
    }

    foreach (BuildingTileEntry *entry, entries) {
        foreach (BuildingTile *btile, entry->mTiles) {
            if (!btile->mTilesetName.isEmpty())
                btiles += btile;
        }
    }

    QSet<QString> ret;

    foreach (BuildingFloor *floor, floors()) {
        foreach (BuildingObject *object, floor->objects())
            btiles |= object->buildingTiles();
        foreach (QString layerName, floor->grimeLayers()) {
            for (int y = 0; y < floor->height(); y++) {
                for (int x = 0; x < floor->width(); x++) {
                    QString tileName = floor->grime()[layerName]->at(x, y);
                    if (tileName.isEmpty())
                        continue;
                    QString tilesetName;
                    int index;
                    if (BuildingTilesMgr::instance()->parseTileName(tileName,
                                                                    tilesetName,
                                                                    index))
                        ret += tilesetName;
                }
            }
        }
    }

    foreach (BuildingTile *btile, btiles) {
        if (!btile->mTilesetName.isEmpty())
            ret += btile->mTilesetName;
    }

    return ret.toList();
}
