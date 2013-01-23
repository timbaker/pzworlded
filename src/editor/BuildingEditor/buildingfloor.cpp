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

#include "buildingfloor.h"

#include "building.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

using namespace BuildingEditor;

/////

FloorTileGrid::FloorTileGrid(int width, int height) :
    mWidth(width),
    mHeight(height),
    mCount(0),
    mUseVector(false)
{
}

const QString &FloorTileGrid::at(int index) const
{
    if (mUseVector)
        return mCellsVector[index];
    QHash<int,QString>::const_iterator it = mCells.find(index);
    if (it != mCells.end())
        return *it;
    return mEmptyCell;
}

void FloorTileGrid::replace(int index, const QString &tile)
{
    if (mUseVector) {
        if (!mCellsVector[index].isEmpty() && tile.isEmpty()) mCount--;
        if (mCellsVector[index].isEmpty() && !tile.isEmpty()) mCount++;
        mCellsVector[index] = tile;
        return;
    }
    QHash<int,QString>::iterator it = mCells.find(index);
    if (it == mCells.end()) {
        if (tile.isEmpty())
            return;
        mCells.insert(index, tile);
        mCount++;
    } else if (!tile.isEmpty()) {
        (*it) = tile;
        mCount++;
    } else {
        mCells.erase(it);
        mCount--;
    }
    if (mCells.size() > 300 * 300 / 3)
        swapToVector();
}

void FloorTileGrid::replace(int x, int y, const QString &tile)
{
    Q_ASSERT(contains(x, y));
    replace(y * mWidth + x, tile);
}

bool FloorTileGrid::replace(const QString &tile)
{
    bool changed = false;
    for (int x = 0; x < mWidth; x++) {
        for (int y = 0; y < mHeight; y++) {
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRegion &rgn, const QString &tile)
{
    bool changed = false;
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                if (at(x, y) != tile) {
                    replace(x, y, tile);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRegion &rgn, const QPoint &p,
                            const FloorTileGrid *other)
{
    Q_ASSERT(other->bounds().translated(p).contains(rgn.boundingRect()));
    bool changed = false;
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                QString tile = other->at(x - p.x(), y - p.y());
                if (at(x, y) != tile) {
                    replace(x, y, tile);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRect &r, const QString &tile)
{
    bool changed = false;
    for (int x = r.left(); x <= r.right(); x++) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QPoint &p, const FloorTileGrid *other)
{
    const QRect r = other->bounds().translated(p) & bounds();
    bool changed = false;
    for (int x = r.left(); x <= r.right(); x++) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            QString tile = other->at(x - p.x(), y - p.y());
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

void FloorTileGrid::clear()
{
    if (mUseVector)
        mCellsVector.fill(mEmptyCell);
    else
        mCells.clear();
    mCount = 0;
}

FloorTileGrid *FloorTileGrid::clone()
{
    return new FloorTileGrid(*this);
}

FloorTileGrid *FloorTileGrid::clone(const QRect &r)
{
    FloorTileGrid *klone = new FloorTileGrid(r.width(), r.height());
    const QRect r2 = r & bounds();
    for (int x = r2.left(); x <= r2.right(); x++) {
        for (int y = r2.top(); y <= r2.bottom(); y++) {
            klone->replace(x - r.x(), y - r.y(), at(x, y));
        }
    }
    return klone;
}

FloorTileGrid *FloorTileGrid::clone(const QRect &r, const QRegion &rgn)
{
    FloorTileGrid *klone = new FloorTileGrid(r.width(), r.height());
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds() & r;
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                klone->replace(x - r.x(), y - r.y(), at(x, y));
            }
        }
    }
    return klone;
}

void FloorTileGrid::swapToVector()
{
    Q_ASSERT(!mUseVector);
    mCellsVector.resize(size());
    QHash<int,QString>::const_iterator it = mCells.begin();
    while (it != mCells.end()) {
        mCellsVector[it.key()] = (*it);
        ++it;
    }
    mCells.clear();
    mUseVector = true;
}

/////

BuildingFloor::BuildingFloor(Building *building, int level) :
    mBuilding(building),
    mLevel(level)
{
    int w = building->width();
    int h = building->height();

    mRoomAtPos.resize(w);
    mIndexAtPos.resize(w);
    for (int x = 0; x < w; x++) {
        mRoomAtPos[x].resize(h);
        mIndexAtPos[x].resize(h);
        for (int y = 0; y < h; y++) {
            mRoomAtPos[x][y] = 0;
        }
    }
}

BuildingFloor::~BuildingFloor()
{
    qDeleteAll(mObjects);
}

BuildingFloor *BuildingFloor::floorAbove() const
{
    return isTopFloor() ? 0 : mBuilding->floor(mLevel + 1);
}

BuildingFloor *BuildingFloor::floorBelow() const
{
    return isBottomFloor() ? 0 : mBuilding->floor(mLevel - 1);
}

bool BuildingFloor::isTopFloor() const
{
    return mLevel == mBuilding->floorCount() - 1;
}

bool BuildingFloor::isBottomFloor() const
{
    return mLevel == 0;
}

void BuildingFloor::insertObject(int index, BuildingObject *object)
{
    mObjects.insert(index, object);
}

BuildingObject *BuildingFloor::removeObject(int index)
{
    return mObjects.takeAt(index);
}

BuildingObject *BuildingFloor::objectAt(int x, int y)
{
    foreach (BuildingObject *object, mObjects)
        if (object->bounds().contains(x, y))
            return object;
    return 0;
}

void BuildingFloor::setGrid(const QVector<QVector<Room *> > &grid)
{
    mRoomAtPos = grid;

    mIndexAtPos.resize(mRoomAtPos.size());
    for (int x = 0; x < mIndexAtPos.size(); x++)
        mIndexAtPos[x].resize(mRoomAtPos[x].size());
}

static void ReplaceRoofSlope(RoofObject *ro, const QRect &r,
                        QVector<QVector<BuildingFloor::Square> > &squares,
                        RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->slopeTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            squares[x][y].ReplaceRoof(ro->slopeTiles(), offset);
}

static void ReplaceRoofGap(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            squares[x][y].ReplaceRoofCap(ro->capTiles(), offset);
}

static void ReplaceRoofCap(RoofObject *ro, int x, int y,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p))
        squares[p.x()][p.y()].ReplaceRoofCap(ro->capTiles(), offset);
}

static void ReplaceRoofTop(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingFloor::Square> > &squares)
{
    if (r.isEmpty()) return;
    int offset = 0;
    if (ro->depth() == RoofObject::Zero)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    else if (ro->depth() == RoofObject::One)
        offset = ro->isN() ? BTC_RoofTops::North1 : BTC_RoofTops::West1;
    else if (ro->depth() == RoofObject::Two)
        offset = ro->isN() ? BTC_RoofTops::North2 : BTC_RoofTops::West2;
    else if (ro->depth() == RoofObject::Three)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    QPoint tileOffset = ro->topTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            (ro->depth() == RoofObject::Zero || ro->depth() == RoofObject::Three)
                ? squares[x][y].ReplaceFloor(ro->topTiles(), offset)
                : squares[x][y].ReplaceRoofTop(ro->topTiles(), offset);
}

static void ReplaceRoofCorner(RoofObject *ro, int x, int y,
                              QVector<QVector<BuildingFloor::Square> > &squares,
                              RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->slopeTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p))
        squares[p.x()][p.y()].ReplaceRoof(ro->slopeTiles(), offset);
}

static void ReplaceFurniture(int x, int y,
                             QVector<QVector<BuildingFloor::Square> > &squares,
                             BuildingTile *btile,
                             BuildingFloor::Square::SquareSection section,
                             BuildingFloor::Square::SquareSection section2
                             = BuildingFloor::Square::SectionInvalid)
{
    if (!btile)
        return;
    QRect bounds(0, 0, squares.size() - 1, squares[0].size() - 1);
    if (bounds.contains(x, y))
        squares[x][y].ReplaceFurniture(btile, section, section2);
}

static void ReplaceDoor(Door *door, QVector<QVector<BuildingFloor::Square> > &squares)
{
    int x = door->x(), y = door->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (bounds.contains(x, y)) {
        squares[x][y].ReplaceDoor(door->tile(),
                                  door->isW() ? BTC_Doors::West
                                              : BTC_Doors::North);
        squares[x][y].ReplaceFrame(door->frameTile(),
                                   door->isW() ? BTC_DoorFrames::West
                                               : BTC_DoorFrames::North);
    }
}

static void ReplaceWindow(Window *window, QVector<QVector<BuildingFloor::Square> > &squares)
{
    int x = window->x(), y = window->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (bounds.contains(x, y)) {
        squares[x][y].ReplaceFrame(window->tile(),
                                   window->isW() ? BTC_Windows::West
                                                 : BTC_Windows::North);

        // Window curtains on exterior walls must be *inside* the
        // room.
        if (squares[x][y].mExterior) {
            int dx = window->isW() ? 1 : 0;
            int dy = window->isN() ? 1 : 0;
            if ((x - dx >= 0) && (y - dy >= 0))
                squares[x - dx][y - dy].ReplaceCurtains(window, true);
        } else
            squares[x][y].ReplaceCurtains(window, false);
    }
}

static void ReplaceWallW(int x, int y, BuildingTileEntry *entry, bool exterior,
                         QVector<QVector<BuildingFloor::Square> > &squares)
{
    QRect bounds(0, 0, squares.size() - 1, squares[0].size() - 1);
    if (bounds.contains(x, y))
        squares[x][y].SetWallW(entry, exterior);
}

static void ReplaceWallN(int x, int y, BuildingTileEntry *entry, bool exterior,
                         QVector<QVector<BuildingFloor::Square> > &squares)
{
    QRect bounds(0, 0, squares.size() - 1, squares[0].size() - 1);
    if (bounds.contains(x, y))
        squares[x][y].SetWallN(entry, exterior);
}

void BuildingFloor::LayoutToSquares()
{
    int w = width() + 1;
    int h = height() + 1;
    // +1 for the outside walls;
    squares.resize(w);
    for (int x = 0; x < w; x++)
        squares[x].fill(Square(), h);

    BuildingTileEntry *wtype = 0;

    BuildingTileEntry *exteriorWall;
    QVector<BuildingTileEntry*> interiorWalls;
    QVector<BuildingTileEntry*> floors;

    exteriorWall = mBuilding->exteriorWall();

    foreach (Room *room, mBuilding->rooms()) {
        interiorWalls += room->tile(Room::InteriorWall);
        floors += room->tile(Room::Floor);
    }

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            Room *room = mRoomAtPos[x][y];
            mIndexAtPos[x][y] = room ? mBuilding->indexOf(room) : -1;
        }
    }

    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            // Place N walls...
            if (x < width()) {
                if (y == height() && mIndexAtPos[x][y - 1] >= 0) {
                    squares[x][y].SetWallN(exteriorWall);
                } else if (y < height() && mIndexAtPos[x][y] < 0 && y > 0 && mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallN(exteriorWall);
                } else if (y < height() && (y == 0 || mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) && mIndexAtPos[x][y] >= 0) {
                    squares[x][y].SetWallN(interiorWalls[mIndexAtPos[x][y]], false);
                }
            }
            // Place W walls...
            if (y < height()) {
                if (x == width() && mIndexAtPos[x - 1][y] >= 0) {
                    squares[x][y].SetWallW(exteriorWall);
                } else if (x < width() && mIndexAtPos[x][y] < 0 && x > 0 && mIndexAtPos[x - 1][y] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallW(exteriorWall);
                } else if (x < width() && mIndexAtPos[x][y] >= 0 && (x == 0 || mIndexAtPos[x - 1][y] != mIndexAtPos[x][y])) {
                    squares[x][y].SetWallW(interiorWalls[mIndexAtPos[x][y]], false);
                }
            }
        }
    }

    // Handle WallObjects.
    foreach (BuildingObject *object, mObjects) {
        if (WallObject *wall = object->asWall()) {
#if 1
            int x = wall->x(), y = wall->y();
            if (wall->isN()) {
                QRect r = wall->bounds() & bounds(1, 0);
                for (y = r.top(); y <= r.bottom(); y++) {
                    bool exterior = (x < width()) ? mIndexAtPos[x][y] < 0 : true;
                    squares[x][y].SetWallW(wall->tile(exterior ? 0 : 1), exterior);
                }
            } else {
                QRect r = wall->bounds() & bounds(0, 1);
                for (x = r.left(); x <= r.right(); x++) {
                    bool exterior = (y < height()) ? mIndexAtPos[x][y] < 0 : true;
                    squares[x][y].SetWallN(wall->tile(exterior ? 0 : 1), exterior);
                }
            }
#else
            int x = wall->x(), y = wall->y();
            if (wall->isN()) {
                for (; y < wall->y() + wall->length(); y++) {
                    bool exterior = (x < width()) ? mIndexAtPos[x][y] < 0 : true;
                    squares[x][y].SetWallW(wall->tile(exterior ? 0 : 1), exterior);
                }
            } else {
                for (; x < wall->x() + wall->length(); x++) {
                    bool exterior = (y < height()) ? mIndexAtPos[x][y] < 0 : true;
                    squares[x][y].SetWallN(wall->tile(exterior ? 0 : 1), exterior);
                }
            }
#endif
        }
    }

    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            Square &s = squares[x][y];
            BuildingTileEntry *wallN = s.mWallN.entry;
            BuildingTileEntry *wallW = s.mWallW.entry;
            if (wallN || wallW) {
                if (!wallN) wallN = BuildingTilesMgr::instance()->noneTileEntry();
                if (!wallW) wallW = BuildingTilesMgr::instance()->noneTileEntry();
                if (wallN == wallW) // may be "none"
                    s.ReplaceWall(wallN, Square::WallOrientNW, s.mWallN.exterior);
                else if (wallW->isNone())
                    s.ReplaceWall(wallN, Square::WallOrientN, s.mWallN.exterior);
                else if (wallN->isNone())
                    s.ReplaceWall(wallW, Square::WallOrientW, s.mWallW.exterior);
                else {
                    // Different non-none tiles.
                    s.mEntries[Square::SectionWall] = wallN;
                    s.mWallOrientation = Square::WallOrientN; // must be set before getWallOffset
                    s.mEntryEnum[Square::SectionWall] = s.getWallOffset();
                    s.mExterior = s.mWallN.exterior;

                    s.mEntries[Square::SectionWall2] = wallW;
                    s.mWallOrientation = Square::WallOrientW; // must be set before getWallOffset
                    s.mEntryEnum[Square::SectionWall2] = s.getWallOffset();

                    s.mWallOrientation = Square::WallOrientNW;
                }
            }
        }
    }

    for (int x = 1; x < width() + 1; x++) {
        for (int y = 1; y < height() + 1; y++) {
            if (squares[x][y].mEntries[Square::SectionWall] &&
                    !squares[x][y].mEntries[Square::SectionWall]->isNone())
                continue;
            // Put in the SE piece...
            if ((squares[x][y - 1].IsWallOrient(Square::WallOrientW) ||
                 squares[x][y - 1].IsWallOrient(Square::WallOrientNW)) &&
                    (squares[x - 1][y].IsWallOrient(Square::WallOrientN) ||
                     squares[x - 1][y].IsWallOrient(Square::WallOrientNW))) {
#if 1
                // With WallObjects, there could be 2 different tiles meeting
                // at this SE corner.
                wtype = squares[x][y - 1].mWallW.entry;
#else
                if (x < width() && mIndexAtPos[x][y - 1] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x][y - 1]];
                else if (y < height() &&  mIndexAtPos[x-1][y] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x - 1][y]];
                else
                    wtype = exteriorWall;
#endif
                squares[x][y].ReplaceWall(wtype, Square::WallOrientSE,
                                          squares[x][y - 1].mExterior);
            }
        }
    }

    QList<FurnitureObject*> wallReplacement;

    foreach (BuildingObject *object, mObjects) {
        int x = object->x();
        int y = object->y();
        if (Door *door = object->asDoor()) {
#if 1
            ReplaceDoor(door, squares);
#else
            squares[x][y].ReplaceDoor(door->tile(),
                                      door->isW() ? BTC_Doors::West
                                                  : BTC_Doors::North);
            squares[x][y].ReplaceFrame(door->frameTile(),
                                       door->isW() ? BTC_DoorFrames::West
                                                   : BTC_DoorFrames::North);
#endif
        }
        if (Window *window = object->asWindow()) {
#if 1
            ReplaceWindow(window, squares);
#else
            squares[x][y].ReplaceFrame(window->tile(),
                                       window->isW() ? BTC_Windows::West
                                                     : BTC_Windows::North);

            // Window curtains on exterior walls must be *inside* the
            // room.
            if (squares[x][y].mExterior) {
                int dx = window->isW() ? 1 : 0;
                int dy = window->isN() ? 1 : 0;
                if ((x - dx >= 0) && (y - dy >= 0))
                    squares[x - dx][y - dy].ReplaceCurtains(window, true);
            } else
                squares[x][y].ReplaceCurtains(window, false);
#endif
        }
        if (Stairs *stairs = object->asStairs()) {
            // Stair objects are 5 tiles long but only have 3 tiles.
            if (stairs->isN()) {
                for (int i = 1; i <= 3; i++)
                    ReplaceFurniture(x, y + i, squares,
                                     stairs->tile()->tile(stairs->getOffset(x, y + i)),
                                     Square::SectionFurniture,
                                     Square::SectionFurniture2);
            } else {
                for (int i = 1; i <= 3; i++)
                    ReplaceFurniture(x + i, y, squares,
                                     stairs->tile()->tile(stairs->getOffset(x + i, y)),
                                     Square::SectionFurniture,
                                     Square::SectionFurniture2);
            }
        }
        if (FurnitureObject *fo = object->asFurniture()) {
            FurnitureTile *ftile = fo->furnitureTile()->resolved();
            if (ftile->owner()->layer() == FurnitureTiles::LayerWalls) {
                wallReplacement += fo;
                continue;
            }
            for (int i = 0; i < ftile->size().height(); i++) {
                for (int j = 0; j < ftile->size().width(); j++) {
                    switch (ftile->owner()->layer()) {
                    case FurnitureTiles::LayerWalls:
                        // Handled after all the door/window objects
                        break;
                    case FurnitureTiles::LayerRoofCap: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         Square::SectionRoofCap,
                                         Square::SectionRoofCap2);
                        break;
                    }
                    case FurnitureTiles::LayerWallOverlay:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         Square::SectionWallOverlay,
                                         Square::SectionWallOverlay2);
                        break;
                    case FurnitureTiles::LayerWallFurniture:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         Square::SectionWallFurniture,
                                         Square::SectionWallFurniture2);
                        break;
                    case FurnitureTiles::LayerFrames: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         Square::SectionFrame);
                        break;
                    }
                    case FurnitureTiles::LayerDoors: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         Square::SectionDoor);
                        break;
                    }
                    case FurnitureTiles::LayerFurniture:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         Square::SectionFurniture,
                                         Square::SectionFurniture2);
                        break;
                    case FurnitureTiles::LayerRoof:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         Square::SectionRoof,
                                         Square::SectionRoof2);
                        break;
                    }
                }
            }
        }
        if (RoofObject *ro = object->asRoof()) {
            QRect r = ro->bounds();

            QRect se = ro->southEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                ReplaceRoofSlope(ro, se, squares, RoofObject::SlopePt5S);
                break;
            case RoofObject::One:
                ReplaceRoofSlope(ro, se, squares, RoofObject::SlopeS1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-1), squares, RoofObject::SlopeOnePt5S);
                break;
            case RoofObject::Two:
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-1), squares, RoofObject::SlopeS2);
                break;
            case RoofObject::TwoPoint5:
                ReplaceRoofSlope(ro, se.adjusted(0,2,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,-1), squares, RoofObject::SlopeS2);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-2), squares, RoofObject::SlopeTwoPt5S);
                break;
            case RoofObject::Three:
                ReplaceRoofSlope(ro, se.adjusted(0,2,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,-1), squares, RoofObject::SlopeS2);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-2), squares, RoofObject::SlopeS3);
                break;
            }

            QRect ee = ro->eastEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                ReplaceRoofSlope(ro, ee, squares, RoofObject::SlopePt5E);
                break;
            case RoofObject::One:
                ReplaceRoofSlope(ro, ee, squares, RoofObject::SlopeE1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoofSlope(ro, ee.adjusted(1,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::SlopeOnePt5E);
                break;
            case RoofObject::Two:
                ReplaceRoofSlope(ro, ee.adjusted(1,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::SlopeE2);
                break;
            case RoofObject::TwoPoint5:
                ReplaceRoofSlope(ro, ee.adjusted(2,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(1,0,-1,0), squares, RoofObject::SlopeE2);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-2,0), squares, RoofObject::SlopeTwoPt5E);
                break;
            case RoofObject::Three:
                ReplaceRoofSlope(ro, ee.adjusted(2,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(1,0,-1,0), squares, RoofObject::SlopeE2);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-2,0), squares, RoofObject::SlopeE3);
                break;
            }

            // Corners
            switch (ro->roofType()) {
            case RoofObject::CornerInnerNW:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner1);
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Inner1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeE2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeS2);
                    break;
                case RoofObject::TwoPoint5:
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Inner2);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+2, squares, RoofObject::Inner1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeE3);
                    ReplaceRoofCorner(ro, r.left(), r.top()+2, squares, RoofObject::SlopeE3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+2, squares, RoofObject::SlopeE2);

                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeS3);
                    ReplaceRoofCorner(ro, r.left()+2, r.top(), squares, RoofObject::SlopeS3);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+1, squares, RoofObject::SlopeS2);
                    break;
                }
                break;
            case RoofObject::CornerOuterSW:
                switch (ro->depth()) {
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.bottom(), squares, RoofObject::CornerSW1);
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left(), r.bottom(), squares, RoofObject::CornerSW1);
                    ReplaceRoofCorner(ro, r.left()+1, r.bottom()-1, squares, RoofObject::CornerSW2);

                    ReplaceRoofCorner(ro, r.left()+1, r.bottom(), squares, RoofObject::SlopeS1);
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left(), r.bottom(), squares, RoofObject::CornerSW1);
                    ReplaceRoofCorner(ro, r.left()+1, r.bottom()-1, squares, RoofObject::CornerSW2);
                    ReplaceRoofCorner(ro, r.left()+2, r.bottom()-2, squares, RoofObject::CornerSW3);

                    ReplaceRoofCorner(ro, r.left()+1, r.bottom(), squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+2, r.bottom(), squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+2, r.bottom()-1, squares, RoofObject::SlopeS2);
                    break;
                }
                break;
            case RoofObject::CornerOuterNE:
                switch (ro->depth()) {
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::CornerNE1);
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::CornerNE1);
                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::CornerNE2);

                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::SlopeE1);
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left()+2, r.top(), squares, RoofObject::CornerNE1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::CornerNE2);
                    ReplaceRoofCorner(ro, r.left(), r.top()+2, squares, RoofObject::CornerNE3);

                    ReplaceRoofCorner(ro, r.left()+2, r.top()+1, squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+2, squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+2, squares, RoofObject::SlopeE2);
                    break;
                }
                break;
            case RoofObject::CornerOuterSE:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer1);
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Outer1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeE1);
                    break;
                case RoofObject::TwoPoint5:
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Outer2);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+2, squares, RoofObject::Outer1);

                    ReplaceRoofCorner(ro, r.left()+2, r.top(), squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+1, squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeE2);

                    ReplaceRoofCorner(ro, r.left(), r.top()+2, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+2, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeS2);
                    break;
                }
                break;
            }

            QRect wg = ro->westGap(RoofObject::Three);
            ReplaceRoofGap(ro, wg, squares, RoofObject::CapGapE3);

            QRect ng = ro->northGap(RoofObject::Three);
            ReplaceRoofGap(ro, ng, squares, RoofObject::CapGapS3);

            QRect eg = ro->eastGap(RoofObject::Three);
            ReplaceRoofGap(ro, eg, squares, RoofObject::CapGapE3);

            QRect sg = ro->southGap(RoofObject::Three);
            ReplaceRoofGap(ro, sg, squares, RoofObject::CapGapS3);
#if 0
            // SE corner 'pole'
            if (ro->depth() == RoofObject::Three && eg.isValid() && sg.isValid() &&
                    (eg.adjusted(0,0,0,1).bottomLeft() == sg.adjusted(0,0,1,0).topRight()))
                ReplaceRoofCap(ro, r.right()+1, r.bottom()+1, squares, RoofObject::CapGapE3, 3);
#endif

            // Roof tops with depth of 3 are placed in the floor layer of the
            // floor above.
            if (ro->depth() != RoofObject::Three)
                ReplaceRoofTop(ro, ro->flatTop(), squares);

            // West cap
            if (ro->isCappedW()) {
                switch (ro->roofType()) {
                case RoofObject::PeakWE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.left(), ro->y(), squares, RoofObject::PeakPt5E);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::PeakOnePt5E);
                        ReplaceRoofCap(ro, r.left(), r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.left(), r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.left(), r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.left(), r.top()+2, squares, RoofObject::PeakTwoPt5E);
                        ReplaceRoofCap(ro, r.left(), r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.left(), r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.left(), r.top()+2, squares, RoofObject::CapFallE3);
                        ReplaceRoofCap(ro, r.left(), r.bottom()-2, squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.left(), r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.left(), r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                case RoofObject::SlopeN:
                case RoofObject::CornerOuterNE:
                case RoofObject::CornerInnerSE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapFallE2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.left(), r.top()+2, squares, RoofObject::CapFallE3);
                        break;
                    }
                    break;
                case RoofObject::SlopeS:
                case RoofObject::CornerInnerNE:
                case RoofObject::CornerOuterSE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.left(), r.top()+1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.left(), r.top()+2, squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                }
            }

            // East cap
            if (ro->isCappedE()) {
                switch (ro->roofType()) {
                case RoofObject::PeakWE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.right()+1, ro->y(), squares, RoofObject::PeakPt5E);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::PeakOnePt5E);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::PeakTwoPt5E);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-2, squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                case RoofObject::SlopeN:
                case RoofObject::CornerInnerSW:
                case RoofObject::CornerOuterNW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        break;
                    }
                    break;
                case RoofObject::SlopeS:
                case RoofObject::CornerInnerNW:
                case RoofObject::CornerOuterSW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                }
            }

            // North cap
            if (ro->isCappedN()) {
                switch (ro->roofType()) {
                case RoofObject::PeakNS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::PeakPt5S);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.right(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::PeakOnePt5S);
                        ReplaceRoofCap(ro, r.right(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.right()-1, r.top(), squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.top(), squares, RoofObject::PeakTwoPt5S);
                        ReplaceRoofCap(ro, r.right()-1, r.top(), squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.top(), squares, RoofObject::CapRiseS3);
                        ReplaceRoofCap(ro, r.right()-2, r.top(), squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.right()-1, r.top(), squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                case RoofObject::SlopeW:
                case RoofObject::CornerInnerSE:
                case RoofObject::CornerOuterSW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapRiseS2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.top(), squares, RoofObject::CapRiseS3);
                        break;
                    }
                    break;
                case RoofObject::SlopeE:
                case RoofObject::CornerInnerSW:
                case RoofObject::CornerOuterSE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left()+0, r.top(), squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left()+0, r.top(), squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+1, r.top(), squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+2, r.top(), squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                }
            }

            // South cap
            if (ro->isCappedS()) {
                switch (ro->roofType()) {
                case RoofObject::PeakNS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::PeakPt5S);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::PeakOnePt5S);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.right()-1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::PeakTwoPt5S);
                        ReplaceRoofCap(ro, r.right()-1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        ReplaceRoofCap(ro, r.right()-2, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.right()-1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                case RoofObject::SlopeW:
                case RoofObject::CornerInnerNE:
                case RoofObject::CornerOuterNW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        break;
                    }
                    break;
                case RoofObject::SlopeE:
                case RoofObject::CornerInnerNW:
                case RoofObject::CornerOuterNE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                }
            }
        }
    }

    foreach (FurnitureObject *fo, wallReplacement) {
        FurnitureTile *ftile = fo->furnitureTile()->resolved();
        int x = fo->x(), y = fo->y();
        int dx = 0, dy = 0;
        switch (fo->furnitureTile()->orient()) {
        case FurnitureTile::FurnitureW:
            break;
        case FurnitureTile::FurnitureE:
            dx = 1;
            break;
        case FurnitureTile::FurnitureN:
            break;
        case FurnitureTile::FurnitureS:
            dy = 1;
            break;
#if 0
        case FurnitureTile::FurnitureNW:
            s.mWallOrientation = Square::WallOrientNW;
            break;
        case FurnitureTile::FurnitureSE:
            s.mWallOrientation = Square::WallOrientSE;
            break;
#endif
        }
        for (int i = 0; i < ftile->size().height(); i++) {
            for (int j = 0; j < ftile->size().width(); j++) {
                if (bounds().adjusted(0,0,1,1).contains(x + j + dx, y + i + dy)) {
                    Square &s = squares[x + j + dx][y + i + dy];
                    s.mTiles[Square::SectionWall] = ftile->tile(j, i);
                    s.mEntries[Square::SectionWall] = 0;
                    s.mEntryEnum[Square::SectionWall] = 0;
                    switch (fo->furnitureTile()->orient()) {
                    case FurnitureTile::FurnitureW:
                    case FurnitureTile::FurnitureE:
                        s.mWallOrientation = Square::WallOrientW;
                        break;
                    case FurnitureTile::FurnitureN:
                    case FurnitureTile::FurnitureS:
                        s.mWallOrientation = Square::WallOrientN;
                        break;
                    }
                }
            }
        }
    }

    // Place floors
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (mIndexAtPos[x][y] >= 0)
                squares[x][y].ReplaceFloor(floors[mIndexAtPos[x][y]], 0);
        }
    }

    // Place flat roof tops above roofs on the floor below
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (RoofObject *ro = object->asRoof()) {
                if (ro->depth() == RoofObject::Three)
                    ReplaceRoofTop(ro, ro->flatTop(), squares);
            }
        }
    }

    // Nuke floors that have stairs on the floor below.
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (Stairs *stairs = object->asStairs()) {
                int x = stairs->x(), y = stairs->y();
                if (stairs->isW()) {
                    if (x + 1 < 0 || x + 3 >= width() || y < 0 || y >= height())
                        continue;
                    squares[x+1][y].ReplaceFloor(0, 0);
                    squares[x+2][y].ReplaceFloor(0, 0);
                    squares[x+3][y].ReplaceFloor(0, 0);
                }
                if (stairs->isN()) {
                    if (x < 0 || x >= width() || y + 1 < 0 || y + 3 >= height())
                        continue;
                    squares[x][y+1].ReplaceFloor(0, 0);
                    squares[x][y+2].ReplaceFloor(0, 0);
                    squares[x][y+3].ReplaceFloor(0, 0);
                }
            }
        }
    }

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            Square &sq = squares[x][y];
            Room *room = mRoomAtPos[x][y];
            // Floor Grime
            if (sq.mEntries[Square::SectionWall]
                    && !sq.mEntries[Square::SectionWall]->isNone()
                    && !sq.mExterior
                    && room && room->tile(Room::GrimeFloor)) {
                int e1 = -1, e2 = -1;

                // Handle 2 different wall tiles due to WallObjects.
                int e = sq.mEntryEnum[Square::SectionWall];
                if (sq.mWallOrientation == Square::WallOrientNW)
                    e = BTC_Walls::NorthWest;

                switch (e) {
                case BTC_Walls::West:
                case BTC_Walls::WestWindow: e1 = BTC_GrimeFloor::West; break;
                case BTC_Walls::WestDoor: break;
                case BTC_Walls::North:
                case BTC_Walls::NorthWindow: e2 = BTC_GrimeFloor::North; break;
                case BTC_Walls::NorthDoor: break;
                case BTC_Walls::NorthWest:
                    e1 = BTC_GrimeFloor::West;
                    e2 = BTC_GrimeFloor::North;
                    break;
                case BTC_Walls::SouthEast: e1 = BTC_GrimeFloor::NorthWest; break;
                }
                if (e1 >= 0) {
                    sq.mEntries[Square::SectionFloorGrime] = room->tile(Room::GrimeFloor);
                    sq.mEntryEnum[Square::SectionFloorGrime] = e1;
                }
                if (e2 >= 0) {
                    sq.mEntries[Square::SectionFloorGrime2] = room->tile(Room::GrimeFloor);
                    sq.mEntryEnum[Square::SectionFloorGrime2] = e2;
                }
            }
            // Floor Grime (on furniture in Walls layer)
            if (sq.mTiles[Square::SectionWall]
                    && !sq.mTiles[Square::SectionWall]->isNone()
                    && !sq.mExterior
                    && room && room->tile(Room::GrimeFloor)) {
                int e1 = -1, e2 = -1;
                switch (sq.mWallOrientation) {
                case Square::WallOrientW: e1 = BTC_GrimeFloor::West; break;
                case Square::WallOrientN: e2 = BTC_GrimeFloor::North; break;
                }
                if (e1 >= 0) {
                    sq.mEntries[Square::SectionFloorGrime] = room->tile(Room::GrimeFloor);
                    sq.mEntryEnum[Square::SectionFloorGrime] = e1;
                }
                if (e2 >= 0) {
                    sq.mEntries[Square::SectionFloorGrime2] = room->tile(Room::GrimeFloor);
                    sq.mEntryEnum[Square::SectionFloorGrime2] = e2;
                }
            }
            // Wall Grime
            if (sq.mEntries[Square::SectionWall]
                    && !sq.mEntries[Square::SectionWall]->isNone()
                    && !sq.mExterior
                    && room && room->tile(Room::GrimeWall)) {
                int e = -1;

                // Handle 2 different wall tiles due to WallObjects.
                int wallEnum = sq.mEntryEnum[Square::SectionWall];
                if (sq.mWallOrientation == Square::WallOrientNW)
                    wallEnum = BTC_Walls::NorthWest;

                switch (wallEnum) {
                case BTC_Walls::West: e = BTC_GrimeWall::West; break;
                case BTC_Walls::WestDoor: e = BTC_GrimeWall::WestDoor; break;
                case BTC_Walls::WestWindow: e = BTC_GrimeWall::WestWindow; break;
                case BTC_Walls::North: e = BTC_GrimeWall::North; break;
                case BTC_Walls::NorthDoor: e = BTC_GrimeWall::NorthDoor; break;
                case BTC_Walls::NorthWindow: e = BTC_GrimeWall::NorthWindow; break;
                case BTC_Walls::NorthWest: e = BTC_GrimeWall::NorthWest; break;
                case BTC_Walls::SouthEast: e = BTC_GrimeWall::SouthEast; break;
                }
                if (e >= 0) {
                    sq.mEntries[Square::SectionWallGrime] = room->tile(Room::GrimeWall);
                    sq.mEntryEnum[Square::SectionWallGrime] = e;
                }
            }
            // Wall Grime (on furniture in Walls layer)
            if (sq.mTiles[Square::SectionWall]
                    && !sq.mTiles[Square::SectionWall]->isNone()
                    && !sq.mExterior
                    && room && room->tile(Room::GrimeFloor)) {
                int e = -1;
                switch (sq.mWallOrientation) {
                case Square::WallOrientW: e = BTC_GrimeWall::West; break;
                case Square::WallOrientN: e = BTC_GrimeWall::North; break;
                }
                if (e >= 0) {
                    sq.mEntries[Square::SectionWallGrime] = room->tile(Room::GrimeWall);
                    sq.mEntryEnum[Square::SectionWallGrime] = e;
                }
            }
        }
    }
}

Door *BuildingFloor::GetDoorAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = o->asDoor())
            return door;
    }
    return 0;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = o->asWindow())
            return window;
    }
    return 0;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = o->asStairs())
            return stairs;
    }
    return 0;
}

FurnitureObject *BuildingFloor::GetFurnitureAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (FurnitureObject *fo = o->asFurniture())
            return fo;
    }
    return 0;
}

void BuildingFloor::SetRoomAt(int x, int y, Room *room)
{
    mRoomAtPos[x][y] = room;
}

Room *BuildingFloor::GetRoomAt(const QPoint &pos)
{
    if (!contains(pos))
        return 0;
    return mRoomAtPos[pos.x()][pos.y()];
}

int BuildingFloor::width() const
{
    return mBuilding->width();
}

int BuildingFloor::height() const
{
    return mBuilding->height();
}

QRect BuildingFloor::bounds(int dw, int dh) const
{
    return mBuilding->bounds().adjusted(0, 0, dw, dh);
}

bool BuildingFloor::contains(int x, int y, int dw, int dh)
{
    return bounds().adjusted(0, 0, dw, dh).contains(x, y);
}

bool BuildingFloor::contains(const QPoint &p, int dw, int dh)
{
    return contains(p.x(), p.y(), dw, dh);
}

QVector<QRect> BuildingFloor::roomRegion(Room *room)
{
    QRegion region;
    for (int y = 0; y < height(); y++) {
        for (int x = 0; x < width(); x++) {
            if (mRoomAtPos[x][y] == room)
                region += QRect(x, y, 1, 1);
        }
    }

    // Clean up the region by merging vertically-adjacent rectangles of the
    // same width.
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++) {
        QRect r = rects[i];
        if (!r.isValid()) continue;
        for (int j = 0; j < rects.size(); j++) {
            if (i == j) continue;
            QRect r2 = rects.at(j);
            if (!r2.isValid()) continue;
            if (r2.left() == r.left() && r2.right() == r.right()) {
                if (r.bottom() + 1 == r2.top()) {
                    r.setBottom(r2.bottom());
                    rects[j] = QRect();
                } else if (r.top() == r2.bottom() + 1) {
                    r.setTop(r2.top());
                    rects[j] = QRect();
                }
            }
        }
        rects[i] = r;
    }

    QVector<QRect> result;
    foreach (QRect r, rects) {
        if (r.isValid())
            result += r;
    }
    return result;
}

QVector<QVector<Room *> > BuildingFloor::resizeGrid(const QSize &newSize) const
{
    QVector<QVector<Room *> > grid = mRoomAtPos;
    grid.resize(newSize.width());
    for (int x = 0; x < newSize.width(); x++)
        grid[x].resize(newSize.height());
    return grid;
}

QMap<QString,FloorTileGrid*> BuildingFloor::resizeGrime(const QSize &newSize) const
{
    QMap<QString,FloorTileGrid*> grid;
    foreach (QString key, mGrimeGrid.keys()) {
        grid[key] = new FloorTileGrid(newSize.width(), newSize.height());
        for (int x = 0; x < qMin(mGrimeGrid[key]->width(), newSize.width()); x++)
            for (int y = 0; y < qMin(mGrimeGrid[key]->height(), newSize.height()); y++)
                grid[key]->replace(x, y, mGrimeGrid[key]->at(x, y));

    }

    return grid;
}

void BuildingFloor::rotate(bool right)
{
    int oldWidth = mRoomAtPos.size();
    int oldHeight = mRoomAtPos[0].size();

    int newWidth = oldWidth, newHeight = oldHeight;
    qSwap(newWidth, newHeight);

    QVector<QVector<Room*> > roomAtPos;
    roomAtPos.resize(newWidth);
    for (int x = 0; x < newWidth; x++) {
        roomAtPos[x].resize(newHeight);
        for (int y = 0; y < newHeight; y++) {
            roomAtPos[x][y] = 0;
        }
    }

    for (int x = 0; x < oldWidth; x++) {
        for (int y = 0; y < oldHeight; y++) {
            Room *room = mRoomAtPos[x][y];
            if (right)
                roomAtPos[oldHeight - y - 1][x] = room;
            else
                roomAtPos[y][oldWidth - x - 1] = room;
        }
    }

    setGrid(roomAtPos);

    foreach (BuildingObject *object, mObjects)
        object->rotate(right);
}

void BuildingFloor::flip(bool horizontal)
{
    if (horizontal) {
        for (int x = 0; x < width() / 2; x++)
            mRoomAtPos[x].swap(mRoomAtPos[width() - x - 1]);
    } else {
        for (int x = 0; x < width(); x++)
            for (int y = 0; y < height() / 2; y++)
                qSwap(mRoomAtPos[x][y], mRoomAtPos[x][height() - y - 1]);
    }

    foreach (BuildingObject *object, mObjects)
        object->flip(horizontal);
}

BuildingFloor *BuildingFloor::clone()
{
    BuildingFloor *klone = new BuildingFloor(mBuilding, mLevel);
    klone->mIndexAtPos = mIndexAtPos;
    klone->mRoomAtPos = mRoomAtPos;
    foreach (BuildingObject *object, mObjects) {
        BuildingObject *kloneObject = object->clone();
        kloneObject->setFloor(klone);
        klone->mObjects += kloneObject;
    }
    klone->mGrimeGrid = mGrimeGrid;
    foreach (QString key, klone->mGrimeGrid.keys())
        klone->mGrimeGrid[key] = new FloorTileGrid(*klone->mGrimeGrid[key]);
    return klone;
}

QString BuildingFloor::grimeAt(const QString &layerName, int x, int y) const
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->at(x, y);
    return QString();
}

FloorTileGrid *BuildingFloor::grimeAt(const QString &layerName, const QRect &r)
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->clone(r);
    return new FloorTileGrid(r.width(), r.height());
}

FloorTileGrid *BuildingFloor::grimeAt(const QString &layerName, const QRect &r,
                                      const QRegion &rgn)
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->clone(r, rgn);
    return new FloorTileGrid(r.width(), r.height());
}

QMap<QString,FloorTileGrid*> BuildingFloor::grimeClone() const
{
    QMap<QString,FloorTileGrid*> clone;
    foreach (QString key, mGrimeGrid.keys()) {
        clone[key] = new FloorTileGrid(*mGrimeGrid[key]);
    }
    return clone;
}

QMap<QString, FloorTileGrid *> BuildingFloor::setGrime(const QMap<QString,
                                                       FloorTileGrid *> &grime)
{
    QMap<QString,FloorTileGrid*> old = mGrimeGrid;
    mGrimeGrid = grime;
    return old;
}

void BuildingFloor::setGrime(const QString &layerName, int x, int y,
                             const QString &tileName)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(x, y, tileName);
}


void BuildingFloor::setGrime(const QString &layerName, const QPoint &p,
                             const FloorTileGrid *other)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(p, other);
}

void BuildingFloor::setGrime(const QString &layerName, const QRegion &rgn,
                             const QString &tileName)
{
    if (!mGrimeGrid.contains(layerName)) {
        if (tileName.isEmpty())
            return;
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    }
    mGrimeGrid[layerName]->replace(rgn, tileName);
}

void BuildingFloor::setGrime(const QString &layerName, const QRegion &rgn,
                             const QPoint &pos, const FloorTileGrid *other)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(rgn, pos, other);
}

/////

BuildingFloor::Square::Square() :
    mEntries(MaxSection, 0),
    mEntryEnum(MaxSection, 0),
    mExterior(false),
    mTiles(MaxSection, 0)
{
    mWallN.entry = mWallW.entry = 0;
}


BuildingFloor::Square::~Square()
{
    // mTiles are owned by BuildingTiles
//    for (int i = 0; i < MaxSection; i++)
    //        delete mTiles[i];
}

void BuildingFloor::Square::SetWallN(BuildingTileEntry *tile, bool exterior)
{
    mWallN.entry = tile;
    mWallN.exterior = exterior;
}

void BuildingFloor::Square::SetWallW(BuildingTileEntry *tile, bool exterior)
{
    mWallW.entry = tile;
    mWallW.exterior = exterior;
}

bool BuildingFloor::Square::IsWallOrient(BuildingFloor::Square::WallOrientation orient)
{
    return mEntries[SectionWall] &&
            !mEntries[SectionWall]->isNone() &&
            (mWallOrientation == orient);
}

void BuildingFloor::Square::ReplaceFloor(BuildingTileEntry *tile, int offset)
{
    mEntries[SectionFloor] = tile;
    mEntryEnum[SectionFloor] = offset;
}

void BuildingFloor::Square::ReplaceWall(BuildingTileEntry *tile,
                                        WallOrientation orient,
                                        bool exterior)
{
    mEntries[SectionWall] = tile;
    mWallOrientation = orient; // Must set this before getWallOffset() is called
    mEntryEnum[SectionWall] = getWallOffset();
    mExterior = exterior;
}

void BuildingFloor::Square::ReplaceDoor(BuildingTileEntry *tile, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[SectionDoor] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[SectionDoor] = offset;
    // FIXME: if this is a NW corner with 2 different wall tiles, this will
    // choose the wrong wall tile.  Fix it by only changing West->WestDoor or
    // North->NorthDoor in SectionWall or SectionWall2.  Same for ReplaceFrame.
    mEntryEnum[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceFrame(BuildingTileEntry *tile, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[SectionFrame] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[SectionFrame] = offset;
    mEntryEnum[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceCurtains(Window *window, bool exterior)
{
    mEntries[exterior ? SectionCurtains2 : SectionCurtains] = window->curtainsTile();
    mEntryEnum[exterior ? SectionCurtains2 : SectionCurtains] = window->isW()
            ? (exterior ? BTC_Curtains::East : BTC_Curtains::West)
            : (exterior ? BTC_Curtains::South : BTC_Curtains::North);
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTileEntry *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mEntries[SectionFurniture] = 0;
        mEntryEnum[SectionFurniture] = 0;
        return;
    }
    if (mEntries[SectionFurniture] && !mEntries[SectionFurniture]->isNone()) {
        mEntries[SectionFurniture2] = tile;
        mEntryEnum[SectionFurniture2] = offset;
        return;
    }
    mEntries[SectionFurniture] = tile;
    mEntryEnum[SectionFurniture] = offset;
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTile *tile,
                                             SquareSection section,
                                             SquareSection section2)
{
    if (mTiles[section] && !mTiles[section]->isNone() && (section2 != SectionInvalid)) {
        mTiles[section2] = tile;
        mEntryEnum[section2] = 0;
        return;
    }
    mTiles[section] = tile;
    mEntryEnum[section] = 0;
}

void BuildingFloor::Square::ReplaceRoof(BuildingTileEntry *tile, int offset)
{
    if (mEntries[SectionRoof] && !mEntries[SectionRoof]->isNone()) {
        mEntries[SectionRoof2] = tile;
        mEntryEnum[SectionRoof2] = offset;
        return;
    }
    mEntries[SectionRoof] = tile;
    mEntryEnum[SectionRoof] = offset;
}

void BuildingFloor::Square::ReplaceRoofCap(BuildingTileEntry *tile, int offset)
{
    if (mEntries[SectionRoofCap] && !mEntries[SectionRoofCap]->isNone()) {
        mEntries[SectionRoofCap2] = tile;
        mEntryEnum[SectionRoofCap2] = offset;
        return;
    }
    mEntries[SectionRoofCap] = tile;
    mEntryEnum[SectionRoofCap] = offset;
}

void BuildingFloor::Square::ReplaceRoofTop(BuildingTileEntry *tile, int offset)
{
    mEntries[SectionRoofTop] = tile;
    mEntryEnum[SectionRoofTop] = offset;
}

int BuildingFloor::Square::getWallOffset()
{
    BuildingTileEntry *tile = mEntries[SectionWall];
    if (!tile)
        return -1;

    int offset = BTC_Walls::West;

    switch (mWallOrientation) {
    case WallOrientN:
        if (mEntries[SectionDoor] != 0)
            offset = BTC_Walls::NorthDoor;
        else if (mEntries[SectionFrame] != 0)
            offset = BTC_Walls::NorthWindow;
        else
            offset = BTC_Walls::North;
        break;
    case WallOrientNW:
        offset = BTC_Walls::NorthWest;
        break;
    case WallOrientW:
        if (mEntries[SectionDoor] != 0)
            offset = BTC_Walls::WestDoor;
        else if (mEntries[SectionFrame] != 0)
            offset = BTC_Walls::WestWindow;
        break;
    case WallOrientSE:
        offset = BTC_Walls::SouthEast;
        break;
    }

    return offset;
}
