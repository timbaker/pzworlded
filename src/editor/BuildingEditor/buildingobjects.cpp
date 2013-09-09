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

    FurnitureTile *oldTile = mFurnitureTile;
    FurnitureTile *newTile = mFurnitureTile;
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
    newTile = oldTile->owner()->tile(map[oldTile->orient()]);

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

        case CornerInnerSW: mType = CornerInnerNW; break;
        case CornerInnerNW: mType = CornerInnerNE; break;
        case CornerInnerNE: mType = CornerInnerSE; break;
        case CornerInnerSE: mType = CornerInnerSW; break;

        case CornerOuterSW: mType = CornerOuterNW; break;
        case CornerOuterNW: mType = CornerOuterNE; break;
        case CornerOuterNE: mType = CornerOuterSE; break;
        case CornerOuterSE: mType = CornerOuterSW; break;
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

        case CornerInnerSW: mType = CornerInnerSE; break;
        case CornerInnerNW: mType = CornerInnerSW; break;
        case CornerInnerNE: mType = CornerInnerNW; break;
        case CornerInnerSE: mType = CornerInnerNE; break;

        case CornerOuterSW: mType = CornerOuterSE; break;
        case CornerOuterNW: mType = CornerOuterSW; break;
        case CornerOuterNE: mType = CornerOuterNW; break;
        case CornerOuterSE: mType = CornerOuterNE; break;
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
        }
        break;
    case FlatTop:
        mHeight = height;
        if (mDepth == InvalidDepth)
            mDepth = Three;
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
    default:
        break;
    }
}

void RoofObject::resize(int width, int height, bool halfDepth)
{
    mHalfDepth = halfDepth;
    if (isCorner()) {
        height = width = qMax(width, height);
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
    #if 0
        BTC_RoofSlopes::FlatTopW1, BTC_RoofSlopes::FlatTopW2, BTC_RoofSlopes::FlatTopW3,
        BTC_RoofSlopes::FlatTopN1, BTC_RoofSlopes::FlatTopN2, BTC_RoofSlopes::FlatTopN3,
    #endif
        BTC_RoofSlopes::Inner1, BTC_RoofSlopes::Inner2, BTC_RoofSlopes::Inner3,
        BTC_RoofSlopes::Outer1, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::Outer3,
        BTC_RoofSlopes::InnerPt5, BTC_RoofSlopes::InnerOnePt5, BTC_RoofSlopes::InnerTwoPt5,
        BTC_RoofSlopes::OuterPt5, BTC_RoofSlopes::OuterOnePt5, BTC_RoofSlopes::OuterTwoPt5,
        BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerSW3,
        BTC_RoofSlopes::CornerNE1, BTC_RoofSlopes::CornerNE2, BTC_RoofSlopes::CornerNE3,
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
        BTC_RoofCaps::CapGapE1, BTC_RoofCaps::CapGapE2, BTC_RoofCaps::CapGapE3
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
    }
    return QRect();
}

QRect RoofObject::cornerOuter()
{
    QRect r = bounds();
    switch (mType) {
    case CornerOuterSE:
        return r;
    }
    return QRect();
}

QString RoofObject::typeToString(RoofObject::RoofType type)
{
    switch (type) {
    case SlopeW: return QLatin1String("SlopeW");
    case SlopeN: return QLatin1String("SlopeN");
    case SlopeE: return QLatin1String("SlopeE");
    case SlopeS: return QLatin1String("SlopeS");

    case PeakWE: return QLatin1String("PeakWE");
    case PeakNS: return QLatin1String("PeakNS");

    case DormerW: return QLatin1String("DormerW");
    case DormerN: return QLatin1String("DormerN");
    case DormerE: return QLatin1String("DormerE");
    case DormerS: return QLatin1String("DormerS");

    case FlatTop: return QLatin1String("FlatTop");

    case CornerInnerSW: return QLatin1String("CornerInnerSW");
    case CornerInnerNW: return QLatin1String("CornerInnerNW");
    case CornerInnerNE: return QLatin1String("CornerInnerNE");
    case CornerInnerSE: return QLatin1String("CornerInnerSE");

    case CornerOuterSW: return QLatin1String("CornerOuterSW");
    case CornerOuterNW: return QLatin1String("CornerOuterNW");
    case CornerOuterNE: return QLatin1String("CornerOuterNE");
    case CornerOuterSE: return QLatin1String("CornerOuterSE");
    default:
        break;
    }

    return QLatin1String("Invalid");
}

RoofObject::RoofType RoofObject::typeFromString(const QString &s)
{
    if (s == QLatin1String("SlopeW")) return SlopeW;
    if (s == QLatin1String("SlopeN")) return SlopeN;
    if (s == QLatin1String("SlopeE")) return SlopeE;
    if (s == QLatin1String("SlopeS")) return SlopeS;

    if (s == QLatin1String("PeakWE")) return PeakWE;
    if (s == QLatin1String("PeakNS")) return PeakNS;

    if (s == QLatin1String("DormerW")) return DormerW;
    if (s == QLatin1String("DormerN")) return DormerN;
    if (s == QLatin1String("DormerE")) return DormerE;
    if (s == QLatin1String("DormerS")) return DormerS;

    if (s == QLatin1String("FlatTop")) return FlatTop;

    if (s == QLatin1String("CornerInnerSW")) return CornerInnerSW;
    if (s == QLatin1String("CornerInnerNW")) return CornerInnerNW;
    if (s == QLatin1String("CornerInnerNE")) return CornerInnerNE;
    if (s == QLatin1String("CornerInnerSE")) return CornerInnerSE;

    if (s == QLatin1String("CornerOuterSW")) return CornerOuterSW;
    if (s == QLatin1String("CornerOuterNW")) return CornerOuterNW;
    if (s == QLatin1String("CornerOuterNE")) return CornerOuterNE;
    if (s == QLatin1String("CornerOuterSE")) return CornerOuterSE;

    return InvalidType;
}

QString RoofObject::depthToString(RoofObject::RoofDepth depth)
{
    switch (depth) {
    case Zero: return QLatin1String("Zero");
    case Point5: return QLatin1String("Point5");
    case One: return QLatin1String("One");
    case OnePoint5: return QLatin1String("OnePoint5");
    case Two: return QLatin1String("Two");
    case TwoPoint5: return QLatin1String("TwoPoint5");
    case Three: return QLatin1String("Three");
    default:
        break;
    }

    qFatal("unhandled roof object depth");

    return QLatin1String("Invalid");
}

RoofObject::RoofDepth RoofObject::depthFromString(const QString &s)
{
    if (s == QLatin1String("Zero")) return Zero;
    if (s == QLatin1String("Point5")) return Point5;
    if (s == QLatin1String("One")) return One;
    if (s == QLatin1String("OnePoint5")) return OnePoint5;
    if (s == QLatin1String("Two")) return Two;
    if (s == QLatin1String("TwoPoint5")) return OnePoint5;
    if (s == QLatin1String("Three")) return Three;

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

QPolygonF Window::calcShape() const
{
    if (isN())
        return QRectF(mX + 7/30.0, mY - 3/30.0, 16/30.0, 6/30.0);
    if (isW())
        return QRectF(mX - 3/30.0, mY + 7/30.0, 6/30.0, 16/30.0);
    return QPolygonF();
}

/////
