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

#include "buildingobjects.h"

#include "buildingfloor.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <qmath.h>

using namespace BuildingEditor;

/////

BuildingObject::BuildingObject(BuildingFloor *floor, int x, int y, Direction dir) :
    mFloor(floor),
    mX(x),
    mY(y),
    mDir(dir),
    mTile(0)
{
}

QString BuildingObject::dirString() const
{
    static const char *s[] = { "N", "S", "E", "W" };
    return QLatin1String(s[mDir]);
}

BuildingObject::Direction BuildingObject::dirFromString(const QString &s)
{
    if (s == QLatin1String("N")) return N;
    if (s == QLatin1String("S")) return S;
    if (s == QLatin1String("W")) return W;
    if (s == QLatin1String("E")) return E;
    return Invalid;
}

QSet<BuildingTile*> BuildingObject::buildingTiles() const
{
    QSet<BuildingTile*> ret;
    foreach (BuildingTileEntry *entry, tiles()) {
        if (entry && !entry->isNone()) {
            foreach (BuildingTile *btile, entry->mTiles) {
                if (btile && !btile->isNone())
                    ret |= btile;
            }
        }
    }
    return ret;
}

bool BuildingObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor; // hackery for BaseObjectTool

    // +1 because doors/windows can be on the outside edge of the building
    QRect floorBounds = floor->bounds(1, 1);
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void BuildingObject::rotate(bool right)
{
    mDir = (mDir == N) ? W : N;

    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();
    if (right) {
        int x = mX;
        mX = oldHeight - mY - 1;
        mY = x;
        if (mDir == W)
            mX++;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - 1;
        if (mDir == N)
            mY++;
    }
}

void BuildingObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - 1;
        if (mDir == W)
            mX++;
    } else {
        mY = mFloor->height() - mY - 1;
        if (mDir == N)
            mY++;
    }
}

bool BuildingObject::sameTilesAs(BuildingObject *other)
{
    QList<BuildingTileEntry*> tiles1 = this->tiles();
    QList<BuildingTileEntry*> tiles2 = other->tiles();
    if (tiles1.size() != tiles2.size())
        return false;
    for (int i = 0; i < tiles1.size(); i++)
        if (!tiles1[i]->equals(tiles2[i]))
            return false;
    return true;
}

int BuildingObject::index()
{
    return mFloor->indexOf(this);
}

/////

BuildingObject *Door::clone() const
{
    Door *clone = new Door(mFloor, mX, mY, mDir);
    clone->mTile = mTile;
    clone->mFrameTile = mFrameTile;
    return clone;
}

bool Door::sameAs(BuildingObject *other)
{
    if (Door *o = other->asDoor()) {
        return mDir == o->mDir && pos() == o->pos() &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                sameTilesAs(other);
    }
    return false;
}

QPolygonF Door::calcShape() const
{
    if (isN())
        return QRectF(mX, mY - 5/30.0, 30/30.0, 10/30.0);
    if (isW())
        return QRectF(mX - 5/30.0, mY, 10/30.0, 30/30.0);
    return QPolygonF();
}

/////

QRect Stairs::bounds() const
{
    if (mDir == N)
        return QRect(mX, mY, 1, 5);
    if (mDir == W)
        return QRect(mX, mY, 5, 1);
    return QRect();
}

void Stairs::rotate(bool right)
{
    BuildingObject::rotate(right);
    if (right) {
        if (mDir == W) // used to be N
            mX -= 5;
    } else {
        if (mDir == N) // used to be W
            mY -= 5;
    }
}

void Stairs::flip(bool horizontal)
{
    BuildingObject::flip(horizontal);
    if (mDir == W && horizontal)
        mX -= 5;
    else if (mDir == N && !horizontal)
        mY -= 5;
}

bool Stairs::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because stairs can't be on the outside edge of the building.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

BuildingObject *Stairs::clone() const
{
    Stairs *clone = new Stairs(mFloor, mX, mY, mDir);
    clone->mTile = mTile;
    return clone;
}

bool Stairs::sameAs(BuildingObject *other)
{
    if (Stairs *o = other->asStairs()) {
        return mDir == o->mDir && pos() == o->pos() &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                sameTilesAs(other);
    }
    return false;
}

QPolygonF Stairs::calcShape() const
{
    if (isN())
        return QRectF(mX, mY, 30/30.0, 30 * 5/30.0);
    if (isW())
        return QRectF(mX, mY, 30 * 5/30.0, 30/30.0);
    return QPolygonF();
}

int Stairs::getOffset(int x, int y)
{
    if (mDir == N) {
        if (x == mX) {
            int index = y - mY;
            if (index == 1)
                return BTC_Stairs::North3;
            if (index == 2)
                return BTC_Stairs::North2;
            if (index == 3)
                return BTC_Stairs::North1;
        }
    }
    if (mDir == W) {
        if (y == mY) {
            int index = x - mX;
            if (index == 1)
                return BTC_Stairs::West3;
            if (index == 2)
                return BTC_Stairs::West2;
            if (index == 3)
                return BTC_Stairs::West1;
        }
    }
    return -1;
}

/////

FurnitureObject::FurnitureObject(BuildingFloor *floor, int x, int y) :
    BuildingObject(floor, x, y, Invalid),
    mFurnitureTile(0)
{

}

QRect FurnitureObject::bounds() const
{
    return mFurnitureTile ? QRect(pos(), mFurnitureTile->resolved()->size())
                           : QRect(mX, mY, 1, 1);
}

void FurnitureObject::rotate(bool right)
{
    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();

    FurnitureTile::FurnitureOrientation map[8];
    if (right) {
        map[FurnitureTile::FurnitureW] = FurnitureTile::FurnitureN;
        map[FurnitureTile::FurnitureN] = FurnitureTile::FurnitureE;
        map[FurnitureTile::FurnitureE] = FurnitureTile::FurnitureS;
        map[FurnitureTile::FurnitureS] = FurnitureTile::FurnitureW;
        map[FurnitureTile::FurnitureSW] = FurnitureTile::FurnitureNW;
        map[FurnitureTile::FurnitureNW] = FurnitureTile::FurnitureNE;
        map[FurnitureTile::FurnitureNE] = FurnitureTile::FurnitureSE;
        map[FurnitureTile::FurnitureSE] = FurnitureTile::FurnitureSW;
    } else {
        map[FurnitureTile::FurnitureW] = FurnitureTile::FurnitureS;
        map[FurnitureTile::FurnitureS] = FurnitureTile::FurnitureE;
        map[FurnitureTile::FurnitureE] = FurnitureTile::FurnitureN;
        map[FurnitureTile::FurnitureN] = FurnitureTile::FurnitureW;
        map[FurnitureTile::FurnitureSW] = FurnitureTile::FurnitureSE;
        map[FurnitureTile::FurnitureSE] = FurnitureTile::FurnitureNE;
        map[FurnitureTile::FurnitureNE] = FurnitureTile::FurnitureNW;
        map[FurnitureTile::FurnitureNW] = FurnitureTile::FurnitureSW;
    }
    FurnitureTile *oldTile = mFurnitureTile;
    FurnitureTile *newTile = oldTile->owner()->tile(map[oldTile->orient()]);

    if (right) {
        int x = mX;
        mX = oldHeight - mY - newTile->resolved()->size().width();
        mY = x;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - newTile->resolved()->size().height();
    }

#if 0
    // Stop things going out of bounds, which can happen if the furniture
    // is asymmetric.
    if (mX < 0)
        mX = 0;
    if (mX + newTile->size().width() > mFloor->width())
        mX = mFloor->width() - newTile->size().width();
    if (mY < 0)
        mY = 0;
    if (mY + newTile->size().height() > mFloor->height())
        mY = mFloor->height() - newTile->size().height();
#endif

    mFurnitureTile = newTile;
}

void FurnitureObject::flip(bool horizontal)
{
    FurnitureTile::FurnitureOrientation map[8];
    if (horizontal) {
        int oldWidth = mFurnitureTile->resolved()->size().width();
        map[FurnitureTile::FurnitureW] = FurnitureTile::FurnitureE;
        map[FurnitureTile::FurnitureN] = FurnitureTile::FurnitureN;
        map[FurnitureTile::FurnitureE] = FurnitureTile::FurnitureW;
        map[FurnitureTile::FurnitureS] = FurnitureTile::FurnitureS;
        map[FurnitureTile::FurnitureSW] = FurnitureTile::FurnitureSE;
        map[FurnitureTile::FurnitureNW] = FurnitureTile::FurnitureNE;
        map[FurnitureTile::FurnitureNE] = FurnitureTile::FurnitureNW;
        map[FurnitureTile::FurnitureSE] = FurnitureTile::FurnitureSW;
        mFurnitureTile = mFurnitureTile->owner()->tile(map[mFurnitureTile->orient()]);
        mX = mFloor->width() - mX - qMax(oldWidth, mFurnitureTile->resolved()->size().width());
    } else {
        int oldHeight = mFurnitureTile->size().height();
        map[FurnitureTile::FurnitureW] = FurnitureTile::FurnitureW;
        map[FurnitureTile::FurnitureN] = FurnitureTile::FurnitureS;
        map[FurnitureTile::FurnitureE] = FurnitureTile::FurnitureE;
        map[FurnitureTile::FurnitureS] = FurnitureTile::FurnitureN;
        map[FurnitureTile::FurnitureSW] = FurnitureTile::FurnitureNW;
        map[FurnitureTile::FurnitureNW] = FurnitureTile::FurnitureSW;
        map[FurnitureTile::FurnitureNE] = FurnitureTile::FurnitureSE;
        map[FurnitureTile::FurnitureSE] = FurnitureTile::FurnitureNE;
        mFurnitureTile = mFurnitureTile->owner()->tile(map[mFurnitureTile->orient()]);
        mY = mFloor->height() - mY - qMax(oldHeight, mFurnitureTile->resolved()->size().height());
    }
}

bool FurnitureObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because furniture can't be on the outside edge of the building.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

QSet<BuildingTile *> FurnitureObject::buildingTiles() const
{
    QSet<BuildingTile *> ret;
    if (!mFurnitureTile || mFurnitureTile->isEmpty())
        return ret;
    foreach (BuildingTile *btile, mFurnitureTile->tiles()) {
        if (btile && !btile->isNone())
            ret += btile;
    }
    return ret;
}

BuildingObject *FurnitureObject::clone() const
{
    FurnitureObject *clone = new FurnitureObject(mFloor, mX, mY);
    clone->mFurnitureTile = mFurnitureTile;
    return clone;
}

bool FurnitureObject::sameAs(BuildingObject *other)
{
    if (FurnitureObject *o = other->asFurniture()) {
        return pos() == o->pos() &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                (mFurnitureTile == o->mFurnitureTile ||
                mFurnitureTile->equals(o->mFurnitureTile));
    }
    return false;
}

QPolygonF FurnitureObject::calcShape() const
{
    QRectF r = bounds();
    FurnitureTile *ftile = furnitureTile();
    FurnitureTiles::FurnitureLayer layer = furnitureTile()->owner()->layer();
    if (inWallLayer()) {
        if (ftile->isW())
            r.setRight(r.left() + 10/30.0);
        else if (ftile->isN())
            r.setBottom(r.top() + 10/30.0);
        else if (ftile->isE())
            r.setLeft(r.right() - 10/30.0);
        else if (ftile->isS())
            r.setTop(r.bottom() - 10/30.0);
        return r;
    }
    if (layer == FurnitureTiles::LayerWalls ||
            layer == FurnitureTiles::LayerRoofCap) {
        if (ftile->isW()) {
            r.setRight(r.left() + 12/30.0);
            r.translate(-6/30.0, 0);
        } else if (ftile->isE()) {
            r.setLeft(r.right() - 12/30.0);
            r.translate(6/30.0, 0);
        } else if (ftile->isN()) {
            r.setBottom(r.top() + 12/30.0);
            r.translate(0, -6/30.0);
        } else if (ftile->isS()) {
            r.setTop(r.bottom() - 12/30.0);
            r.translate(0, 6/30.0);
        }
        return r;
    }
    if (layer == FurnitureTiles::LayerFrames) {
        // Mimic window shape
        if (ftile->isW()) {
            r.setRight(r.left() + 6/30.0);
            r.adjust(0,7/30.0,0,-7/30.0);
            r.translate(-3/30.0, 0);
        } else if (ftile->isE()) {
            r.setLeft(r.right() - 6/30.0);
            r.adjust(0,7/30.0,0,-7/30.0);
            r.translate(3/30.0, 0);
        } else if (ftile->isN()) {
            r.adjust(7/30.0,0,-7/30.0,0);
            r.setBottom(r.top() + 6/30.0);
            r.translate(0, -3/30.0);
        } else if (ftile->isS()) {
            r.adjust(7/30.0,0,-7/30.0,0);
            r.setTop(r.bottom() - 6/30.0);
            r.translate(0, 3/30.0);
        }
        return r;
    }
    if (layer == FurnitureTiles::LayerDoors) {
        // Mimic door shape
        if (ftile->isW()) {
            r.setRight(r.left() + 10/30.0);
            r.translate(-5/30.0, 0);
        } else if (ftile->isE()) {
            r.setLeft(r.right() - 10/30.0);
            r.translate(5/30.0, 0);
        } else if (ftile->isN()) {
            r.setBottom(r.top() + 10/30.0);
            r.translate(0, -5/30.0);
        } else if (ftile->isS()) {
            r.setTop(r.bottom() - 10/30.0);
            r.translate(0, 5/30.0);
        }
        return r;
    }
    r.adjust(2/30.0, 2/30.0, -2/30.0, -2/30.0);
    return r;
}

void FurnitureObject::setFurnitureTile(FurnitureTile *tile)
{
#if 0
    // FIXME: the object might change size and go out of bounds.
    QSize oldSize = mFurnitureTile ? mFurnitureTile->size() : QSize(0, 0);
    QSize newSize = tile ? tile->size() : QSize(0, 0);
    if (oldSize != newSize) {

    }
#endif
    mFurnitureTile = tile;
}

bool FurnitureObject::inWallLayer() const
{
    return mFurnitureTile->owner()->layer() == FurnitureTiles::LayerWallOverlay
            || mFurnitureTile->owner()->layer() == FurnitureTiles::LayerWallFurniture;
}

/////

RoofObject::RoofObject(BuildingFloor *floor, int x, int y, int width, int height,
                       RoofType type, RoofDepth depth,
                       bool cappedW, bool cappedN, bool cappedE, bool cappedS) :
    BuildingObject(floor, x, y, BuildingObject::Invalid),
    mWidth(width),
    mHeight(height),
    mType(type),
    mDepth(depth),
    mCappedW(cappedW),
    mCappedN(cappedN),
    mCappedE(cappedE),
    mCappedS(cappedS),
    mHalfDepth(depth == Point5 || depth == OnePoint5 || depth == TwoPoint5),
    mCapTiles(0),
    mSlopeTiles(0),
    mTopTiles(0)
{
    resize(mWidth, mHeight, mHalfDepth);
}

QRect RoofObject::bounds() const
{
    return QRect(mX, mY, mWidth, mHeight);
}

void RoofObject::rotate(bool right)
{
    int oldFloorWidth = mFloor->height();
    int oldFloorHeight = mFloor->width();

    qSwap(mWidth, mHeight);

    if (right) {
        int x = mX;
        mX = oldFloorHeight - mY - bounds().width();
        mY = x;

        switch (mType) {
        case SlopeW: mType = SlopeN; break;
        case SlopeN: mType = SlopeE; break;
        case SlopeE: mType = SlopeS; break;
        case SlopeS: mType = SlopeW; break;
        case PeakWE: mType = PeakNS; break;
        case PeakNS: mType = PeakWE; break;
        case DormerW: mType = DormerN; break;
        case DormerN: mType = DormerE; break;
        case DormerE: mType = DormerS; break;
        case DormerS: mType = DormerW; break;
        case FlatTop: break;

        case ShallowSlopeW: mType = ShallowSlopeN; break;
        case ShallowSlopeN: mType = ShallowSlopeE; break;
        case ShallowSlopeE: mType = ShallowSlopeS; break;
        case ShallowSlopeS: mType = ShallowSlopeW; break;
        case ShallowPeakWE: mType = ShallowPeakNS; break;
        case ShallowPeakNS: mType = ShallowPeakWE; break;

        case Slope30W: mType = Slope30N; break;
        case Slope30N: mType = Slope30E; break;
        case Slope30E: mType = Slope30S; break;
        case Slope30S: mType = Slope30W; break;
        case Peak30WE: mType = Peak30NS; break;
        case Peak30NS: mType = Peak30WE; break;
        case Dormer30W: mType = Dormer30N; break;
        case Dormer30N: mType = Dormer30E; break;
        case Dormer30E: mType = Dormer30S; break;
        case Dormer30S: mType = Dormer30W; break;

        case CornerInnerSW: mType = CornerInnerNW; break;
        case CornerInnerNW: mType = CornerInnerNE; break;
        case CornerInnerNE: mType = CornerInnerSE; break;
        case CornerInnerSE: mType = CornerInnerSW; break;

        case CornerOuterSW: mType = CornerOuterNW; break;
        case CornerOuterNW: mType = CornerOuterNE; break;
        case CornerOuterNE: mType = CornerOuterSE; break;
        case CornerOuterSE: mType = CornerOuterSW; break;

        case CornerSlope30InnerSW: mType = CornerSlope30InnerNW; break;
        case CornerSlope30InnerNW: mType = CornerSlope30InnerNE; break;
        case CornerSlope30InnerNE: mType = CornerSlope30InnerSE; break;
        case CornerSlope30InnerSE: mType = CornerSlope30InnerSW; break;

        case CornerSlope30OuterSW: mType = CornerSlope30OuterNW; break;
        case CornerSlope30OuterNW: mType = CornerSlope30OuterNE; break;
        case CornerSlope30OuterNE: mType = CornerSlope30OuterSE; break;
        case CornerSlope30OuterSE: mType = CornerSlope30OuterSW; break;

        default:
            break;
        }

        // Rotate caps
        bool w = mCappedW;
        mCappedW = mCappedS;
        mCappedS = mCappedE;
        mCappedE = mCappedN;
        mCappedN = w;

    } else {
        int x = mX;
        mX = mY;
        mY = oldFloorWidth - x - bounds().height();

        switch (mType) {
        case SlopeW: mType = SlopeS; break;
        case SlopeN: mType = SlopeW; break;
        case SlopeE: mType = SlopeN; break;
        case SlopeS: mType = SlopeE; break;
        case PeakWE: mType = PeakNS; break;
        case PeakNS: mType = PeakWE; break;
        case DormerW: mType = DormerS; break;
        case DormerN: mType = DormerW; break;
        case DormerE: mType = DormerN; break;
        case DormerS: mType = DormerE; break;
        case FlatTop: break;

        case ShallowSlopeW: mType = ShallowSlopeS; break;
        case ShallowSlopeN: mType = ShallowSlopeW; break;
        case ShallowSlopeE: mType = ShallowSlopeN; break;
        case ShallowSlopeS: mType = ShallowSlopeE; break;
        case ShallowPeakWE: mType = ShallowPeakNS; break;
        case ShallowPeakNS: mType = ShallowPeakWE; break;

        case Slope30W: mType = Slope30S; break;
        case Slope30N: mType = Slope30W; break;
        case Slope30E: mType = Slope30N; break;
        case Slope30S: mType = Slope30E; break;
        case Peak30WE: mType = Peak30NS; break;
        case Peak30NS: mType = Peak30WE; break;
        case Dormer30W: mType = Dormer30S; break;
        case Dormer30N: mType = Dormer30W; break;
        case Dormer30E: mType = Dormer30N; break;
        case Dormer30S: mType = Dormer30E; break;

        case CornerInnerSW: mType = CornerInnerSE; break;
        case CornerInnerNW: mType = CornerInnerSW; break;
        case CornerInnerNE: mType = CornerInnerNW; break;
        case CornerInnerSE: mType = CornerInnerNE; break;

        case CornerOuterSW: mType = CornerOuterSE; break;
        case CornerOuterNW: mType = CornerOuterSW; break;
        case CornerOuterNE: mType = CornerOuterNW; break;
        case CornerOuterSE: mType = CornerOuterNE; break;

        case CornerSlope30InnerSW: mType = CornerSlope30InnerSE; break;
        case CornerSlope30InnerNW: mType = CornerSlope30InnerSW; break;
        case CornerSlope30InnerNE: mType = CornerSlope30InnerNW; break;
        case CornerSlope30InnerSE: mType = CornerSlope30InnerNE; break;

        case CornerSlope30OuterSW: mType = CornerSlope30OuterSE; break;
        case CornerSlope30OuterNW: mType = CornerSlope30OuterSW; break;
        case CornerSlope30OuterNE: mType = CornerSlope30OuterNW; break;
        case CornerSlope30OuterSE: mType = CornerSlope30OuterNE; break;

        default:
            break;
        }

        // Rotate caps
        bool w = mCappedW;
        mCappedW = mCappedN;
        mCappedN = mCappedE;
        mCappedE = mCappedS;
        mCappedS = w;
    }
}

void RoofObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - bounds().width();

        switch (mType) {
        case SlopeW: mType = SlopeE; break;
        case SlopeN:  break;
        case SlopeE: mType = SlopeW; break;
        case SlopeS:  break;
        case PeakWE:  break;
        case PeakNS:  break;
        case DormerW: mType = DormerE; break;
        case DormerN: break;
        case DormerE: mType = DormerW; break;
        case DormerS: break;
        case FlatTop: break;

        case ShallowSlopeW: mType = ShallowSlopeE; break;
        case ShallowSlopeN:  break;
        case ShallowSlopeE: mType = ShallowSlopeW; break;
        case ShallowSlopeS:  break;
        case ShallowPeakWE:  break;
        case ShallowPeakNS:  break;

        case Slope30W: mType = Slope30E; break;
        case Slope30N: break;
        case Slope30E: mType = Slope30W; break;
        case Slope30S: break;
        case Dormer30W: mType = Dormer30E; break;
        case Dormer30N: break;
        case Dormer30E: mType = Dormer30W; break;
        case Dormer30S: break;

        case CornerInnerSW: mType = CornerInnerSE; break;
        case CornerInnerNW: mType = CornerInnerNE; break;
        case CornerInnerNE: mType = CornerInnerNW; break;
        case CornerInnerSE: mType = CornerInnerSW; break;

        case CornerOuterSW: mType = CornerOuterSE; break;
        case CornerOuterNW: mType = CornerOuterNE; break;
        case CornerOuterNE: mType = CornerOuterNW; break;
        case CornerOuterSE: mType = CornerOuterSW; break;
        default:
            break;
        }

        qSwap(mCappedW, mCappedE);
    } else {
        mY = mFloor->height() - mY - bounds().height();
        switch (mType) {
        case SlopeW:  break;
        case SlopeN: mType = SlopeS; break;
        case SlopeE:  break;
        case SlopeS: mType = SlopeN; break;
        case PeakWE:  break;
        case PeakNS:  break;
        case DormerW: break;
        case DormerN: mType = DormerS; break;
        case DormerE: break;
        case DormerS: mType = DormerN; break;
        case FlatTop: break;

        case ShallowSlopeW:  break;
        case ShallowSlopeN: mType = ShallowSlopeS; break;
        case ShallowSlopeE:  break;
        case ShallowSlopeS: mType = ShallowSlopeN; break;
        case ShallowPeakWE:  break;
        case ShallowPeakNS:  break;

        case Slope30W:  break;
        case Slope30N: mType = Slope30S; break;
        case Slope30E:  break;
        case Slope30S: mType = Slope30N; break;
        case Dormer30W: break;
        case Dormer30N: mType = Dormer30S; break;
        case Dormer30E: break;
        case Dormer30S: mType = Dormer30N; break;

        case CornerInnerSW: mType = CornerInnerNW; break;
        case CornerInnerNW: mType = CornerInnerSW; break;
        case CornerInnerNE: mType = CornerInnerSE; break;
        case CornerInnerSE: mType = CornerInnerNE; break;

        case CornerOuterSW: mType = CornerOuterNW; break;
        case CornerOuterNW: mType = CornerOuterSW; break;
        case CornerOuterNE: mType = CornerOuterSE; break;
        case CornerOuterSE: mType = CornerOuterNE; break;
        default:
            break;
        }

        qSwap(mCappedN, mCappedS);
    }
}

bool RoofObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because roofs can't be on the outside edge of the building.
    // However, the E or S cap wall tiles can be on the outside edge.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void RoofObject::setTile(BuildingTileEntry *tile, int alternate)
{
    if ((alternate == TileCap) && (/*tile->isNone() || */tile->asRoofCap()))
        mCapTiles = tile;
    else if ((alternate == TileSlope) && (/*tile->isNone() || */tile->asRoofSlope()))
        mSlopeTiles = tile;
    else if ((alternate == TileTop) && (tile->isNone() || tile->asRoofTop()))
        mTopTiles = tile;
}

BuildingTileEntry *RoofObject::tile(int alternate) const
{
    if (alternate == TileCap) return mCapTiles;
    if (alternate == TileSlope) return mSlopeTiles;
    if (alternate == TileTop) return mTopTiles;
    return 0;
}

BuildingObject *RoofObject::clone() const
{
    RoofObject *clone = new RoofObject(mFloor, mX, mY, mWidth, mHeight, mType, mDepth,
                                       mCappedW, mCappedN, mCappedE, mCappedS);
    clone->mHalfDepth = mHalfDepth;
    clone->mCapTiles = mCapTiles;
    clone->mSlopeTiles = mSlopeTiles;
    clone->mTopTiles = mTopTiles;
    return clone;
}

bool RoofObject::sameAs(BuildingObject *other)
{
    if (RoofObject *o = other->asRoof()) {
        return bounds() == o->bounds() &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                mHalfDepth == o->mHalfDepth &&
                mCapTiles->equals(o->mCapTiles) &&
                mSlopeTiles->equals(o->mSlopeTiles) &&
                mTopTiles->equals(o->mTopTiles);
    }
    return false;

}

QPolygonF RoofObject::calcShape() const
{
    return QRectF(bounds());
}

void RoofObject::setCapTiles(BuildingTileEntry *entry)
{
    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();
    if (!entry->isNone() && !entry->asRoofCap()) {
        qFatal("wrong type of tiles passed to RoofObject::setCapTiles");
        return;
    }
    mCapTiles = entry;
}

void RoofObject::setSlopeTiles(BuildingTileEntry *entry)
{
    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();
    if (!entry->isNone() && !entry->asRoofSlope()) {
        qFatal("wrong type of tiles passed to RoofObject::setSlopeTiles");
        return;
    }
    mSlopeTiles = entry;
}

void RoofObject::setTopTiles(BuildingTileEntry *entry)
{
    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();
    if (!entry->isNone() && !entry->asRoofTop()) {
        qFatal("wrong type of tiles passed to RoofObject::setTopTiles");
        return;
    }
    mTopTiles = entry;
}

void RoofObject::setType(RoofObject::RoofType type)
{
    mType = type;
}

void RoofObject::setWidth(int width)
{
    switch (mType) {
    case SlopeW:
    case SlopeE:
        mWidth = qBound(1, width, 3);
        switch (mWidth) {
        case 1:
            mDepth = mHalfDepth ? Point5 : One;
            break;
        case 2:
            mDepth = mHalfDepth ? OnePoint5 : Two;
            break;
        case 3:
            mDepth = mHalfDepth ? TwoPoint5 : Three;
            break;
        }
        break;
    case SlopeN:
    case SlopeS:
    case PeakWE:
    case DormerW:
    case DormerE:
        mWidth = width;
        break;
    case FlatTop:
        mWidth = width;
        if (mDepth == InvalidDepth) // true when creating the object
            mDepth = Zero;
        break;
    case PeakNS:
    case DormerN:
    case DormerS:
        mWidth = width; //qBound(1, width, 6);
        switch (mWidth) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        default:
            mDepth = Three;
            break;
        }
        break;

    case ShallowSlopeW:
    case ShallowSlopeE:
        mWidth = qBound(1, width, 2);
        mDepth = Zero;
        break;
    case ShallowSlopeN:
    case ShallowSlopeS:
        mWidth = width;
        mDepth = Zero;
        break;
    case ShallowPeakWE:
        mWidth = width;
        mDepth = Zero;
        break;
    case ShallowPeakNS:
        if (width < 4) mWidth = 2;
        else mWidth = 4;
        mDepth = Zero;
        break;

    case Slope30W:
    case Slope30E:
        mWidth = qBound(1, width, 6);
        mDepth = Zero;
        break;
    case Slope30N:
    case Slope30S:
        mWidth = width;
        mDepth = Zero;
        break;
    case Peak30WE:
        mWidth = width;
        mDepth = Zero;
        break;
    case Peak30NS:
    case Peak30Quad:
        if (width > 9) mWidth = 11;
        else if (width > 7) mWidth = 9;
        else if (width > 5) mWidth = 7;
        else if (width > 3) mWidth = 5;
        else if (width > 1) mWidth = 3;
        else mWidth = 1;
        mDepth = Zero;
        break;
    case Dormer30W:
    case Dormer30E:
        mWidth = width;
        break;
    case Dormer30N:
    case Dormer30S:
        if (width > 9) mWidth = std::max(width, 11);
        else if (width > 7) mWidth = 9;
        else if (width > 5) mWidth = 7;
        else if (width > 3) mWidth = 5;
        else if (width > 1) mWidth = 3;
        else mWidth = 1;
        mDepth = Zero;
        break;

    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
    case CornerOuterSW:
    case CornerOuterNW:
    case CornerOuterNE:
    case CornerOuterSE:
        mWidth = qBound(1, width, 3);
        switch (mWidth) {
        case 1:
            mDepth = mHalfDepth ? Point5 : One;
            break;
        case 2:
            mDepth = mHalfDepth ? OnePoint5 : Two;
            break;
        case 3:
            mDepth = mHalfDepth ? TwoPoint5 : Three;
            break;
        }
        break;

    case CornerSlope30InnerSW:
    case CornerSlope30InnerNW:
    case CornerSlope30InnerNE:
    case CornerSlope30InnerSE:
    case CornerSlope30OuterSW:
    case CornerSlope30OuterNW:
    case CornerSlope30OuterNE:
    case CornerSlope30OuterSE:
        mWidth = qBound(1, width, 6);
        switch (mWidth) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        case 6:
            mDepth = Three;
            break;
        }
        break;

    default:
        break;
    }
}

void RoofObject::setHeight(int height)
{
    switch (mType) {
    case SlopeW:
    case SlopeE:
        mHeight = height;
        break;
    case SlopeN:
    case SlopeS:
        mHeight = qBound(1, height, 3);
        switch (mHeight) {
        case 1:
            mDepth = mHalfDepth ? Point5 : One;
            break;
        case 2:
            mDepth = mHalfDepth ? OnePoint5 : Two;
            break;
        case 3:
            mDepth = mHalfDepth ? TwoPoint5 : Three;
            break;
        }
        break;
    case PeakWE:
    case DormerW:
    case DormerE:
        mHeight = height; //qBound(1, height, 6);
        switch (mHeight) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        default:
            mDepth = Three;
            break;
        }
        if (mType == DormerW || mType == DormerE) {
            switch (mDepth) {
            case OnePoint5: case Two: mWidth = qMax(mWidth, 2); break;
            case TwoPoint5: case Three: mWidth = qMax(mWidth, 3); break;
            default: break;
            }
        }
        break;
    case PeakNS:
        mHeight = height;
        break;
    case DormerN:
    case DormerS:
        mHeight = height;
        switch (mDepth) {
        case OnePoint5: case Two: mHeight = qMax(mHeight, 2); break;
        case TwoPoint5: case Three: mHeight = qMax(mHeight, 3); break;
        default: break;
        }
        break;
    case FlatTop:
        mHeight = height;
        if (mDepth == InvalidDepth)
            mDepth = Three;
        break;

    case ShallowSlopeW:
    case ShallowSlopeE:
        mHeight = height;
        mDepth = Zero;
        break;
    case ShallowSlopeN:
    case ShallowSlopeS:
        mHeight = qBound(1, height, 2);
        mDepth = Zero;
        break;
    case ShallowPeakWE:
        if (height < 4) mHeight = 2;
        else mHeight = 4;
        mDepth = Zero;
        break;
    case ShallowPeakNS:
        mHeight = height;
        mDepth = Zero;
        break;

    case Slope30W:
    case Slope30E:
        mHeight = height;
        mDepth = Zero;
        break;
    case Slope30N:
    case Slope30S:
        mHeight = qBound(1, height, 6);
        mDepth = Zero;
        break;
    case Peak30WE:
    case Peak30Quad:
        if (height > 9) mHeight = 11;
        else if (height > 7) mHeight = 9;
        else if (height > 5) mHeight = 7;
        else if (height > 3) mHeight = 5;
        else if (height > 1) mHeight = 3;
        else mHeight = 1;
        mDepth = Zero;
        break;
    case Peak30NS:
        mHeight = height;
        mDepth = Zero;
        break;
    case Dormer30W:
    case Dormer30E:
        if (height > 9) mHeight = std::max(height, 11);
        else if (height > 7) mHeight = 9;
        else if (height > 5) mHeight = 7;
        else if (height > 3) mHeight = 5;
        else if (height > 1) mHeight = 3;
        else mHeight = 1;
        mDepth = Zero;
        break;
    case Dormer30N:
    case Dormer30S:
        mHeight = height;
        break;

    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
    case CornerOuterSW:
    case CornerOuterNW:
    case CornerOuterNE:
    case CornerOuterSE:
        mHeight = qBound(1, height, 3);
        switch (mHeight) {
        case 1:
            mDepth = mHalfDepth ? Point5 : One;
            break;
        case 2:
            mDepth = mHalfDepth ? OnePoint5 : Two;
            break;
        case 3:
            mDepth = mHalfDepth ? TwoPoint5 : Three;
            break;
        }
        break;

    case CornerSlope30InnerSW:
    case CornerSlope30InnerNW:
    case CornerSlope30InnerNE:
    case CornerSlope30InnerSE:
    case CornerSlope30OuterSW:
    case CornerSlope30OuterNW:
    case CornerSlope30OuterNE:
    case CornerSlope30OuterSE:
        mHeight = qBound(1, height, 6);
        switch (mHeight) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        case 6:
            mDepth = Three;
            break;
        }
        break;

    default:
        break;
    }
}

void RoofObject::resize(int width, int height, bool halfDepth)
{
    mHalfDepth = halfDepth;
    if (isCorner() || (mType == Peak30Quad)) {
        height = width = std::max(width, height);
    }
    if (mType == Dormer30W || mType == Dormer30E) {
        setHeight(height);
        width = std::max(width, std::min(mHeight / 2, 6));
        setWidth(width);
        return;
    }
    if (mType == Dormer30N || mType == Dormer30S) {
        setWidth(width);
        height = std::max(height, std::min(mWidth / 2, 6));
        setHeight(height);
        return;
    }
    setWidth(width);
    setHeight(height);
}

void RoofObject::depthUp()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case One: mDepth = Two; break;
        case Two: mDepth = Three; break;
        default:
            break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: mDepth = One; break;
        case One: mDepth = OnePoint5; break;
        case OnePoint5: mDepth = Two; break;
        case Two: mDepth = TwoPoint5; break;
        case TwoPoint5: mDepth = Three; break;
        default:
            break;
        }
        break;
    case FlatTop:
        switch (mDepth) {
        case Zero: mDepth = One; break;
        case One: mDepth = Two; break;
        case Two: mDepth = Three; break;
        default:
            break;
        }
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: mDepth = One; break;
        case OnePoint5: mDepth = Two; break;
        case TwoPoint5: mDepth = Three; break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void RoofObject::depthDown()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Two: mDepth = One; break;
        case Three: mDepth = Two; break;
        default:
            break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case One: mDepth = Point5; break;
        case OnePoint5: mDepth = One; break;
        case Two: mDepth = OnePoint5; break;
        case TwoPoint5: mDepth = Two; break;
        case Three: mDepth = TwoPoint5; break;
        default:
            break;
        }
        break;
    case FlatTop:
        switch (mDepth) {
        case One: mDepth = Zero; break;
        case Two: mDepth = One; break;
        case Three: mDepth = Two; break;
        default:
            break;
        }
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: break;
        case One: mDepth = Point5; break;
        case OnePoint5: break;
        case Two: mDepth = OnePoint5; break;
        case TwoPoint5: break;
        case Three: mDepth = TwoPoint5; break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

bool RoofObject::isDepthMax()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Three: return true; ///
        default:
            break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Three: return true; ///
        default:
            break;
        }
        break;
    case FlatTop:
        return mDepth == Three;
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case One:
        case Two:
        case Three: return true; ///
        default:
            break;
        }
        break;
    default:
        break;
    }

    return false;
}

bool RoofObject::isDepthMin()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case One: return true; ///
        default:
            break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: return true; ///
        default:
            break;
        }
        break;
    case FlatTop:
        return mDepth == Zero;
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5:
        case OnePoint5:
        case TwoPoint5: return true; ///
        default:
            break;
        }
        break;
    default:
        break;
    }

    return false;
}

int RoofObject::actualWidth() const
{
    return mWidth;
}

int RoofObject::actualHeight() const
{
    return mHeight;
}

int RoofObject::slopeThickness() const
{
    if (mType == PeakWE || mType == DormerW || mType == DormerE)
        return qCeil(qreal(qMin(mHeight, 6)) / 2);
    if (mType == PeakNS || mType == DormerN || mType == DormerS)
        return qCeil(qreal(qMin(mWidth, 6)) / 2);

    return 0;
}

bool RoofObject::isHalfDepth() const
{
    return mDepth == Point5 || mDepth == OnePoint5 || mDepth == TwoPoint5;
}

void RoofObject::toggleCappedW()
{
    mCappedW = !mCappedW;
}

void RoofObject::toggleCappedN()
{
    mCappedN = !mCappedN;
}

void RoofObject::toggleCappedE()
{
    mCappedE = !mCappedE;
}

void RoofObject::toggleCappedS()
{
    mCappedS = !mCappedS;
}

void RoofObject::setDefaultCaps()
{
    mCappedW = mCappedN = mCappedE = mCappedS = true;
    switch (mType) {
    case SlopeW: mCappedE = false; break;
    case SlopeN: mCappedS = false; break;
    case SlopeE: mCappedW = false; break;
    case SlopeS: mCappedN = false; break;
    case ShallowSlopeW: mCappedE = false; break;
    case ShallowSlopeN: mCappedS = false; break;
    case ShallowSlopeE: mCappedW = false; break;
    case ShallowSlopeS: mCappedN = false; break;
    case Slope30W: mCappedE = false; break;
    case Slope30N: mCappedS = false; break;
    case Slope30E: mCappedW = false; break;
    case Slope30S: mCappedN = false; break;
    case Peak30WE: mCappedN = mCappedS = false; break;
    case Peak30NS: mCappedW = mCappedE = false; break;
    case Peak30Quad:
        mCappedW = mCappedN = mCappedE = mCappedS = false;
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        mCappedW = mCappedN = mCappedE = mCappedS = false;
        break;
    case CornerOuterSW:
    case CornerOuterNW:
    case CornerOuterNE:
    case CornerOuterSE:
        mCappedW = mCappedN = mCappedE = mCappedS = false;
        break;
    case CornerSlope30InnerSW:
    case CornerSlope30InnerNW:
    case CornerSlope30InnerNE:
    case CornerSlope30InnerSE:
        mCappedW = mCappedN = mCappedE = mCappedS = false;
        break;
    case CornerSlope30OuterSW:
    case CornerSlope30OuterNW:
    case CornerSlope30OuterNE:
    case CornerSlope30OuterSE:
        mCappedW = mCappedN = mCappedE = mCappedS = false;
        break;
    default:
        break;
    }
}

int RoofObject::getOffset(RoofObject::RoofTile tile) const
{
    static const BTC_RoofSlopes::TileEnum mapSlope[] = {
        BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeS3,
        BTC_RoofSlopes::SlopeE1, BTC_RoofSlopes::SlopeE2, BTC_RoofSlopes::SlopeE3,
        BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E,
        BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E,
        BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E,

        BTC_RoofSlopes::ShallowSlopeW1, BTC_RoofSlopes::ShallowSlopeW2,
        BTC_RoofSlopes::ShallowSlopeE1, BTC_RoofSlopes::ShallowSlopeE2,
        BTC_RoofSlopes::ShallowSlopeN1, BTC_RoofSlopes::ShallowSlopeN2,
        BTC_RoofSlopes::ShallowSlopeS1, BTC_RoofSlopes::ShallowSlopeS2,

        BTC_RoofSlopes::Slope30S1, BTC_RoofSlopes::Slope30S2, BTC_RoofSlopes::Slope30S3, BTC_RoofSlopes::Slope30S4, BTC_RoofSlopes::Slope30S5, BTC_RoofSlopes::Slope30S6,
        BTC_RoofSlopes::Slope30E1, BTC_RoofSlopes::Slope30E2, BTC_RoofSlopes::Slope30E3, BTC_RoofSlopes::Slope30E4, BTC_RoofSlopes::Slope30E5, BTC_RoofSlopes::Slope30E6,
        BTC_RoofSlopes::Slope30W1, BTC_RoofSlopes::Slope30W2, BTC_RoofSlopes::Slope30W3, BTC_RoofSlopes::Slope30W4, BTC_RoofSlopes::Slope30W5, BTC_RoofSlopes::Slope30W6,
        BTC_RoofSlopes::Slope30N1, BTC_RoofSlopes::Slope30N2, BTC_RoofSlopes::Slope30N3, BTC_RoofSlopes::Slope30N4, BTC_RoofSlopes::Slope30N5, BTC_RoofSlopes::Slope30N6,

        BTC_RoofSlopes::Peak30NS1, BTC_RoofSlopes::Peak30NS2, BTC_RoofSlopes::Peak30NS3, BTC_RoofSlopes::Peak30NS4, BTC_RoofSlopes::Peak30NS5, BTC_RoofSlopes::Peak30NS6,
        BTC_RoofSlopes::Peak30WE1, BTC_RoofSlopes::Peak30WE2, BTC_RoofSlopes::Peak30WE3, BTC_RoofSlopes::Peak30WE4, BTC_RoofSlopes::Peak30WE5, BTC_RoofSlopes::Peak30WE6,
        BTC_RoofSlopes::Peak30Quad1, BTC_RoofSlopes::Peak30Quad2, BTC_RoofSlopes::Peak30Quad3, BTC_RoofSlopes::Peak30Quad4, BTC_RoofSlopes::Peak30Quad5, BTC_RoofSlopes::Peak30Quad6,

        BTC_RoofSlopes::Inner1, BTC_RoofSlopes::Inner2, BTC_RoofSlopes::Inner3,
        BTC_RoofSlopes::Outer1, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::Outer3,
        BTC_RoofSlopes::InnerPt5, BTC_RoofSlopes::InnerOnePt5, BTC_RoofSlopes::InnerTwoPt5,
        BTC_RoofSlopes::OuterPt5, BTC_RoofSlopes::OuterOnePt5, BTC_RoofSlopes::OuterTwoPt5,
        BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerSW3,
        BTC_RoofSlopes::CornerNE1, BTC_RoofSlopes::CornerNE2, BTC_RoofSlopes::CornerNE3,

        BTC_RoofSlopes::InnerSlope30SE1, BTC_RoofSlopes::InnerSlope30SE2, BTC_RoofSlopes::InnerSlope30SE3, BTC_RoofSlopes::InnerSlope30SE4, BTC_RoofSlopes::InnerSlope30SE5, BTC_RoofSlopes::InnerSlope30SE6,
        BTC_RoofSlopes::InnerSlope30NE1, BTC_RoofSlopes::InnerSlope30NE2, BTC_RoofSlopes::InnerSlope30NE3, BTC_RoofSlopes::InnerSlope30NE4, BTC_RoofSlopes::InnerSlope30NE5, BTC_RoofSlopes::InnerSlope30NE6,
        BTC_RoofSlopes::InnerSlope30NW1, BTC_RoofSlopes::InnerSlope30NW2, BTC_RoofSlopes::InnerSlope30NW3, BTC_RoofSlopes::InnerSlope30NW4, BTC_RoofSlopes::InnerSlope30NW5, BTC_RoofSlopes::InnerSlope30NW6,
        BTC_RoofSlopes::InnerSlope30SW1, BTC_RoofSlopes::InnerSlope30SW2, BTC_RoofSlopes::InnerSlope30SW3, BTC_RoofSlopes::InnerSlope30SW4, BTC_RoofSlopes::InnerSlope30SW5, BTC_RoofSlopes::InnerSlope30SW6,

        BTC_RoofSlopes::OuterSlope30SE1, BTC_RoofSlopes::OuterSlope30SE2, BTC_RoofSlopes::OuterSlope30SE3, BTC_RoofSlopes::OuterSlope30SE4, BTC_RoofSlopes::OuterSlope30SE5, BTC_RoofSlopes::OuterSlope30SE6,
        BTC_RoofSlopes::OuterSlope30NE1, BTC_RoofSlopes::OuterSlope30NE2, BTC_RoofSlopes::OuterSlope30NE3, BTC_RoofSlopes::OuterSlope30NE4, BTC_RoofSlopes::OuterSlope30NE5, BTC_RoofSlopes::OuterSlope30NE6,
        BTC_RoofSlopes::OuterSlope30NW1, BTC_RoofSlopes::OuterSlope30NW2, BTC_RoofSlopes::OuterSlope30NW3, BTC_RoofSlopes::OuterSlope30NW4, BTC_RoofSlopes::OuterSlope30NW5, BTC_RoofSlopes::OuterSlope30NW6,
        BTC_RoofSlopes::OuterSlope30SW1, BTC_RoofSlopes::OuterSlope30SW2, BTC_RoofSlopes::OuterSlope30SW3, BTC_RoofSlopes::OuterSlope30SW4, BTC_RoofSlopes::OuterSlope30SW5, BTC_RoofSlopes::OuterSlope30SW6,
    };

    static const BTC_RoofCaps::TileEnum mapCap[] = {
        BTC_RoofCaps::CapRiseE1, BTC_RoofCaps::CapRiseE2, BTC_RoofCaps::CapRiseE3,
        BTC_RoofCaps::CapFallE1, BTC_RoofCaps::CapFallE2, BTC_RoofCaps::CapFallE3,
        BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapRiseS3,
        BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapFallS3,
        BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E,
        BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E,
        BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E,
        BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapS3,
        BTC_RoofCaps::CapGapE1, BTC_RoofCaps::CapGapE2, BTC_RoofCaps::CapGapE3,

        BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowRiseS2,
        BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowFallS2,
        BTC_RoofCaps::CapShallowRiseE1, BTC_RoofCaps::CapShallowRiseE2,
        BTC_RoofCaps::CapShallowFallE1, BTC_RoofCaps::CapShallowFallE2,

        BTC_RoofCaps::CapSlope30RiseE1, BTC_RoofCaps::CapSlope30RiseE2, BTC_RoofCaps::CapSlope30RiseE3, BTC_RoofCaps::CapSlope30RiseE4, BTC_RoofCaps::CapSlope30RiseE5, BTC_RoofCaps::CapSlope30RiseE6,
        BTC_RoofCaps::CapSlope30FallE1, BTC_RoofCaps::CapSlope30FallE2, BTC_RoofCaps::CapSlope30FallE3, BTC_RoofCaps::CapSlope30FallE4, BTC_RoofCaps::CapSlope30FallE5, BTC_RoofCaps::CapSlope30FallE6,
        BTC_RoofCaps::CapSlope30RiseS1, BTC_RoofCaps::CapSlope30RiseS2, BTC_RoofCaps::CapSlope30RiseS3, BTC_RoofCaps::CapSlope30RiseS4, BTC_RoofCaps::CapSlope30RiseS5, BTC_RoofCaps::CapSlope30RiseS6,
        BTC_RoofCaps::CapSlope30FallS1, BTC_RoofCaps::CapSlope30FallS2, BTC_RoofCaps::CapSlope30FallS3, BTC_RoofCaps::CapSlope30FallS4, BTC_RoofCaps::CapSlope30FallS5, BTC_RoofCaps::CapSlope30FallS6,
        BTC_RoofCaps::CapPeak30E1, BTC_RoofCaps::CapPeak30E2, BTC_RoofCaps::CapPeak30E3, BTC_RoofCaps::CapPeak30E4, BTC_RoofCaps::CapPeak30E5, BTC_RoofCaps::CapPeak30E6,
        BTC_RoofCaps::CapPeak30S1, BTC_RoofCaps::CapPeak30S2, BTC_RoofCaps::CapPeak30S3, BTC_RoofCaps::CapPeak30S4, BTC_RoofCaps::CapPeak30S5, BTC_RoofCaps::CapPeak30S6,
    };

    if (tile >= CapRiseE1) {
        Q_ASSERT(tile - CapRiseE1 < sizeof(mapCap) / sizeof(mapCap[0]));
        return mapCap[tile - CapRiseE1];
    }

    Q_ASSERT(tile < sizeof(mapSlope) / sizeof(mapSlope[0]));

    return mapSlope[tile];
}

QRect RoofObject::westEdge()
{
    QRect r = bounds();
    if (mType == SlopeW)
        return QRect(r.left(), r.top(),
                     actualWidth(), r.height());
    if (mType == PeakNS) {
        return QRect(r.left(), r.top(),
                     slopeThickness(), r.height());
    }
    return QRect();
}

QRect RoofObject::northEdge()
{
    QRect r = bounds();
    if (mType == SlopeN)
        return QRect(r.left(), r.top(),
                     r.width(), actualHeight());
    if (mType == PeakWE) {
        return QRect(r.left(), r.top(),
                     r.width(), slopeThickness());
    }
    return QRect();
}

QRect RoofObject::eastEdge()
{
    QRect r = bounds();
    if (mType == SlopeE || mType == CornerInnerSW/*hack*/)
        return QRect(r.right() - actualWidth() + 1, r.top(),
                     actualWidth(), r.height());
    if (mType == PeakNS) {
        return QRect(r.right() - slopeThickness() + 1, r.top(),
                     slopeThickness(), r.height());
    }
    if (mType == DormerN) {
        return QRect(r.right() - slopeThickness() + 1, r.top(),
                     slopeThickness(), r.height() - slopeThickness());
    }
    if (mType == DormerS) {
        return QRect(r.right() - slopeThickness() + 1,
                     r.top() + slopeThickness(),
                     slopeThickness(),
                     r.height() - slopeThickness());
    }
    return QRect();
}

QRect RoofObject::southEdge()
{
    QRect r = bounds();
    if (mType == SlopeS || mType == CornerInnerNE/*hack*/)
        return QRect(r.left(), r.bottom() - actualHeight() + 1,
                     r.width(), actualHeight());
    if (mType == PeakWE) {
        return QRect(r.left(), r.bottom() - slopeThickness() + 1,
                     r.width(), slopeThickness());
    }
    if (mType == DormerW) {
        return QRect(r.left(), r.bottom() - slopeThickness() + 1,
                     r.width() - slopeThickness(), slopeThickness());
    }
    if (mType == DormerE) {
        return QRect(r.left() + slopeThickness(),
                     r.bottom() - slopeThickness() + 1,
                     r.width() - slopeThickness(),
                     slopeThickness());
    }
    return QRect();
}

#if 0
QRect RoofObject::northCapRise()
{
    if (!isCappedN())
        return QRect();
    QRect r = bounds();
    switch (mType) {
    case ShallowSlopeW:
        return QRect(r.x(), r.top(), r.width(), 1);
    case ShallowPeakNS:
        return QRect(r.x(), r.top(), r.width() / 2, 1);
    }
    return QRect();
}

QRect RoofObject::northCapFall()
{
    if (!isCappedN())
        return QRect();
    QRect r = bounds();
    switch (mType) {
    case ShallowSlopeE:
        return QRect(r.x(), r.top(), r.width(), 1);
    case ShallowPeakNS:
        return QRect(r.center().x(), r.top(), r.width() / 2, 1);
    }
    return QRect();
}

QRect RoofObject::southCapRise()
{
    if (!isCappedS())
        return QRect();
    QRect r = bounds();
    switch (mType) {
    case ShallowSlopeW:
        return QRect(r.x(), r.bottom() + 1, r.width(), 1);
    case ShallowPeakNS:
        return QRect(r.x(), r.bottom() + 1, r.width() / 2, 1);
    }
    return QRect();
}

QRect RoofObject::southCapFall()
{
    if (!isCappedS())
        return QRect();
    QRect r = bounds();
    switch (mType) {
    case ShallowSlopeE:
        return QRect(r.x(), r.bottom() + 1, r.width(), 1);
    case ShallowPeakNS:
        return QRect(r.center().x(), r.bottom() + 1, r.width() / 2, 1);
    }
    return QRect();
}

QRect RoofObject::westGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedW)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeE || mType == FlatTop
            || mType == CornerInnerSW
            || mType == CornerInnerNW) {
        return QRect(r.left(), r.top(), 1, r.height());
    }
    if ((mType == PeakWE || mType == DormerW) && mHeight > 6)
        return QRect(r.left(), r.top() + 3, 1, r.height() - 6);
    return QRect();
}

QRect RoofObject::northGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedN)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeS || mType == FlatTop
            || mType == CornerInnerNW
            || mType == CornerInnerNE) {
        return QRect(r.left(), r.top(), r.width(), 1);
    }
    if ((mType == PeakNS || mType == DormerN) && mWidth > 6)
        return QRect(r.left() + 3, r.top(), r.width() - 6, 1);
    return QRect();
}

QRect RoofObject::eastGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedE)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeW || mType == FlatTop
            || mType == CornerInnerSE
            || mType == CornerInnerNE) {
        return QRect(r.right() + 1, r.top(), 1, r.height());
    }
    if ((mType == PeakWE || mType == DormerE) && mHeight > 6)
        return QRect(r.right() + 1, r.top() + 3, 1, r.height() - 6);
    return QRect();
}

QRect RoofObject::southGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedS)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeN || mType == FlatTop
            || mType == CornerInnerSE
            || mType == CornerInnerSW) {
        return QRect(r.left(), r.bottom() + 1, r.width(), 1);
    }
    if ((mType == PeakNS || mType == DormerS) && mWidth > 6)
        return QRect(r.left() + 3, r.bottom() + 1, r.width() - 6, 1);
    return QRect();
}
#endif

QRect RoofObject::flatTop()
{
    QRect r = bounds();
    if (mType == FlatTop)
        return r;
    if ((mType == PeakWE || mType == DormerW || mType == DormerE) && mHeight > 6)
        return QRect(r.left(), r.top() + 3, r.width(), r.height() - 6);
    if ((mType == PeakNS || mType == DormerN || mType == DormerS) && mWidth > 6)
        return QRect(r.left() + 3, r.top(), r.width() - 6, r.height());
    return QRect();
}

class RoofTiler
{
public:
    RoofTiler(QVector<RoofObject::RoofTile> &tiles, int tilesW, int tilesH)
        : tiles(tiles)
        , tilesW(tilesW)
        , tilesH(tilesH)
    {
    }

    void tile(int x, int y, RoofObject::RoofTile roofTile)
    {
        if (x < 0 || x >= tilesW || y < 0 || y >= tilesH) {
            return;
        }
        tiles[x + y * tilesW] = roofTile;
    }

    void row(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int dx = 0; dx < span; dx++) {
            tile(x + dx, y, roofTile);
        }
    }

    void column(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int dy = 0; dy < span; dy++) {
            tile(x, y + dy, roofTile);
        }
    }

    void rowIncr(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int i = 0; i < span; i++) {
            tile(x + i, y, static_cast<RoofObject::RoofTile>(roofTile + i));
        }
    }

    void rowDecr(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int i = 0; i < span; i++) {
            tile(x - i, y, static_cast<RoofObject::RoofTile>(roofTile + i));
        }
    }

    void columnIncr(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int i = 0; i < span; i++) {
            tile(x, y + i, static_cast<RoofObject::RoofTile>(roofTile + i));
        }
    }

    void columnDecr(int x, int y, int span, RoofObject::RoofTile roofTile)
    {
        for (int i = 0; i < span; i++) {
            tile(x, y - i, static_cast<RoofObject::RoofTile>(roofTile + i));
        }
    }

    void fallColumn(int x, int y, int span)
    {

    }

    QVector<RoofObject::RoofTile> &tiles;
    int tilesW;
    int tilesH;
};

class QuadRoofTiler : public RoofTiler
{
public:
    QuadRoofTiler(QVector<RoofObject::RoofTile> &tiles, int tilesWH)
        : RoofTiler(tiles, tilesWH, tilesWH)
    {
    }

    void ring(int x, int y, int wh,
              RoofObject::RoofTile cornerNW, RoofObject::RoofTile cornerNE, RoofObject::RoofTile cornerSE, RoofObject::RoofTile cornerSW,
              RoofObject::RoofTile slopeW, RoofObject::RoofTile slopeN, RoofObject::RoofTile slopeE, RoofObject::RoofTile slopeS)
    {
        tile(x, y, cornerNW);
        tile(x + wh - 1, y, cornerNE);
        tile(x + wh - 1, y + wh - 1, cornerSE);
        tile(x, y + wh - 1, cornerSW);
        for (int dx = 1; dx < wh - 1; dx++) {
            tile(x + dx, y, slopeN);
            tile(x + dx, y + wh - 1, slopeS);
        }
        for (int dy = 1; dy < wh - 1; dy++) {
            tile(x, y + dy, slopeW);
            tile(x + wh - 1, y + dy, slopeE);
        }
    }
};

QVector<RoofObject::RoofTile> RoofObject::slopeTiles(QRect &b)
{
    QRect r = bounds();
    b = r;
    QVector<RoofTile> pat, ret;

    switch (mType) {
    case SlopeE:
        if (mDepth == Three) pat << SlopeE3 << SlopeE2 << SlopeE1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5E << SlopeE2 << SlopeE1;
        else if (mDepth == Two) pat << SlopeE2 << SlopeE1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5E << SlopeE1;
        else if (mDepth == One) pat << SlopeE1;
        else if (mDepth == Point5) pat << SlopePt5E;
        for (int y = 0; y < mHeight; y++) ret += pat;
        break;
    case SlopeS:
        if (mDepth == Three) pat << SlopeS3 << SlopeS2 << SlopeS1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5S << SlopeS2 << SlopeS1;
        else if (mDepth == Two) pat << SlopeS2 << SlopeS1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5S << SlopeS1;
        else if (mDepth == One) pat << SlopeS1;
        else if (mDepth == Point5) pat << SlopePt5S;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case PeakWE:
        b = QRect(r.left(), r.bottom() + 1 - slopeThickness(), r.width(), slopeThickness());
        if (mDepth == Three) pat << SlopeS3 << SlopeS2 << SlopeS1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5S << SlopeS2 << SlopeS1;
        else if (mDepth == Two) pat << SlopeS2 << SlopeS1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5S << SlopeS1;
        else if (mDepth == One) pat << SlopeS1;
        else if (mDepth == Point5) pat << SlopePt5S;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case PeakNS:
        b = QRect(r.right() + 1 - slopeThickness(), r.top(), slopeThickness(), r.height());
        if (mDepth == Three) pat << SlopeE3 << SlopeE2 << SlopeE1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5E << SlopeE2 << SlopeE1;
        else if (mDepth == Two) pat << SlopeE2 << SlopeE1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5E << SlopeE1;
        else if (mDepth == One) pat << SlopeE1;
        else if (mDepth == Point5) pat << SlopePt5E;
        for (int y = 0; y < mHeight; y++) ret += pat;
        break;

    case DormerW:
        b = QRect(r.left(), r.bottom() - slopeThickness() + 1,
                  r.width() - slopeThickness(), slopeThickness());
        if (mDepth == Three) pat << SlopeS3 << SlopeS2 << SlopeS1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5S << SlopeS2 << SlopeS1;
        else if (mDepth == Two) pat << SlopeS2 << SlopeS1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5S << SlopeS1;
        else if (mDepth == One) pat << SlopeS1;
        else if (mDepth == Point5) pat << SlopePt5S;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < b.width(); x++)
                ret += pat[y];
        break;
     case DormerE:
        b = QRect(r.left() + slopeThickness(),
                  r.bottom() - slopeThickness() + 1,
                  r.width() - slopeThickness(),
                  slopeThickness());
        if (mDepth == Three) pat << SlopeS3 << SlopeS2 << SlopeS1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5S << SlopeS2 << SlopeS1;
        else if (mDepth == Two) pat << SlopeS2 << SlopeS1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5S << SlopeS1;
        else if (mDepth == One) pat << SlopeS1;
        else if (mDepth == Point5) pat << SlopePt5S;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < b.width(); x++)
                ret += pat[y];
        break;
    case DormerN:
        b = QRect(r.right() - slopeThickness() + 1, r.top(),
                  slopeThickness(), r.height() - slopeThickness());
        if (mDepth == Three) pat << SlopeE3 << SlopeE2 << SlopeE1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5E << SlopeE2 << SlopeE1;
        else if (mDepth == Two) pat << SlopeE2 << SlopeE1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5E << SlopeE1;
        else if (mDepth == One) pat << SlopeE1;
        else if (mDepth == Point5) pat << SlopePt5E;
        for (int y = 0; y < b.height(); y++) ret += pat;
        break;
    case DormerS:
        b = QRect(r.right() - slopeThickness() + 1,
                  r.top() + slopeThickness(),
                  slopeThickness(),
                  r.height() - slopeThickness());
        if (mDepth == Three) pat << SlopeE3 << SlopeE2 << SlopeE1;
        else if (mDepth == TwoPoint5) pat << SlopeTwoPt5E << SlopeE2 << SlopeE1;
        else if (mDepth == Two) pat << SlopeE2 << SlopeE1;
        else if (mDepth == OnePoint5) pat << SlopeOnePt5E << SlopeE1;
        else if (mDepth == One) pat << SlopeE1;
        else if (mDepth == Point5) pat << SlopePt5E;
        for (int y = 0; y < b.height(); y++) ret += pat;
        break;

    case ShallowSlopeW:
        pat << ShallowSlopeW1;
        if (mWidth > 1) pat << ShallowSlopeW2;
        for (int y = 0; y < mHeight; y++) ret += pat;
        break;
    case ShallowSlopeE:
        if (mWidth > 1) pat << ShallowSlopeE2;
        pat << ShallowSlopeE1;
        for (int y = 0; y < mHeight; y++) ret += pat;
        break;
    case ShallowPeakNS:
        pat << ShallowSlopeW1;
        if (mWidth > 2) pat << ShallowSlopeW2 << ShallowSlopeE2;
        pat << ShallowSlopeE1;
        for (int y = 0; y < mHeight; y++) ret += pat;
        break;
    case ShallowSlopeN:
        pat << ShallowSlopeN1;
        if (mHeight > 1) pat << ShallowSlopeN2;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case ShallowSlopeS:
        if (mHeight > 1) pat << ShallowSlopeS2;
        pat << ShallowSlopeS1;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case ShallowPeakWE:
        pat << ShallowSlopeN1;
        if (mHeight > 2) pat << ShallowSlopeN2 << ShallowSlopeS2;
        pat << ShallowSlopeS1;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;

    case Slope30W:
        pat << Slope30W1;
        if (mWidth > 1) pat << Slope30W2;
        if (mWidth > 2) pat << Slope30W3;
        if (mWidth > 3) pat << Slope30W4;
        if (mWidth > 4) pat << Slope30W5;
        if (mWidth > 5) pat << Slope30W6;
        for (int y = 0; y < mHeight; y++) {
            ret += pat;
        }
        break;
    case Slope30N:
        pat << Slope30N1;
        if (mHeight > 1) pat << Slope30N2;
        if (mHeight > 2) pat << Slope30N3;
        if (mHeight > 3) pat << Slope30N4;
        if (mHeight > 4) pat << Slope30N5;
        if (mHeight > 5) pat << Slope30N6;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case Slope30E:
        if (mWidth > 5) pat << Slope30E6;
        if (mWidth > 4) pat << Slope30E5;
        if (mWidth > 3) pat << Slope30E4;
        if (mWidth > 2) pat << Slope30E3;
        if (mWidth > 1) pat << Slope30E2;
        pat << Slope30E1;
        for (int y = 0; y < mHeight; y++) {
            ret += pat;
        }
        break;
    case Slope30S:
        if (mHeight > 5) pat << Slope30S6;
        if (mHeight > 4) pat << Slope30S5;
        if (mHeight > 3) pat << Slope30S4;
        if (mHeight > 2) pat << Slope30S3;
        if (mHeight > 1) pat << Slope30S2;
        pat << Slope30S1;
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case Peak30WE:
        pat.resize(mHeight);
        pat[mHeight / 2] = Peak30NS1;
        if (mHeight >= 3) {
            pat[0] = Slope30N1;
            pat[mHeight - 1] = Slope30S1;
            pat[mHeight / 2] = Peak30NS2;
        }
        if (mHeight >= 5) {
            pat[1] = Slope30N2;
            pat[mHeight - 2] = Slope30S2;
            pat[mHeight / 2] = Peak30NS3;
        }
        if (mHeight >= 7) {
            pat[2] = Slope30N3;
            pat[mHeight - 3] = Slope30S3;
            pat[mHeight / 2] = Peak30NS4;
        }
        if (mHeight >= 9) {
            pat[3] = Slope30N4;
            pat[mHeight - 4] = Slope30S4;
            pat[mHeight / 2] = Peak30NS5;
        }
        if (mHeight >= 11) {
            pat[4] = Slope30N5;
            pat[mHeight - 5] = Slope30S5;
            pat[mHeight / 2] = Peak30NS6;
        }
        for (int y = 0; y < pat.size(); y++)
            for (int x = 0; x < mWidth; x++)
                ret += pat[y];
        break;
    case Peak30NS:
        pat.resize(mWidth);
        pat[mWidth / 2] = Peak30WE1;
        if (mWidth >= 3) {
            pat[0] = Slope30W1;
            pat[mWidth - 1] = Slope30E1;
            pat[mWidth / 2] = Peak30WE2;
        }
        if (mWidth >= 5) {
            pat[1] = Slope30W2;
            pat[mWidth - 2] = Slope30E2;
            pat[mWidth / 2] = Peak30WE3;
        }
        if (mWidth >= 7) {
            pat[2] = Slope30W3;
            pat[mWidth - 3] = Slope30E3;
            pat[mWidth / 2] = Peak30WE4;
        }
        if (mWidth >= 9) {
            pat[3] = Slope30W4;
            pat[mWidth - 4] = Slope30E4;
            pat[mWidth / 2] = Peak30WE5;
        }
        if (mWidth >= 11) {
            pat[4] = Slope30W5;
            pat[mWidth - 5] = Slope30E5;
            pat[mWidth / 2] = Peak30WE6;
        }
        for (int y = 0; y < mHeight; y++)
            ret += pat;
        break;
    case Peak30Quad: {
        ret.resize(mWidth * mHeight);
        ret.fill(RoofTile::TileCount);
        QuadRoofTiler tiler(ret, mWidth);
        if (mWidth == 1) {
            tiler.tile(0, 0, Peak30Quad1);
        }
        if (mWidth >= 3) {
            tiler.tile(mWidth / 2, mWidth / 2, Peak30Quad2);
            tiler.ring(0, 0, mWidth,
                         OuterSlope30NW1, OuterSlope30NE1, OuterSlope30SE1, OuterSlope30SW1,
                         Slope30W1, Slope30N1, Slope30E1, Slope30S1);
        }
        if (mWidth >= 5) {
            tiler.tile(mWidth / 2, mWidth / 2, Peak30Quad3);
            tiler.ring(1, 1, mWidth-2,
                         OuterSlope30NW2, OuterSlope30NE2, OuterSlope30SE2, OuterSlope30SW2,
                         Slope30W2, Slope30N2, Slope30E2, Slope30S2);
        }
        if (mWidth >= 7) {
            tiler.tile(mWidth / 2, mWidth / 2, Peak30Quad4);
            tiler.ring(2, 2, mWidth-4,
                         OuterSlope30NW3, OuterSlope30NE3, OuterSlope30SE3, OuterSlope30SW3,
                         Slope30W3, Slope30N3, Slope30E3, Slope30S3);
        }
        if (mWidth >= 9) {
            tiler.tile(mWidth / 2, mWidth / 2, Peak30Quad5);
            tiler.ring(3, 3, mWidth-6,
                         OuterSlope30NW4, OuterSlope30NE4, OuterSlope30SE4, OuterSlope30SW4,
                         Slope30W4, Slope30N4, Slope30E4, Slope30S4);
        }
        if (mWidth >= 11) {
            tiler.tile(mWidth / 2, mWidth / 2, Peak30Quad6);
            tiler.ring(4, 4, mWidth-8,
                         OuterSlope30NW5, OuterSlope30NE5, OuterSlope30SE5, OuterSlope30SW5,
                         Slope30W5, Slope30N5, Slope30E5, Slope30S5);
        }
        break;
    }
    case Dormer30W: {
        ret.resize(mWidth * mHeight);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, mHeight);
        if (mHeight <= 11) {
            RoofTile peakTile = Peak30NS1;
            if (mHeight == 11) {
                peakTile = Peak30NS6;
            } else if (mHeight == 9) {
                peakTile = Peak30NS5;
            } else if (mHeight == 7) {
                peakTile = Peak30NS4;
            } else if (mHeight == 5) {
                peakTile = Peak30NS3;
            } else if (mHeight == 3) {
                peakTile = Peak30NS2;
            } else if (mHeight == 1) {
                peakTile = Peak30NS1;
            }
            tiler.row(0, mHeight / 2, mWidth, peakTile);
        }
        int right = mWidth - 1;
        if (mHeight > 11) {
            tiler.tile(right, 5, InnerSlope30SE6);
            tiler.tile(right, mHeight - 6, InnerSlope30NE6);
            tiler.row(0, 5, right, Slope30N6);
            tiler.row(0, mHeight - 6, right, Slope30S6);
            --right;
        }
        if (mHeight >= 11) {
            tiler.tile(right, 4, InnerSlope30SE5);
            tiler.tile(right, mHeight - 5, InnerSlope30NE5);
            tiler.row(0, 4, right, Slope30N5);
            tiler.row(0, mHeight - 5, right, Slope30S5);
            --right;
        }
        if (mHeight >= 9) {
            tiler.tile(right, 3, InnerSlope30SE4);
            tiler.tile(right, mHeight - 4, InnerSlope30NE4);
            tiler.row(0, 3, right, Slope30N4);
            tiler.row(0, mHeight - 4, right, Slope30S4);
            --right;
        }
        if (mHeight >= 7) {
            tiler.tile(right, 2, InnerSlope30SE3);
            tiler.tile(right, mHeight - 3, InnerSlope30NE3);
            tiler.row(0, 2, right, Slope30N3);
            tiler.row(0, mHeight - 3, right, Slope30S3);
            --right;
        }
        if (mHeight >= 5) {
            tiler.tile(right, 1, InnerSlope30SE2);
            tiler.tile(right, mHeight - 2, InnerSlope30NE2);
            tiler.row(0, 1, right, Slope30N2);
            tiler.row(0, mHeight - 2, right, Slope30S2);
            --right;
        }
        if (mHeight >= 3) {
            tiler.tile(right, 0, InnerSlope30SE1);
            tiler.tile(right, mHeight - 1, InnerSlope30NE1);
            tiler.row(0, 0, right, Slope30N1);
            tiler.row(0, mHeight - 1, right, Slope30S1);
        }
        break;
    }
    case Dormer30E: {
        ret.resize(mWidth * mHeight);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, mHeight);
        if (mHeight <= 11) {
            RoofTile peakTile = Peak30NS1;
            if (mHeight == 11) {
                peakTile = Peak30NS6;
            } else if (mHeight == 9) {
                peakTile = Peak30NS5;
            } else if (mHeight == 7) {
                peakTile = Peak30NS4;
            } else if (mHeight == 5) {
                peakTile = Peak30NS3;
            } else if (mHeight == 3) {
                peakTile = Peak30NS2;
            } else if (mHeight == 1) {
                peakTile = Peak30NS1;
            }
            tiler.row(0, mHeight / 2, mWidth, peakTile);
        }
        int left = 0;
        if (mHeight > 11) {
            tiler.tile(left, 5, InnerSlope30SW6);
            tiler.tile(left, mHeight - 6, InnerSlope30NW6);
            ++left;
            tiler.row(left, 5, mWidth - left, Slope30N6);
            tiler.row(left, mHeight - 6, mWidth - left, Slope30S6);
        }
        if (mHeight >= 11) {
            tiler.tile(left, 4, InnerSlope30SW5);
            tiler.tile(left, mHeight - 5, InnerSlope30NW5);
            ++left;
            tiler.row(left, 4, mWidth - left, Slope30N5);
            tiler.row(left, mHeight - 5, mWidth - left, Slope30S5);
        }
        if (mHeight >= 9) {
            tiler.tile(left, 3, InnerSlope30SW4);
            tiler.tile(left, mHeight - 4, InnerSlope30NW4);
            ++left;
            tiler.row(left, 3, mWidth - left, Slope30N4);
            tiler.row(left, mHeight - 4, mWidth - left, Slope30S4);
        }
        if (mHeight >= 7) {
            tiler.tile(left, 2, InnerSlope30SW3);
            tiler.tile(left, mHeight - 3, InnerSlope30NW3);
            ++left;
            tiler.row(left, 2, mWidth - left, Slope30N3);
            tiler.row(left, mHeight - 3, mWidth - left, Slope30S3);
        }
        if (mHeight >= 5) {
            tiler.tile(left, 1, InnerSlope30SW2);
            tiler.tile(left, mHeight - 2, InnerSlope30NW2);
            ++left;
            tiler.row(left, 1, mWidth - left, Slope30N2);
            tiler.row(left, mHeight - 2, mWidth - left, Slope30S2);
        }
        if (mHeight >= 3) {
            tiler.tile(left, 0, InnerSlope30SW1);
            tiler.tile(left, mHeight - 1, InnerSlope30NW1);
            ++left;
            tiler.row(left, 0, mWidth - left, Slope30N1);
            tiler.row(left, mHeight - 1, mWidth - left, Slope30S1);
        }
        break;
    }
    case Dormer30N: {
        ret.resize(mWidth * mHeight);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, mHeight);
        if (mWidth <= 11) {
            RoofTile peakTile = Peak30NS1;
            if (mWidth == 11) {
                peakTile = Peak30WE6;
            } else if (mWidth == 9) {
                peakTile = Peak30WE5;
            } else if (mWidth == 7) {
                peakTile = Peak30WE4;
            } else if (mWidth == 5) {
                peakTile = Peak30WE3;
            } else if (mWidth == 3) {
                peakTile = Peak30WE2;
            } else if (mWidth == 1) {
                peakTile = Peak30WE1;
            }
            tiler.column(mWidth / 2, 0, mHeight, peakTile);
        }
        int bottom = mHeight - 1;
        if (mWidth > 11) {
            tiler.tile(5, bottom, InnerSlope30SE6);
            tiler.tile(mWidth - 6, bottom, InnerSlope30SW6);
            tiler.column(5, 0, bottom, Slope30W6);
            tiler.column(mWidth - 6, 0, bottom, Slope30E6);
            --bottom;
        }
        if (mWidth >= 11) {
            tiler.tile(4, bottom, InnerSlope30SE5);
            tiler.tile(mWidth - 5, bottom, InnerSlope30SW5);
            tiler.column(4, 0, bottom, Slope30W5);
            tiler.column(mWidth - 5, 0, bottom, Slope30E5);
            --bottom;
        }
        if (mWidth >= 9) {
            tiler.tile(3, bottom, InnerSlope30SE4);
            tiler.tile(mWidth - 4, bottom, InnerSlope30SW4);
            tiler.column(3, 0, bottom, Slope30W4);
            tiler.column(mWidth - 4, 0, bottom, Slope30E4);
            --bottom;
        }
        if (mWidth >= 7) {
            tiler.tile(2, bottom, InnerSlope30SE3);
            tiler.tile(mWidth - 3, bottom, InnerSlope30SW3);
            tiler.column(2, 0, bottom, Slope30W3);
            tiler.column(mWidth - 3, 0, bottom, Slope30E3);
            --bottom;
        }
        if (mWidth >= 5) {
            tiler.tile(1, bottom, InnerSlope30SE2);
            tiler.tile(mWidth - 2, bottom, InnerSlope30SW2);
            tiler.column(1, 0, bottom, Slope30W2);
            tiler.column(mWidth - 2, 0, bottom, Slope30E2);
            --bottom;
        }
        if (mWidth >= 3) {
            tiler.tile(0, bottom, InnerSlope30SE1);
            tiler.tile(mWidth - 1, bottom, InnerSlope30SW1);
            tiler.column(0, 0, bottom, Slope30W1);
            tiler.column(mWidth - 1, 0, bottom, Slope30E1);
        }
        break;
    }
    case Dormer30S: {
        ret.resize(mWidth * mHeight);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, mHeight);
        if (mWidth <= 11) {
            RoofTile peakTile = Peak30NS1;
            if (mWidth == 11) {
                peakTile = Peak30WE6;
            } else if (mWidth == 9) {
                peakTile = Peak30WE5;
            } else if (mWidth == 7) {
                peakTile = Peak30WE4;
            } else if (mWidth == 5) {
                peakTile = Peak30WE3;
            } else if (mWidth == 3) {
                peakTile = Peak30WE2;
            } else if (mWidth == 1) {
                peakTile = Peak30WE1;
            }
            tiler.column(mWidth / 2, 0, mHeight, peakTile);
        }
        int top = 0;
        if (mWidth > 11) {
            tiler.tile(5, top, InnerSlope30NE6);
            tiler.tile(mWidth - 6, top, InnerSlope30NW6);
            ++top;
            tiler.column(5, top, mHeight - top, Slope30W6);
            tiler.column(mWidth - 6, top, mHeight - top, Slope30E6);
        }
        if (mWidth >= 11) {
            tiler.tile(4, top, InnerSlope30NE5);
            tiler.tile(mWidth - 5, top, InnerSlope30NW5);
            ++top;
            tiler.column(4, top, mHeight - top, Slope30W5);
            tiler.column(mWidth - 5, top, mHeight - top, Slope30E5);
        }
        if (mWidth >= 9) {
            tiler.tile(3, top, InnerSlope30NE4);
            tiler.tile(mWidth - 4, top, InnerSlope30NW4);
            ++top;
            tiler.column(3, top, mHeight - top, Slope30W4);
            tiler.column(mWidth - 4, top, mHeight - top, Slope30E4);
        }
        if (mWidth >= 7) {
            tiler.tile(2, top, InnerSlope30NE3);
            tiler.tile(mWidth - 3, top, InnerSlope30NW3);
            ++top;
            tiler.column(2, top, mHeight - top, Slope30W3);
            tiler.column(mWidth - 3, top, mHeight - top, Slope30E3);
        }
        if (mWidth >= 5) {
            tiler.tile(1, top, InnerSlope30NE2);
            tiler.tile(mWidth - 2, top, InnerSlope30NW2);
            ++top;
            tiler.column(1, top, mHeight - top, Slope30W2);
            tiler.column(mWidth - 2, top, mHeight - top, Slope30E2);
        }
        if (mWidth >= 3) {
            tiler.tile(0, top, InnerSlope30NE1);
            tiler.tile(mWidth - 1, top, InnerSlope30NW1);
            ++top;
            tiler.column(0, top, mHeight - top, Slope30W1);
            tiler.column(mWidth - 1, top, mHeight - top, Slope30E1);
        }
        break;
    }
    default:
        break;
    }
    return ret;
}

QVector<RoofObject::RoofTile> RoofObject::westCapTiles(QRect &b)
{
    QRect r = bounds();
    b = QRect(r.left(), r.top(), 1, r.height());
    QVector<RoofTile> ret;
    if (!isCappedW()) return ret;

    switch (mType) {
    case PeakWE:
    case DormerW:
        if (mDepth == Three) {
            ret << CapFallE1 << CapFallE2 << CapFallE3;
            for (int y = 0; y < mHeight - 6; y++) ret << CapGapE3;
            ret << CapRiseE3 << CapRiseE2 << CapRiseE1;
        } else if (mDepth == TwoPoint5)
            ret << CapFallE1 << CapFallE2 << PeakTwoPt5E << CapRiseE2 << CapRiseE1;
        else if (mDepth == Two)
            ret << CapFallE1 << CapFallE2 << CapRiseE2 << CapRiseE1;
        else if (mDepth == OnePoint5)
            ret << CapFallE1 << PeakOnePt5E << CapRiseE1;
        else if (mDepth == One)
            ret << CapFallE1 << CapRiseE1;
        else if (mDepth == Point5)
            ret << PeakPt5E;
        break;
    case SlopeN:
    case CornerOuterNE:
    case CornerInnerSE:
        if (mDepth == Three) {
            ret << CapFallE1 << CapFallE2 << CapFallE3;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapFallE1 << CapFallE2;
            b.adjust(0,0,0,-1);
        } else if (mDepth == Two)
            ret << CapFallE1 << CapFallE2;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapFallE1;
            b.adjust(0,0,0,-1);
        } else if (mDepth == One)
            ret << CapFallE1;
        else if (mDepth == Point5) {}
        break;
    case SlopeS:
    case CornerInnerNE:
    case CornerOuterSE:
        if (mDepth == Three) {
            ret << CapRiseE3 << CapRiseE2 << CapRiseE1;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapRiseE2 << CapRiseE1;
            b.adjust(0,1,0,0);
        } else if (mDepth == Two)
            ret << CapRiseE2 << CapRiseE1;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapRiseE1;
            b.adjust(0,1,0,0);
        } else if (mDepth == One)
            ret << CapRiseE1;
        else if (mDepth == Point5) {}
        break;
    case ShallowSlopeN:
        ret << CapShallowFallE1;
        if (mHeight > 1) ret << CapShallowFallE2;
        break;
    case ShallowSlopeS:
        if (mHeight > 1) ret << CapShallowRiseE2;
        ret << CapShallowRiseE1;
        break;
    case ShallowPeakWE:
        ret << CapShallowFallE1;
        if (mHeight > 2) ret << CapShallowFallE2 << CapShallowRiseE2;
        ret << CapShallowRiseE1;
    case Slope30W:
        break;
    case Slope30N:
    case CornerSlope30InnerSE:
    case CornerSlope30OuterNE:
        ret += CapSlope30FallE1;
        if (mHeight > 1) ret += CapSlope30FallE2;
        if (mHeight > 2) ret += CapSlope30FallE3;
        if (mHeight > 3) ret += CapSlope30FallE4;
        if (mHeight > 4) ret += CapSlope30FallE5;
        if (mHeight > 5) ret += CapSlope30FallE6;
        break;
    case Slope30E:
        break;
    case Slope30S:
    case CornerSlope30InnerNE:
    case CornerSlope30OuterSE:
        if (mHeight > 5) ret += CapSlope30RiseE6;
        if (mHeight > 4) ret += CapSlope30RiseE5;
        if (mHeight > 3) ret += CapSlope30RiseE4;
        if (mHeight > 2) ret += CapSlope30RiseE3;
        if (mHeight > 1) ret += CapSlope30RiseE2;
        ret += CapSlope30RiseE1;
        break;
    case Peak30NS:
        break;
    case Peak30WE:
        ret.resize(mHeight);
        ret[mHeight / 2] = CapPeak30E1;
        if (mHeight >= 3) {
            ret[0] = CapSlope30FallE1;
            ret[mHeight - 1] = CapSlope30RiseE1;
            ret[mHeight / 2] = CapPeak30E2;
        }
        if (mHeight >= 5) {
            ret[1] = CapSlope30FallE2;
            ret[mHeight - 2] = CapSlope30RiseE2;
            ret[mHeight / 2] = CapPeak30E3;
        }
        if (mHeight >= 7) {
            ret[2] = CapSlope30FallE3;
            ret[mHeight - 3] = CapSlope30RiseE3;
            ret[mHeight / 2] = CapPeak30E4;
        }
        if (mHeight >= 9) {
            ret[3] = CapSlope30FallE4;
            ret[mHeight - 4] = CapSlope30RiseE4;
            ret[mHeight / 2] = CapPeak30E5;
        }
        if (mHeight >= 11) {
            ret[4] = CapSlope30FallE5;
            ret[mHeight - 5] = CapSlope30RiseE5;
            ret[mHeight / 2] = CapPeak30E6;
        }
        break;
    case Dormer30W: {
        ret.resize(mHeight);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, 1, mHeight);
        tiler.columnIncr(0, 0, std::min(mHeight / 2, 6), CapSlope30FallE1);
        tiler.columnDecr(0, mHeight - 1, std::min(mHeight / 2, 6), CapSlope30RiseE1);
        RoofTile peakTile = TileCount;
        if (mHeight == 1) {
            peakTile = CapPeak30E1;
        } else if (mHeight == 3) {
            peakTile = CapPeak30E2;
        } else if (mHeight == 5) {
            peakTile = CapPeak30E3;
        } else if (mHeight == 7) {
            peakTile = CapPeak30E4;
        } else if (mHeight == 9) {
            peakTile = CapPeak30E5;
        } else if (mHeight == 11) {
            peakTile = CapPeak30E6;
        }
        if (mHeight > 12) {
            tiler.column(0, 6, mHeight - 12, CapGapE3);
        } else if (mHeight < 12) {
            ret[mHeight / 2] = peakTile;
        }
        break;
    }
    case FlatTop:
    case CornerInnerSW:
    case CornerInnerNW: {
        RoofTile tile = CapGapE3;
        if (mDepth == Three) tile = CapGapE3;
        else if (mDepth == Two) tile = CapGapE2;
        else if (mDepth == One) tile = CapGapE1;
        else break;
        for (int y = 0; y < mHeight; y++) ret << tile;
        break;
    }
    default:
        break;
    }
    return ret;
}

QVector<RoofObject::RoofTile> RoofObject::eastCapTiles(QRect &b)
{
    QRect r = bounds();
    b = QRect(r.right() + 1, r.top(), 1, r.height());
    QVector<RoofTile> ret;
    if (!isCappedE()) return ret;

    switch (mType) {
    case PeakWE:
    case DormerE:
        if (mDepth == Three) {
            ret << CapFallE1 << CapFallE2 << CapFallE3;
            for (int y = 0; y < mHeight - 6; y++) ret << CapGapE3;
            ret << CapRiseE3 << CapRiseE2 << CapRiseE1;
        } else if (mDepth == TwoPoint5)
            ret << CapFallE1 << CapFallE2 << PeakTwoPt5E << CapRiseE2 << CapRiseE1;
        else if (mDepth == Two)
            ret << CapFallE1 << CapFallE2 << CapRiseE2 << CapRiseE1;
        else if (mDepth == OnePoint5)
            ret << CapFallE1 << PeakOnePt5E << CapRiseE1;
        else if (mDepth == One)
            ret << CapFallE1 << CapRiseE1;
        else if (mDepth == Point5)
            ret << PeakPt5E;
        break;
    case SlopeN:
    case CornerInnerSW:
    case CornerOuterNW:
        if (mDepth == Three) {
            ret << CapFallE1 << CapFallE2 << CapFallE3;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapFallE1 << CapFallE2;
            b.adjust(0,0,0,-1);
        } else if (mDepth == Two)
            ret << CapFallE1 << CapFallE2;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapFallE1;
            b.adjust(0,0,0,-1);
        } else if (mDepth == One)
            ret << CapFallE1;
        else if (mDepth == Point5) {}
        break;
    case SlopeS:
    case CornerInnerNW:
    case CornerOuterSW:
        if (mDepth == Three) {
            ret << CapRiseE3 << CapRiseE2 << CapRiseE1;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapRiseE2 << CapRiseE1;
            b.adjust(0,1,0,0);
        } else if (mDepth == Two)
            ret << CapRiseE2 << CapRiseE1;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapRiseE1;
            b.adjust(0,1,0,0);
        } else if (mDepth == One)
            ret << CapRiseE1;
        else if (mDepth == Point5) {}
        break;
    case ShallowSlopeN:
        ret += CapShallowFallE1;
        if (mHeight > 1) ret += CapShallowFallE2;
        break;
    case ShallowSlopeS:
        if (mHeight > 1) ret += CapShallowRiseE2;
        ret += CapShallowRiseE1;
        break;
    case ShallowPeakWE:
        ret += CapShallowFallE1;
        if (mHeight > 2) ret << CapShallowFallE2 << CapShallowRiseE2;
        ret += CapShallowRiseE1;
        break;
    case Slope30W:
        break;
    case Slope30N:
    case CornerSlope30InnerSW:
    case CornerSlope30OuterNW:
        ret += CapSlope30FallE1;
        if (mHeight > 1) ret += CapSlope30FallE2;
        if (mHeight > 2) ret += CapSlope30FallE3;
        if (mHeight > 3) ret += CapSlope30FallE4;
        if (mHeight > 4) ret += CapSlope30FallE5;
        if (mHeight > 5) ret += CapSlope30FallE6;
        break;
    case Slope30E:
        break;
    case Slope30S:
    case CornerSlope30InnerNW:
    case CornerSlope30OuterSW:
        if (mHeight > 5) ret += CapSlope30RiseE6;
        if (mHeight > 4) ret += CapSlope30RiseE5;
        if (mHeight > 3) ret += CapSlope30RiseE4;
        if (mHeight > 2) ret += CapSlope30RiseE3;
        if (mHeight > 1) ret += CapSlope30RiseE2;
        ret += CapSlope30RiseE1;
        break;
    case Peak30WE:
        ret.resize(mHeight);
        ret[mHeight / 2] = CapPeak30E1;
        if (mHeight >= 3) {
            ret[0] = CapSlope30FallE1;
            ret[mHeight - 1] = CapSlope30RiseE1;
            ret[mHeight / 2] = CapPeak30E2;
        }
        if (mHeight >= 5) {
            ret[1] = CapSlope30FallE2;
            ret[mHeight - 2] = CapSlope30RiseE2;
            ret[mHeight / 2] = CapPeak30E3;
        }
        if (mHeight >= 7) {
            ret[2] = CapSlope30FallE3;
            ret[mHeight - 3] = CapSlope30RiseE3;
            ret[mHeight / 2] = CapPeak30E4;
        }
        if (mHeight >= 9) {
            ret[3] = CapSlope30FallE4;
            ret[mHeight - 4] = CapSlope30RiseE4;
            ret[mHeight / 2] = CapPeak30E5;
        }
        if (mHeight >= 11) {
            ret[4] = CapSlope30FallE5;
            ret[mHeight - 5] = CapSlope30RiseE5;
            ret[mHeight / 2] = CapPeak30E6;
        }
        break;
    case Dormer30E: {
        ret.resize(mHeight);
        ret.fill(TileCount);
        RoofTiler tiler(ret, 1, mHeight);
        tiler.columnIncr(0, 0, std::min(mHeight / 2, 6), CapSlope30FallE1);
        tiler.columnDecr(0, mHeight - 1, std::min(mHeight / 2, 6), CapSlope30RiseE1);
        RoofTile peakTile = TileCount;
        if (mHeight == 1) {
            peakTile = CapPeak30E1;
        } else if (mHeight == 3) {
            peakTile = CapPeak30E2;
        } else if (mHeight == 5) {
            peakTile = CapPeak30E3;
        } else if (mHeight == 7) {
            peakTile = CapPeak30E4;
        } else if (mHeight == 9) {
            peakTile = CapPeak30E5;
        } else if (mHeight == 11) {
            peakTile = CapPeak30E6;
        }
        if (mHeight > 12) {
            tiler.column(0, 6, mHeight - 12, CapGapE3);
        } else if (mHeight < 12) {
            ret[mHeight / 2] = peakTile;
        }
        break;
    }
    case FlatTop:
    case CornerInnerSE:
    case CornerInnerNE: {
        RoofTile tile = CapGapE3;
        if (mDepth == Three) tile = CapGapE3;
        else if (mDepth == Two) tile = CapGapE2;
        else if (mDepth == One) tile = CapGapE1;
        else break;
        for (int y = 0; y < mHeight; y++) ret << tile;
        break;
    }
    default:
        break;
    }
    return ret;
}

QVector<RoofObject::RoofTile> RoofObject::northCapTiles(QRect &b)
{
    QRect r = bounds();
    b = QRect(r.x(), r.top(), r.width(), 1);
    QVector<RoofTile> ret;
    if (!isCappedN()) return ret;

    switch (mType) {
    case PeakNS:
    case DormerN:
        if (mDepth == Three) {
            ret << CapRiseS1 << CapRiseS2 << CapRiseS3;
            for (int x = 0; x < mWidth - 6; x++) ret << CapGapS3;
            ret << CapFallS3 << CapFallS2 << CapFallS1;
        } else if (mDepth == TwoPoint5)
            ret << CapRiseS1 << CapRiseS2 << PeakTwoPt5S << CapFallS2 << CapFallS1;
        else if (mDepth == Two)
            ret << CapRiseS1 << CapRiseS2 << CapFallS2 << CapFallS1;
        else if (mDepth == OnePoint5)
            ret << CapRiseS1 << PeakOnePt5S << CapFallS1;
        else if (mDepth == One)
            ret << CapRiseS1 << CapFallS1;
        else if (mDepth == Point5)
            ret << PeakPt5S;
        break;
    case SlopeW:
    case CornerInnerSE:
    case CornerOuterSW:
        if (mDepth == Three) {
            ret << CapRiseS1 << CapRiseS2 << CapRiseS3;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapRiseS1 << CapRiseS2;
            b.adjust(0,0,-1,0);
        } else if (mDepth == Two)
            ret << CapRiseS1 << CapRiseS2;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapRiseS1;
            b.adjust(0,0,-1,0);
        } else if (mDepth == One)
            ret << CapRiseS1;
        else if (mDepth == Point5) {}
        break;
    case SlopeE:
    case CornerInnerSW:
    case CornerOuterSE:
        if (mDepth == Three) {
            ret << CapFallS3 << CapFallS2 << CapFallS1;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapFallS2 << CapFallS1;
            b.adjust(1,0,0,0);
        } else if (mDepth == Two)
            ret << CapFallS2 << CapFallS1;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapFallS1;
            b.adjust(1,0,0,0);
        } else if (mDepth == One)
            ret << CapFallS1;
        else if (mDepth == Point5) {}
        break;
    case ShallowSlopeW:
        ret += CapShallowRiseS1;
        if (mWidth > 1) ret += CapShallowRiseS2;
        break;
    case ShallowSlopeE:
        if (mWidth > 1) ret += CapShallowFallS2;
        ret += CapShallowFallS1;
        break;
    case ShallowPeakNS:
        ret += CapShallowRiseS1;
        if (mWidth > 2) ret << CapShallowRiseS2 << CapShallowFallS2;
        ret += CapShallowFallS1;
        break;
    case Slope30W:
    case CornerSlope30InnerSE:
    case CornerSlope30OuterSW:
        ret += CapSlope30RiseS1;
        if (mWidth > 1) ret += CapSlope30RiseS2;
        if (mWidth > 2) ret += CapSlope30RiseS3;
        if (mWidth > 3) ret += CapSlope30RiseS4;
        if (mWidth > 4) ret += CapSlope30RiseS5;
        if (mWidth > 5) ret += CapSlope30RiseS6;
        break;
    case Slope30N:
        break;
    case Slope30E:
    case CornerSlope30InnerSW:
    case CornerSlope30OuterSE:
        if (mWidth > 5) ret += CapSlope30FallS6;
        if (mWidth > 4) ret += CapSlope30FallS5;
        if (mWidth > 3) ret += CapSlope30FallS4;
        if (mWidth > 2) ret += CapSlope30FallS3;
        if (mWidth > 1) ret += CapSlope30FallS2;
        ret += CapSlope30FallS1;
        break;
    case Slope30S:
        break;
    case Peak30NS:
        ret.resize(mWidth);
        ret[mWidth / 2] = CapPeak30S1;
        if (mWidth >= 3) {
            ret[0] = CapSlope30RiseS1;
            ret[mWidth - 1] = CapSlope30FallS1;
            ret[mWidth / 2] = CapPeak30S2;
        }
        if (mWidth >= 5) {
            ret[1] = CapSlope30RiseS2;
            ret[mWidth - 2] = CapSlope30FallS2;
            ret[mWidth / 2] = CapPeak30S3;
        }
        if (mWidth >= 7) {
            ret[2] = CapSlope30RiseS3;
            ret[mWidth - 3] = CapSlope30FallS3;
            ret[mWidth / 2] = CapPeak30S4;
        }
        if (mWidth >= 9) {
            ret[3] = CapSlope30RiseS4;
            ret[mWidth - 4] = CapSlope30FallS4;
            ret[mWidth / 2] = CapPeak30S5;
        }
        if (mWidth >= 11) {
            ret[4] = CapSlope30RiseS5;
            ret[mWidth - 5] = CapSlope30FallS5;
            ret[mWidth / 2] = CapPeak30S6;
        }
        break;
    case Dormer30N: {
        ret.resize(mWidth);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, 1);
        tiler.rowIncr(0, 0, std::min(mWidth / 2, 6), CapSlope30RiseS1);
        tiler.rowDecr(mWidth - 1, 0, std::min(mWidth / 2, 6), CapSlope30FallS1);
        RoofTile peakTile = TileCount;
        if (mWidth == 1) {
            peakTile = CapPeak30S1;
        } else if (mWidth == 3) {
            peakTile = CapPeak30S2;
        } else if (mWidth == 5) {
            peakTile = CapPeak30S3;
        } else if (mWidth == 7) {
            peakTile = CapPeak30S4;
        } else if (mWidth == 9) {
            peakTile = CapPeak30S5;
        } else if (mWidth == 11) {
            peakTile = CapPeak30S6;
        }
        if (mWidth > 12) {
            tiler.row(6, 0, mWidth - 12, CapGapS3);
        } else if (mWidth < 12) {
            ret[mWidth / 2] = peakTile;
        }
        break;
    }
    case FlatTop:
    case CornerInnerNW:
    case CornerInnerNE: {
        RoofTile tile = CapGapS3;
        if (mDepth == Three) tile = CapGapS3;
        else if (mDepth == Two) tile = CapGapS2;
        else if (mDepth == One) tile = CapGapS1;
        else break;
        for (int x = 0; x < mWidth; x++) ret << tile;
        break;
    }
    default:
        break;
    }
    return ret;
}

QVector<RoofObject::RoofTile> RoofObject::southCapTiles(QRect &b)
{
    QRect r = bounds();
    b = QRect(r.x(), r.bottom() + 1, r.width(), 1);
    QVector<RoofTile> ret;
    if (!isCappedS()) return ret;

    switch (mType) {
    case PeakNS:
    case DormerS:
        if (mDepth == Three) {
            ret << CapRiseS1 << CapRiseS2 << CapRiseS3;
            for (int x = 0; x < mWidth - 6; x++) ret << CapGapS3;
            ret << CapFallS3 << CapFallS2 << CapFallS1;
        } else if (mDepth == TwoPoint5)
            ret << CapRiseS1 << CapRiseS2 << PeakTwoPt5S << CapFallS2 << CapFallS1;
        else if (mDepth == Two)
            ret << CapRiseS1 << CapRiseS2 << CapFallS2 << CapFallS1;
        else if (mDepth == OnePoint5)
            ret << CapRiseS1 << PeakOnePt5S << CapFallS1;
        else if (mDepth == One)
            ret << CapRiseS1 << CapFallS1;
        else if (mDepth == Point5)
            ret << PeakPt5S;
        break;
    case SlopeW:
    case CornerInnerNE:
    case CornerOuterNW:
        if (mDepth == Three) {
            ret << CapRiseS1 << CapRiseS2 << CapRiseS3;
        } else if (mDepth == TwoPoint5) {
            ret << CapRiseS1 << CapRiseS2;
            b.adjust(0,0,-1,0);
        } else if (mDepth == Two)
            ret << CapRiseS1 << CapRiseS2;
        else if (mDepth == OnePoint5) {
            ret << CapRiseS1;
            b.adjust(0,0,-1,0);
        } else if (mDepth == One)
            ret << CapRiseS1;
        else if (mDepth == Point5) {}
        break;
    case SlopeE:
    case CornerInnerNW:
    case CornerOuterNE:
        if (mDepth == Three) {
            ret << CapFallS3 << CapFallS2 << CapFallS1;
        } else if (mDepth == TwoPoint5) {
            ret << /* ??? << */ CapFallS2 << CapFallS1;
            b.adjust(1,0,0,0);
        } else if (mDepth == Two)
            ret << CapFallS2 << CapFallS1;
        else if (mDepth == OnePoint5) {
            ret << /* ??? << */ CapFallS1;
            b.adjust(1,0,0,0);
        } else if (mDepth == One)
            ret << CapFallS1;
        else if (mDepth == Point5) {}
        break;
    case ShallowSlopeW:
        ret += CapShallowRiseS1;
        if (mWidth > 1) ret += CapShallowRiseS2;
        break;
    case ShallowSlopeE:
        if (mWidth > 1) ret += CapShallowFallS2;
        ret += CapShallowFallS1;
        break;
    case ShallowPeakNS:
        ret += CapShallowRiseS1;
        if (mWidth > 2) ret << CapShallowRiseS2 << CapShallowFallS2;
        ret += CapShallowFallS1;
        break;
    case Slope30W:
    case CornerSlope30InnerNE:
    case CornerSlope30OuterNW:
        ret += CapSlope30RiseS1;
        if (mWidth > 1) ret += CapSlope30RiseS2;
        if (mWidth > 2) ret += CapSlope30RiseS3;
        if (mWidth > 3) ret += CapSlope30RiseS4;
        if (mWidth > 4) ret += CapSlope30RiseS5;
        if (mWidth > 5) ret += CapSlope30RiseS6;
        break;
    case Slope30N:
        break;
    case Slope30E:
    case CornerSlope30InnerNW:
    case CornerSlope30OuterNE:
        if (mWidth > 5) ret += CapSlope30FallS6;
        if (mWidth > 4) ret += CapSlope30FallS5;
        if (mWidth > 3) ret += CapSlope30FallS4;
        if (mWidth > 2) ret += CapSlope30FallS3;
        if (mWidth > 1) ret += CapSlope30FallS2;
        ret += CapSlope30FallS1;
        break;
    case Slope30S:
        break;
    case Peak30NS:
        ret.resize(mWidth);
        ret[mWidth / 2] = CapPeak30S1;
        if (mWidth >= 3) {
            ret[0] = CapSlope30RiseS1;
            ret[mWidth - 1] = CapSlope30FallS1;
            ret[mWidth / 2] = CapPeak30S2;
        }
        if (mWidth >= 5) {
            ret[1] = CapSlope30RiseS2;
            ret[mWidth - 2] = CapSlope30FallS2;
            ret[mWidth / 2] = CapPeak30S3;
        }
        if (mWidth >= 7) {
            ret[2] = CapSlope30RiseS3;
            ret[mWidth - 3] = CapSlope30FallS3;
            ret[mWidth / 2] = CapPeak30S4;
        }
        if (mWidth >= 9) {
            ret[3] = CapSlope30RiseS4;
            ret[mWidth - 4] = CapSlope30FallS4;
            ret[mWidth / 2] = CapPeak30S5;
        }
        if (mWidth >= 11) {
            ret[4] = CapSlope30RiseS5;
            ret[mWidth - 5] = CapSlope30FallS5;
            ret[mWidth / 2] = CapPeak30S6;
        }
        break;
    case Dormer30S: {
        ret.resize(mWidth);
        ret.fill(RoofTile::TileCount);
        RoofTiler tiler(ret, mWidth, 1);
        tiler.rowIncr(0, 0, std::min(mWidth / 2, 6), CapSlope30RiseS1);
        tiler.rowDecr(mWidth - 1, 0, std::min(mWidth / 2, 6), CapSlope30FallS1);
        RoofTile peakTile = TileCount;
        if (mWidth == 1) {
            peakTile = CapPeak30S1;
        } else if (mWidth == 3) {
            peakTile = CapPeak30S2;
        } else if (mWidth == 5) {
            peakTile = CapPeak30S3;
        } else if (mWidth == 7) {
            peakTile = CapPeak30S4;
        } else if (mWidth == 9) {
            peakTile = CapPeak30S5;
        } else if (mWidth == 11) {
            peakTile = CapPeak30S6;
        }
        if (mWidth > 12) {
            tiler.row(6, 0, mWidth - 12, CapGapS3);
        } else if (mWidth < 12) {
            ret[mWidth / 2] = peakTile;
        }
        break;
    }
    case FlatTop:
    case CornerInnerSE:
    case CornerInnerSW: {
        RoofTile tile = CapGapS3;
        if (mDepth == Three) tile = CapGapS3;
        else if (mDepth == Two) tile = CapGapS2;
        else if (mDepth == One) tile = CapGapS1;
        else break;
        for (int x = 0; x < mWidth; x++) ret << tile;
        break;
    }
    default:
        break;
    }
    return ret;
}

static void setCornerTile(QVector<RoofObject::RoofTile> &tiles, int w, int x, int y, RoofObject::RoofTile tile)
{
    tiles[x + y * w] = tile;
}

static void setCornerTileColumn(QVector<RoofObject::RoofTile> &tiles, int w, int x, int y, const QVector<RoofObject::RoofTile> &row)
{
    for (int dy = 0; dy < row.size(); dy++) {
        tiles[x + (y + dy) * w] = row.at(dy);
    }
}

static void setCornerTileRow(QVector<RoofObject::RoofTile> &tiles, int w, int x, int y, const QVector<RoofObject::RoofTile> &col)
{
    for (int dx = 0; dx < col.size(); dx++) {
        tiles[(x + dx) + y * w] = col.at(dx);
    }
}

QVector<RoofObject::RoofTile> RoofObject::cornerTiles(QRect &b)
{
    QRect r = bounds();
    b = r;
    QVector<RoofTile> ret;

    switch (mType) {
    case DormerE:
        b = QRect(r.left(), r.bottom() + 1 - slopeThickness(),
                  slopeThickness(), slopeThickness());
        // Same as CornerInnerNW but with no east slope tiles
        if (mDepth == Three) {
            ret << Inner3 << SlopeS3 << SlopeS3
                << TileCount << Inner2 << SlopeS2
                << TileCount << TileCount << Inner1;
        } else if (mDepth == TwoPoint5) {
            ret << InnerTwoPt5 << SlopeTwoPt5S << SlopeTwoPt5S
                << TileCount << Inner2 << SlopeS2
                << TileCount << TileCount << Inner1;
        } else if (mDepth == Two) {
            ret << Inner2 << SlopeS2
                << TileCount << Inner1;
        } else if (mDepth == OnePoint5) {
            ret << InnerOnePt5 << SlopeOnePt5S
                << TileCount << Inner1;
        } else if (mDepth == One) {
            ret << Inner1;
        } else if (mDepth == Point5) {
            ret << InnerPt5;
        }
        break;
    case DormerS:
        b = QRect(r.right() - slopeThickness() + 1, r.top(),
                  slopeThickness(), slopeThickness());
        // Same as CornerInnerNW but with no south slope tiles
        if (mDepth == Three) {
            ret << Inner3 << TileCount << TileCount
                << SlopeE3 << Inner2 << TileCount
                << SlopeE3 << SlopeE2 << Inner1;
        } else if (mDepth == TwoPoint5) {
            ret << InnerTwoPt5 << TileCount << TileCount
                << SlopeTwoPt5E << Inner2 << TileCount
                << SlopeTwoPt5E << SlopeE2 << Inner1;
        } else if (mDepth == Two) {
            ret << Inner2 << TileCount
                << SlopeE2 << Inner1;
        } else if (mDepth == OnePoint5) {
            ret << InnerOnePt5 << TileCount
                << SlopeOnePt5E << Inner1;
        } else if (mDepth == One) {
            ret << Inner1;
        } else if (mDepth == Point5) {
            ret << InnerPt5;
        }
        break;

    case CornerInnerSW:
        break;
    case CornerInnerNW:
        if (mDepth == Three) {
            ret << Inner3 << SlopeS3 << SlopeS3
                << SlopeE3 << Inner2 << SlopeS2
                << SlopeE3 << SlopeE2 << Inner1;
        } else if (mDepth == TwoPoint5) {
            ret << InnerTwoPt5 << SlopeTwoPt5S << SlopeTwoPt5S
                << SlopeTwoPt5E << Inner2 << SlopeS2
                << SlopeTwoPt5E << SlopeE2 << Inner1;
        } else if (mDepth == Two) {
            ret << Inner2 << SlopeS2
                << SlopeE2 << Inner1;
        } else if (mDepth == OnePoint5) {
            ret << InnerOnePt5 << SlopeOnePt5S
                << SlopeOnePt5E << Inner1;
        } else if (mDepth == One) {
            ret << Inner1;
        } else if (mDepth == Point5) {
            ret << InnerPt5;
        }
        break;
    case CornerInnerNE:
        break;
    case CornerInnerSE:
        break;

    case CornerOuterSW:
        if (mDepth == Three) {
            ret << TileCount << TileCount << CornerSW3
                << TileCount << CornerSW2 << SlopeS2
                << CornerSW1 << SlopeS1 << SlopeS1;
        } else if (mDepth == Two) {
            ret << TileCount << CornerSW2
                << CornerSW1 << SlopeS1;
        } else if (mDepth == One) {
            ret << CornerSW1;
        }
        break;
    case CornerOuterNW:
        break;
    case CornerOuterNE:
        if (mDepth == Three) {
            ret << TileCount << TileCount << CornerNE1
                << TileCount << CornerNE2 << SlopeE1
                << CornerNE3 << SlopeE2 << SlopeE1;
        } else if (mDepth == Two) {
            ret << TileCount << CornerNE1
                << CornerNE2 << SlopeE1;
        } else if (mDepth == One) {
            ret << CornerNE1;
        }
        break;
    case CornerOuterSE:
        if (mDepth == Three) {
            ret << Outer3 << SlopeE2 << SlopeE1
                << SlopeS2 << Outer2 << SlopeE1
                << SlopeS1 << SlopeS1 << Outer1;
        } else if (mDepth == TwoPoint5) {
            ret << OuterTwoPt5 << SlopeE2 << SlopeE1
                << SlopeS2 << Outer2 << SlopeE1
                << SlopeS1 << SlopeS1 << Outer1;
        } else if (mDepth == Two) {
            ret << Outer2 << SlopeE1
                << SlopeS1 << Outer1;
        } else if (mDepth == OnePoint5) {
            ret << OuterOnePt5 << SlopeE1
                << SlopeS1 << Outer1;
        } else if (mDepth == One) {
            ret << Outer1;
        } else if (mDepth == Point5) {
            ret << OuterPt5;
        }
        break;

    case CornerSlope30InnerSW:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 5, InnerSlope30SW6);
            setCornerTile(ret, b.width(), 1, 4, InnerSlope30SW5);
            setCornerTile(ret, b.width(), 2, 3, InnerSlope30SW4);
            setCornerTile(ret, b.width(), 3, 2, InnerSlope30SW3);
            setCornerTile(ret, b.width(), 4, 1, InnerSlope30SW2);
            setCornerTile(ret, b.width(), 5, 0, InnerSlope30SW1);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30E6, Slope30E6, Slope30E6, Slope30E6, Slope30E6 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E5, Slope30E5, Slope30E5, Slope30E5 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E4, Slope30E4, Slope30E4 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 1, 5, { Slope30N6, Slope30N6, Slope30N6, Slope30N6, Slope30N6 });
            setCornerTileRow(ret, b.width(), 2, 4, { Slope30N5, Slope30N5, Slope30N5, Slope30N5 });
            setCornerTileRow(ret, b.width(), 3, 3, { Slope30N4, Slope30N4, Slope30N4 });
            setCornerTileRow(ret, b.width(), 4, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 5, 1, { Slope30N2 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 4, InnerSlope30SW5);
            setCornerTile(ret, b.width(), 1, 3, InnerSlope30SW4);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30SW3);
            setCornerTile(ret, b.width(), 3, 1, InnerSlope30SW2);
            setCornerTile(ret, b.width(), 4, 0, InnerSlope30SW1);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30E5, Slope30E5, Slope30E5, Slope30E5 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E4, Slope30E4, Slope30E4 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 1, 4, { Slope30N5, Slope30N5, Slope30N5, Slope30N5 });
            setCornerTileRow(ret, b.width(), 2, 3, { Slope30N4, Slope30N4, Slope30N4 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 4, 1, { Slope30N2 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 3, InnerSlope30SW4);
            setCornerTile(ret, b.width(), 1, 2, InnerSlope30SW3);
            setCornerTile(ret, b.width(), 2, 1, InnerSlope30SW2);
            setCornerTile(ret, b.width(), 3, 0, InnerSlope30SW1);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30E4, Slope30E4, Slope30E4 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 1, 3, { Slope30N4, Slope30N4, Slope30N4 });
            setCornerTileRow(ret, b.width(), 2, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 3, 1, { Slope30N2 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 2, InnerSlope30SW3);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SW2);
            setCornerTile(ret, b.width(), 2, 0, InnerSlope30SW1);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 1, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30N2 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 1, InnerSlope30SW2);
            setCornerTile(ret, b.width(), 1, 0, InnerSlope30SW1);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 1, 1, { Slope30N2 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SW1);
        }
        break;
    case CornerSlope30InnerNW:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW6);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NW5);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30NW4);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30NW3);
            setCornerTile(ret, b.width(), 4, 4, InnerSlope30NW2);
            setCornerTile(ret, b.width(), 5, 5, InnerSlope30NW1);
            setCornerTileColumn(ret, b.width(), 5, 0, { Slope30S6, Slope30S5, Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30S6, Slope30S5, Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30S6, Slope30S5, Slope30S4 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S6, Slope30S5 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S6 });
            setCornerTileRow(ret, b.width(), 0, 5, { Slope30E6, Slope30E5, Slope30E4, Slope30E3, Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 4, { Slope30E6, Slope30E5, Slope30E4, Slope30E3 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30E6, Slope30E5, Slope30E4 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30E6, Slope30E5 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30E6 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW5);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NW4);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30NW3);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30NW2);
            setCornerTile(ret, b.width(), 4, 4, InnerSlope30NW1);
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30S5, Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30S5, Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S5, Slope30S4 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S5 });
            setCornerTileRow(ret, b.width(), 0, 4, { Slope30E5, Slope30E4, Slope30E3, Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30E5, Slope30E4, Slope30E3 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30E5, Slope30E4 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30E5 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW4);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NW3);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30NW2);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30NW1);
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S4 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30E4, Slope30E3, Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30E4, Slope30E3 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30E4 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW3);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NW2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30NW1);
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S3 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30E3, Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30E3 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW2);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NW1);
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S2 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30E2 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NW1);
        }
        break;
    case CornerSlope30InnerNE:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 5, InnerSlope30NE1);
            setCornerTile(ret, b.width(), 1, 4, InnerSlope30NE2);
            setCornerTile(ret, b.width(), 2, 3, InnerSlope30NE3);
            setCornerTile(ret, b.width(), 3, 2, InnerSlope30NE4);
            setCornerTile(ret, b.width(), 4, 1, InnerSlope30NE5);
            setCornerTile(ret, b.width(), 5, 0, InnerSlope30NE6);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30S6, Slope30S5, Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S6, Slope30S5, Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S6, Slope30S5, Slope30S4 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30S6, Slope30S5 });
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30S6 });
            setCornerTileRow(ret, b.width(), 1, 5, { Slope30W2, Slope30W3, Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 2, 4, { Slope30W3, Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 3, 3, { Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 4, 2, { Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 5, 1, { Slope30W6 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 4, InnerSlope30NE1);
            setCornerTile(ret, b.width(), 1, 3, InnerSlope30NE2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30NE3);
            setCornerTile(ret, b.width(), 3, 1, InnerSlope30NE4);
            setCornerTile(ret, b.width(), 4, 0, InnerSlope30NE5);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30S5, Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S5, Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S5, Slope30S4 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30S5 });
            setCornerTileRow(ret, b.width(), 1, 4, { Slope30W2, Slope30W3, Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 2, 3, { Slope30W3, Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 4, 1, { Slope30W5 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 3, InnerSlope30NE1);
            setCornerTile(ret, b.width(), 1, 2, InnerSlope30NE2);
            setCornerTile(ret, b.width(), 2, 1, InnerSlope30NE3);
            setCornerTile(ret, b.width(), 3, 0, InnerSlope30NE4);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30S4, Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S4, Slope30S3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30S4 });
            setCornerTileRow(ret, b.width(), 1, 3, { Slope30W2, Slope30W3, Slope30W4 });
            setCornerTileRow(ret, b.width(), 2, 2, { Slope30W3, Slope30W4 });
            setCornerTileRow(ret, b.width(), 3, 1, { Slope30W4 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 2, InnerSlope30NE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30NE2);
            setCornerTile(ret, b.width(), 2, 0, InnerSlope30NE3);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30S3, Slope30S2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30S3 });
            setCornerTileRow(ret, b.width(), 1, 2, { Slope30W2, Slope30W3 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30W3 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 1, InnerSlope30NE1);
            setCornerTile(ret, b.width(), 1, 0, InnerSlope30NE2);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30S2 });
            setCornerTileRow(ret, b.width(), 1, 1, { Slope30W2 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30NE1);
        }
        break;
    case CornerSlope30InnerSE:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SE2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30SE3);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30SE4);
            setCornerTile(ret, b.width(), 4, 4, InnerSlope30SE5);
            setCornerTile(ret, b.width(), 5, 5, InnerSlope30SE6);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30N2, Slope30N3, Slope30N4, Slope30N5, Slope30N6 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30N3, Slope30N4, Slope30N5, Slope30N6 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30N4, Slope30N5, Slope30N6 });
            setCornerTileColumn(ret, b.width(), 3, 4, { Slope30N5, Slope30N6 });
            setCornerTileColumn(ret, b.width(), 4, 5, { Slope30N6 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30W2, Slope30W3, Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30W3, Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30W4, Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 4, 3, { Slope30W5, Slope30W6 });
            setCornerTileRow(ret, b.width(), 5, 4, { Slope30W6 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SE2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30SE3);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30SE4);
            setCornerTile(ret, b.width(), 4, 4, InnerSlope30SE5);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30N2, Slope30N3, Slope30N4, Slope30N5 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30N3, Slope30N4, Slope30N5 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30N4, Slope30N5 });
            setCornerTileColumn(ret, b.width(), 3, 4, { Slope30N5 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30W2, Slope30W3, Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30W3, Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30W4, Slope30W5 });
            setCornerTileRow(ret, b.width(), 4, 3, { Slope30W5 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SE2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30SE3);
            setCornerTile(ret, b.width(), 3, 3, InnerSlope30SE4);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30N2, Slope30N3, Slope30N4 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30N3, Slope30N4 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30N4 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30W2, Slope30W3, Slope30W4 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30W3, Slope30W4 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30W4 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SE2);
            setCornerTile(ret, b.width(), 2, 2, InnerSlope30SE3);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30N2, Slope30N3 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30N3 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30W2, Slope30W3 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30W3 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, InnerSlope30SE2);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30N2 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30W2 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, InnerSlope30SE1);
        }
        break;

    case CornerSlope30OuterSW:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 5, OuterSlope30SW1);
            setCornerTile(ret, b.width(), 1, 4, OuterSlope30SW2);
            setCornerTile(ret, b.width(), 2, 3, OuterSlope30SW3);
            setCornerTile(ret, b.width(), 3, 2, OuterSlope30SW4);
            setCornerTile(ret, b.width(), 4, 1, OuterSlope30SW5);
            setCornerTile(ret, b.width(), 5, 0, OuterSlope30SW6);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30W1, Slope30W1, Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30W2, Slope30W2, Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30W3, Slope30W3, Slope30W3 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30W4, Slope30W4 });
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30W5 });
            setCornerTileRow(ret, b.width(), 1, 5, { Slope30S1, Slope30S1, Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 2, 4, { Slope30S2, Slope30S2, Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 3, 3, { Slope30S3, Slope30S3, Slope30S3 });
            setCornerTileRow(ret, b.width(), 4, 2, { Slope30S4, Slope30S4 });
            setCornerTileRow(ret, b.width(), 5, 1, { Slope30S5 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 4, OuterSlope30SW1);
            setCornerTile(ret, b.width(), 1, 3, OuterSlope30SW2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30SW3);
            setCornerTile(ret, b.width(), 3, 1, OuterSlope30SW4);
            setCornerTile(ret, b.width(), 4, 0, OuterSlope30SW5);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30W1, Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30W2, Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30W3, Slope30W3 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30W4  });
            setCornerTileRow(ret, b.width(), 1, 4, { Slope30S1, Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 2, 3, { Slope30S2, Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30S3, Slope30S3 });
            setCornerTileRow(ret, b.width(), 4, 1, { Slope30S4 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 3, OuterSlope30SW1);
            setCornerTile(ret, b.width(), 1, 2, OuterSlope30SW2);
            setCornerTile(ret, b.width(), 2, 1, OuterSlope30SW3);
            setCornerTile(ret, b.width(), 3, 0, OuterSlope30SW4);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30W3 });
            setCornerTileRow(ret, b.width(), 1, 3, { Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 2, 2, { Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 3, 1, { Slope30S3 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 2, OuterSlope30SW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SW2);
            setCornerTile(ret, b.width(), 2, 0, OuterSlope30SW3);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30W2 });
            setCornerTileRow(ret, b.width(), 1, 2, { Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30S2 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 1, OuterSlope30SW1);
            setCornerTile(ret, b.width(), 1, 0, OuterSlope30SW2);
            setCornerTileColumn(ret, b.width(), 0, 0, { Slope30W1 });
            setCornerTileRow(ret, b.width(), 1, 1, { Slope30S1 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SW1);
        }
        break;
    case CornerSlope30OuterNW:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NW2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30NW3);
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30NW4);
            setCornerTile(ret, b.width(), 4, 4, OuterSlope30NW5);
            setCornerTile(ret, b.width(), 5, 5, OuterSlope30NW6);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30W1, Slope30W1, Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30W2, Slope30W2, Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30W3, Slope30W3, Slope30W3 });
            setCornerTileColumn(ret, b.width(), 3, 4, { Slope30W4, Slope30W4 });
            setCornerTileColumn(ret, b.width(), 4, 5, { Slope30W5 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30N1, Slope30N1, Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30N2, Slope30N2, Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30N3, Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 4, 3, { Slope30N4, Slope30N4 });
            setCornerTileRow(ret, b.width(), 5, 4, { Slope30N5 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NW2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30NW3);
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30NW4);
            setCornerTile(ret, b.width(), 4, 4, OuterSlope30NW5);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30W1, Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30W2, Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30W3, Slope30W3 });
            setCornerTileColumn(ret, b.width(), 3, 4, { Slope30W4 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30N1, Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30N2, Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 4, 3, { Slope30N4 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NW2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30NW3);
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30NW4);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30W1, Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30W2, Slope30W2 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30W3 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 3, 2, { Slope30N3 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NW2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30NW3);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30W1, Slope30W1 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30W2 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 2, 1, { Slope30N2 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NW2);
            setCornerTileColumn(ret, b.width(), 0, 1, { Slope30W1 });
            setCornerTileRow(ret, b.width(), 1, 0, { Slope30N1 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NW1);
        }
        break;
    case CornerSlope30OuterNE:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 5, 0, OuterSlope30NE1);
            setCornerTile(ret, b.width(), 4, 1, OuterSlope30NE2);
            setCornerTile(ret, b.width(), 3, 2, OuterSlope30NE3);
            setCornerTile(ret, b.width(), 2, 3, OuterSlope30NE4);
            setCornerTile(ret, b.width(), 1, 4, OuterSlope30NE5);
            setCornerTile(ret, b.width(), 0, 5, OuterSlope30NE6);
            setCornerTileColumn(ret, b.width(), 5, 1, { Slope30E1, Slope30E1, Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 4, 2, { Slope30E2, Slope30E2, Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 3, 3, { Slope30E3, Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 2, 4, { Slope30E4, Slope30E4 });
            setCornerTileColumn(ret, b.width(), 1, 5, { Slope30E5 });
            setCornerTileRow(ret, b.width(), 0, 0, { Slope30N1, Slope30N1, Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30N2, Slope30N2, Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30N3, Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30N4, Slope30N4 });
            setCornerTileRow(ret, b.width(), 0, 4, { Slope30N5 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 4, 0, OuterSlope30NE1);
            setCornerTile(ret, b.width(), 3, 1, OuterSlope30NE2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30NE3);
            setCornerTile(ret, b.width(), 1, 3, OuterSlope30NE4);
            setCornerTile(ret, b.width(), 0, 4, OuterSlope30NE5);
            setCornerTileColumn(ret, b.width(), 4, 1, { Slope30E1, Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 3, 2, { Slope30E2, Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 2, 3, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 1, 4, { Slope30E4 });
            setCornerTileRow(ret, b.width(), 0, 0, { Slope30N1, Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30N2, Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30N3, Slope30N3 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30N4 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 3, 0, OuterSlope30NE1);
            setCornerTile(ret, b.width(), 2, 1, OuterSlope30NE2);
            setCornerTile(ret, b.width(), 1, 2, OuterSlope30NE3);
            setCornerTile(ret, b.width(), 0, 3, OuterSlope30NE4);
            setCornerTileColumn(ret, b.width(), 3, 1, { Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 2, 2, { Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 1, 3, { Slope30E3 });
            setCornerTileRow(ret, b.width(), 0, 0, { Slope30N1, Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30N2, Slope30N2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30N3 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 2, 0, OuterSlope30NE1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30NE2);
            setCornerTile(ret, b.width(), 0, 2, OuterSlope30NE3);
            setCornerTileColumn(ret, b.width(), 2, 1, { Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 1, 2, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 0, { Slope30N1, Slope30N1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30N2 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 1, 0, OuterSlope30NE1);
            setCornerTile(ret, b.width(), 0, 1, OuterSlope30NE2);
            setCornerTileColumn(ret, b.width(), 1, 1, { Slope30E1 });
            setCornerTileRow(ret, b.width(), 0, 0, { Slope30N1 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30NE1);
        }
        break;
    case CornerSlope30OuterSE:
        ret.resize(b.width() * b.height());
        ret.fill(RoofTile::TileCount);
        if (mDepth == Three) { // 6x6 corners 1-6
            setCornerTile(ret, b.width(), 5, 5, OuterSlope30SE1);
            setCornerTile(ret, b.width(), 4, 4, OuterSlope30SE2);
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30SE3);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30SE4);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SE5);
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE6);
            setCornerTileColumn(ret, b.width(), 5, 0, { Slope30E1, Slope30E1, Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30E2, Slope30E2, Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30E3, Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E4, Slope30E4 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E5 });
            setCornerTileRow(ret, b.width(), 0, 5, { Slope30S1, Slope30S1, Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 0, 4, { Slope30S2, Slope30S2, Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30S3, Slope30S3, Slope30S3 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30S4, Slope30S4 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30S5 });
        } else if (mDepth == TwoPoint5) { // 5x5 corners 1-5
            setCornerTile(ret, b.width(), 4, 4, OuterSlope30SE1);
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30SE2);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30SE3);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SE4);
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE5);
            setCornerTileColumn(ret, b.width(), 4, 0, { Slope30E1, Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30E2, Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E3, Slope30E3 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E4 });
            setCornerTileRow(ret, b.width(), 0, 4, { Slope30S1, Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30S2, Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30S3, Slope30S3 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30S4 });
        } else if (mDepth == Two) { // 4x4 corners 1-4
            setCornerTile(ret, b.width(), 3, 3, OuterSlope30SE1);
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30SE2);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SE3);
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE4);
            setCornerTileColumn(ret, b.width(), 3, 0, { Slope30E1, Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E2, Slope30E2 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E3 });
            setCornerTileRow(ret, b.width(), 0, 3, { Slope30S1, Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30S2, Slope30S2 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30S3 });
        } else if (mDepth == OnePoint5) { // 3x3 corners 1-3
            setCornerTile(ret, b.width(), 2, 2, OuterSlope30SE1);
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SE2);
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE3);
            setCornerTileColumn(ret, b.width(), 2, 0, { Slope30E1, Slope30E1 });
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E2 });
            setCornerTileRow(ret, b.width(), 0, 2, { Slope30S1, Slope30S1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30S2 });
        } else if (mDepth == One) { // 2x2 corners 1-2
            setCornerTile(ret, b.width(), 1, 1, OuterSlope30SE1);
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE2);
            setCornerTileColumn(ret, b.width(), 1, 0, { Slope30E1 });
            setCornerTileRow(ret, b.width(), 0, 1, { Slope30S1 });
        } else if (mDepth == Point5) { // 1x1 corner 1
            setCornerTile(ret, b.width(), 0, 0, OuterSlope30SE1);
        }
        break;

    default:
        break;
    }
    return ret;
}
#if 0
QRect RoofObject::cornerInner(bool &slopeE, bool &slopeS)
{
    QRect r = bounds();
    switch (mType) {
    case DormerE:
        slopeE = false, slopeS = true;
        return QRect(r.left(), r.bottom() - slopeThickness() + 1,
                     slopeThickness(), slopeThickness());
    case DormerS:
        slopeE = true, slopeS = false;
        return QRect(r.right() - slopeThickness() + 1, r.top(),
                     slopeThickness(), slopeThickness());
    case CornerInnerNW:
        slopeE = slopeS = true;
        return r;
    default:
        break;
    }
    return QRect();
}

QRect RoofObject::cornerOuter()
{
    QRect r = bounds();
    switch (mType) {
    case CornerOuterSE:
        return r;
    default:
        break;
    }
    return QRect();
}
#endif

QString RoofObject::typeToString(RoofObject::RoofType type)
{
    switch (type) {
    case SlopeW: return QStringLiteral("SlopeW");
    case SlopeN: return QStringLiteral("SlopeN");
    case SlopeE: return QStringLiteral("SlopeE");
    case SlopeS: return QStringLiteral("SlopeS");

    case PeakWE: return QStringLiteral("PeakWE");
    case PeakNS: return QStringLiteral("PeakNS");

    case DormerW: return QStringLiteral("DormerW");
    case DormerN: return QStringLiteral("DormerN");
    case DormerE: return QStringLiteral("DormerE");
    case DormerS: return QStringLiteral("DormerS");

    case FlatTop: return QStringLiteral("FlatTop");

    case ShallowSlopeW: return QStringLiteral("ShallowSlopeW");
    case ShallowSlopeE: return QStringLiteral("ShallowSlopeE");
    case ShallowSlopeN: return QStringLiteral("ShallowSlopeN");
    case ShallowSlopeS: return QStringLiteral("ShallowSlopeS");
    case ShallowPeakWE: return QStringLiteral("ShallowPeakWE");
    case ShallowPeakNS: return QStringLiteral("ShallowPeakNS");

    case Slope30W: return QStringLiteral("Slope30W");
    case Slope30N: return QStringLiteral("Slope30N");
    case Slope30E: return QStringLiteral("Slope30E");
    case Slope30S: return QStringLiteral("Slope30S");
    case Peak30WE: return QStringLiteral("Peak30WE");
    case Peak30NS: return QStringLiteral("Peak30NS");
    case Peak30Quad: return QStringLiteral("Peak30Quad");

    case Dormer30W: return QStringLiteral("Dormer30W");
    case Dormer30N: return QStringLiteral("Dormer30N");
    case Dormer30E: return QStringLiteral("Dormer30E");
    case Dormer30S: return QStringLiteral("Dormer30S");

    case CornerInnerSW: return QStringLiteral("CornerInnerSW");
    case CornerInnerNW: return QStringLiteral("CornerInnerNW");
    case CornerInnerNE: return QStringLiteral("CornerInnerNE");
    case CornerInnerSE: return QStringLiteral("CornerInnerSE");

    case CornerOuterSW: return QStringLiteral("CornerOuterSW");
    case CornerOuterNW: return QStringLiteral("CornerOuterNW");
    case CornerOuterNE: return QStringLiteral("CornerOuterNE");
    case CornerOuterSE: return QStringLiteral("CornerOuterSE");

    case CornerSlope30InnerSW: return QStringLiteral("CornerSlope30InnerSW");
    case CornerSlope30InnerNW: return QStringLiteral("CornerSlope30InnerNW");
    case CornerSlope30InnerNE: return QStringLiteral("CornerSlope30InnerNE");
    case CornerSlope30InnerSE: return QStringLiteral("CornerSlope30InnerSE");

    case CornerSlope30OuterSW: return QStringLiteral("CornerSlope30OuterSW");
    case CornerSlope30OuterNW: return QStringLiteral("CornerSlope30OuterNW");
    case CornerSlope30OuterNE: return QStringLiteral("CornerSlope30OuterNE");
    case CornerSlope30OuterSE: return QStringLiteral("CornerSlope30OuterSE");

    default:
        break;
    }

    return QStringLiteral("Invalid");
}

RoofObject::RoofType RoofObject::typeFromString(const QString &s)
{
    if (s == QStringLiteral("SlopeW")) return SlopeW;
    if (s == QStringLiteral("SlopeN")) return SlopeN;
    if (s == QStringLiteral("SlopeE")) return SlopeE;
    if (s == QStringLiteral("SlopeS")) return SlopeS;

    if (s == QStringLiteral("PeakWE")) return PeakWE;
    if (s == QStringLiteral("PeakNS")) return PeakNS;

    if (s == QStringLiteral("DormerW")) return DormerW;
    if (s == QStringLiteral("DormerN")) return DormerN;
    if (s == QStringLiteral("DormerE")) return DormerE;
    if (s == QStringLiteral("DormerS")) return DormerS;

    if (s == QStringLiteral("FlatTop")) return FlatTop;

    if (s == QStringLiteral("ShallowSlopeW")) return ShallowSlopeW;
    if (s == QStringLiteral("ShallowSlopeN")) return ShallowSlopeN;
    if (s == QStringLiteral("ShallowSlopeE")) return ShallowSlopeE;
    if (s == QStringLiteral("ShallowSlopeS")) return ShallowSlopeS;

    if (s == QStringLiteral("ShallowPeakWE")) return ShallowPeakWE;
    if (s == QStringLiteral("ShallowPeakNS")) return ShallowPeakNS;

    if (s == QStringLiteral("Slope30W")) return Slope30W;
    if (s == QStringLiteral("Slope30N")) return Slope30N;
    if (s == QStringLiteral("Slope30E")) return Slope30E;
    if (s == QStringLiteral("Slope30S")) return Slope30S;
    if (s == QStringLiteral("Peak30WE")) return Peak30WE;
    if (s == QStringLiteral("Peak30NS")) return Peak30NS;
    if (s == QStringLiteral("Peak30Quad")) return Peak30Quad;

    if (s == QStringLiteral("Dormer30W")) return Dormer30W;
    if (s == QStringLiteral("Dormer30N")) return Dormer30N;
    if (s == QStringLiteral("Dormer30E")) return Dormer30E;
    if (s == QStringLiteral("Dormer30S")) return Dormer30S;

    if (s == QStringLiteral("CornerInnerSW")) return CornerInnerSW;
    if (s == QStringLiteral("CornerInnerNW")) return CornerInnerNW;
    if (s == QStringLiteral("CornerInnerNE")) return CornerInnerNE;
    if (s == QStringLiteral("CornerInnerSE")) return CornerInnerSE;

    if (s == QStringLiteral("CornerOuterSW")) return CornerOuterSW;
    if (s == QStringLiteral("CornerOuterNW")) return CornerOuterNW;
    if (s == QStringLiteral("CornerOuterNE")) return CornerOuterNE;
    if (s == QStringLiteral("CornerOuterSE")) return CornerOuterSE;

    if (s == QStringLiteral("CornerSlope30InnerSW")) return CornerSlope30InnerSW;
    if (s == QStringLiteral("CornerSlope30InnerNW")) return CornerSlope30InnerNW;
    if (s == QStringLiteral("CornerSlope30InnerNE")) return CornerSlope30InnerNE;
    if (s == QStringLiteral("CornerSlope30InnerSE")) return CornerSlope30InnerSE;

    if (s == QStringLiteral("CornerSlope30OuterSW")) return CornerSlope30OuterSW;
    if (s == QStringLiteral("CornerSlope30OuterNW")) return CornerSlope30OuterNW;
    if (s == QStringLiteral("CornerSlope30OuterNE")) return CornerSlope30OuterNE;
    if (s == QStringLiteral("CornerSlope30OuterSE")) return CornerSlope30OuterSE;

    return InvalidType;
}

QString RoofObject::depthToString(RoofObject::RoofDepth depth)
{
    switch (depth) {
    case Zero: return QStringLiteral("Zero");
    case Point5: return QStringLiteral("Point5");
    case One: return QStringLiteral("One");
    case OnePoint5: return QStringLiteral("OnePoint5");
    case Two: return QStringLiteral("Two");
    case TwoPoint5: return QStringLiteral("TwoPoint5");
    case Three: return QStringLiteral("Three");
    default:
        break;
    }

    qFatal("unhandled roof object depth");

    return QStringLiteral("Invalid");
}

RoofObject::RoofDepth RoofObject::depthFromString(const QString &s)
{
    if (s == QStringLiteral("Zero")) return Zero;
    if (s == QStringLiteral("Point5")) return Point5;
    if (s == QStringLiteral("One")) return One;
    if (s == QStringLiteral("OnePoint5")) return OnePoint5;
    if (s == QStringLiteral("Two")) return Two;
    if (s == QStringLiteral("TwoPoint5")) return OnePoint5;
    if (s == QStringLiteral("Three")) return Three;

    return InvalidDepth;
}

/////

WallObject::WallObject(BuildingFloor *floor, int x, int y,
                       Direction dir, int length) :
    BuildingObject(floor, x, y, dir),
    mLength(length),
    mExteriorTrimTile(0),
    mInteriorTile(0),
    mInteriorTrimTile(0)
{
}

QRect WallObject::bounds() const
{
    return QRect(mX, mY, isW() ? mLength : 1, isW() ? 1 : mLength);
}

void WallObject::rotate(bool right)
{
    int oldFloorWidth = mFloor->height();
    int oldFloorHeight = mFloor->width();

    if (right) {
        int x = mX;
        mX = oldFloorHeight - mY - (isN() ? mLength: 0);
        mY = x;
    } else {
        int x = mX;
        mX = mY;
        mY = oldFloorWidth - x - (isW() ? mLength: 0);
    }

    mDir = isN() ? W : N;
}

void WallObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - (isW() ? mLength: 0);
    } else {
        mY = mFloor->height() - mY - (isN() ? mLength: 0);
    }
}

bool WallObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // Horizontal walls can't go past the right edge of the building.
    // Vertical walls can't go past the bottom edge of the building.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    if (isN())
        floorBounds.adjust(0,0,1,0);
    else
        floorBounds.adjust(0,0,0,1);
    return (floorBounds & objectBounds) == objectBounds;
}

BuildingObject *WallObject::clone() const
{
    WallObject *clone = new WallObject(mFloor, mX, mY, mDir, mLength);
    for (int i = 0; i < TileCount; i++)
        clone->setTile(tile(i), i);
    return clone;
}

bool WallObject::sameAs(BuildingObject *other)
{
    if (WallObject *o = other->asWall()) {
        return mDir == o->mDir && pos() == o->pos() &&
                mLength == o->mLength &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                sameTilesAs(other);
    }
    return false;
}

QPolygonF WallObject::calcShape() const
{
    if (isN())
       return QRectF(mX - 6/30.0, mY, 12/30.0, mLength * 30/30.0);
    if (isW())
        return QRectF(mX, mY - 6/30.0, mLength * 30/30.0, 12/30.0);
    return QPolygonF();
}

/////

BuildingObject *Window::clone() const
{
    Window *clone = new Window(mFloor, mX, mY, mDir);
    clone->mTile = mTile;
    clone->mCurtainsTile = mCurtainsTile;
    clone->mShuttersTile = mShuttersTile;
    return clone;
}

bool Window::sameAs(BuildingObject *other)
{
    if (Window *o = other->asWindow()) {
        return mDir == o->mDir && pos() == o->pos() &&
                (!mFloor || !o->mFloor || mFloor->level() == o->mFloor->level()) &&
                sameTilesAs(other);
    }
    return false;
}

QPolygonF Window::calcShape() const
{
    if (isN())
        return QRectF(mX + 7/30.0, mY - 3/30.0, 16/30.0, 6/30.0);
    if (isW())
        return QRectF(mX - 3/30.0, mY + 7/30.0, 6/30.0, 16/30.0);
    return QPolygonF();
}

/////

bool BasementAccess::fromString(const QString &value)
{
    QStringList ss = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (ss.length() == 3) {
        bool ok;
        mX = ss[0].toInt(&ok);
        if (ok == false) {
            return false;
        }
        mY = ss[1].toInt(&ok);
        if (ok == false) {
            return false;
        }
        QString direction = ss[2];
        if (direction == QStringLiteral("N")) {
            mDirection = BuildingObject::Direction::N;
        } else if (direction == QStringLiteral("S")) {
            mDirection = BuildingObject::Direction::S;
        } else if (direction == QStringLiteral("W")) {
            mDirection = BuildingObject::Direction::W;
        } else if (direction == QStringLiteral("E")) {
            mDirection = BuildingObject::Direction::E;
        } else {
            return false;
        }
        return true;
    }
    return false;
}

QString BasementAccess::toString() const
{
    return QStringLiteral("%1 %2 %3").arg(mX).arg(mY).arg(dirString());
}

QString BasementAccess::dirString() const
{
    static const char *s[] = { "N", "S", "E", "W", "Invalid" };
    return QLatin1String(s[mDirection]);
}
