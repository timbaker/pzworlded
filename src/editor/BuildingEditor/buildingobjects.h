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

#ifndef BUILDINGOBJECTS_H
#define BUILDINGOBJECTS_H

#include <QPolygon>
#include <QRegion>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FurnitureObject;
class FurnitureTile;
class RoofObject;
class RoofTile;
class Stairs;
class WallObject;
class Window;

class BuildingObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W,
        Invalid
    };

    BuildingObject(BuildingFloor *floor, int x, int y, Direction mDir);

    void setFloor(BuildingFloor *floor)
    { mFloor = floor; }

    BuildingFloor *floor() const
    { return mFloor; }

    int index();

    virtual QRect bounds() const
    { return QRect(mX, mY, 1, 1); }

    void setPos(int x, int y)
    { mX = x, mY = y; }

    void setPos(const QPoint &pos)
    { setPos(pos.x(), pos.y()); }

    QPoint pos() const
    { return QPoint(mX, mY); }

    int x() const { return mX; }
    int y() const { return mY; }

    void setDir(Direction dir)
    { mDir = dir; }

    Direction dir() const
    { return mDir; }

    bool isW() const
    { return mDir == W; }

    bool isN() const
    { return mDir == N; }

    QString dirString() const;
    static Direction dirFromString(const QString &s);

    virtual void setTile(BuildingTileEntry *tile, int alternate = 0)
    {
        Q_UNUSED(alternate)
        mTile = tile;
    }

    virtual BuildingTileEntry *tile(int alternate = 0) const
    {
        Q_UNUSED(alternate)
        return mTile;
    }

    virtual bool isValidPos(const QPoint &offset = QPoint(),
                            BuildingEditor::BuildingFloor *floor = 0) const;

    virtual void rotate(bool right);
    virtual void flip(bool horizontal);

    virtual bool affectsFloorAbove() const { return false; }

    virtual BuildingObject *clone() const = 0;

    virtual QPolygonF calcShape() const = 0;

    virtual Door *asDoor() { return 0; }
    virtual Window *asWindow() { return 0; }
    virtual Stairs *asStairs() { return 0; }
    virtual FurnitureObject *asFurniture() { return 0; }
    virtual RoofObject *asRoof() { return 0; }
    virtual WallObject *asWall() { return 0; }

protected:
    BuildingFloor *mFloor;
    Direction mDir;
    int mX;
    int mY;
    BuildingTileEntry *mTile;
};

class Door : public BuildingObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    Door *asDoor() { return this; }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    { alternate ? mFrameTile = tile : mTile = tile; }

    BuildingTileEntry *tile(int alternate = 0) const
    { return alternate ? mFrameTile : mTile; }

    BuildingTileEntry *frameTile() const
    { return mFrameTile; }
private:
    BuildingTileEntry *mFrameTile;
};

class Stairs : public BuildingObject
{
public:
    Stairs(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {
    }

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    bool affectsFloorAbove() const { return true; }

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    Stairs *asStairs() { return this; }

    int getOffset(int x, int y);
};

class WallObject : public BuildingObject
{
public:
    WallObject(BuildingFloor *floor, int x, int y, Direction dir, int length);

    QRect bounds() const;

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    { alternate ? mInteriorTile = tile : mTile = tile; }

    BuildingTileEntry *tile(int alternate = 0) const
    { return alternate ? mInteriorTile : mTile; }

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    WallObject *asWall() { return this; }

    void setLength(int length)
    { mLength = length; }

    int length() const
    { return mLength; }

private:
    int mLength;
    BuildingTileEntry *mInteriorTile;
};

class Window : public BuildingObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mCurtainsTile(0)
    {

    }

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    { alternate ? mCurtainsTile = tile : mTile = tile; }

    BuildingTileEntry *tile(int alternate = 0) const
    { return alternate ? mCurtainsTile : mTile; }

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    Window *asWindow() { return this; }

    int getOffset() const
    { return (mDir == N) ? 1 : 0; }

    BuildingTileEntry *curtainsTile()
    { return mCurtainsTile; }

private:
    BuildingTileEntry *mCurtainsTile;
};

class FurnitureObject : public BuildingObject
{
public:
    FurnitureObject(BuildingFloor *floor, int x, int y);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    FurnitureObject *asFurniture() { return this; }

    void setFurnitureTile(FurnitureTile *tile);

    FurnitureTile *furnitureTile() const
    { return mFurnitureTile; }

    bool inWallLayer() const;

private:
    FurnitureTile *mFurnitureTile;
};

class RoofObject : public BuildingObject
{
public:
    enum RoofType {
        SlopeW,
        SlopeN,
        SlopeE,
        SlopeS,
        PeakWE,
        PeakNS,
        FlatTop,
        CornerInnerSW,
        CornerInnerNW,
        CornerInnerNE,
        CornerInnerSE,
        CornerOuterSW,
        CornerOuterNW,
        CornerOuterNE,
        CornerOuterSE,
        InvalidType
    };

    enum RoofDepth {
        Zero,
        Point5,
        One,
        OnePoint5,
        Two,
        TwoPoint5,
        Three,
        InvalidDepth
    };

    /* Enum for setTile() and tile() 'alternate' argument. */
    enum RoofObjectTiles {
        TileCap,
        TileSlope,
        TileTop
    };

    RoofObject(BuildingFloor *floor, int x, int y, int width, int height,
               RoofType type, RoofDepth depth,
               bool cappedW, bool cappedN, bool cappedE, bool cappedS);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    void setTile(BuildingTileEntry *tile, int alternate = 0);

    BuildingTileEntry *tile(int alternate = 0) const;

    bool affectsFloorAbove() const { return true; }

    BuildingObject *clone() const;

    QPolygonF calcShape() const;

    RoofObject *asRoof() { return this; }

    void setCapTiles(BuildingTileEntry *entry);

    BuildingTileEntry *capTiles() const
    { return mCapTiles; }

    void setSlopeTiles(BuildingTileEntry *entry);

    BuildingTileEntry *slopeTiles() const
    { return mSlopeTiles; }

    void setTopTiles(BuildingTileEntry *entry);

    BuildingTileEntry *topTiles() const
    { return mTopTiles; }

    void setType(RoofType type);

    RoofType roofType() const
    { return mType; }

    bool isCorner() const
    { return mType >= CornerInnerSW && mType <= CornerOuterSE; }

    void setWidth(int width);

    int width() const
    { return mWidth; }

    void setHeight(int height);

    int height() const
    { return mHeight; }

    void resize(int width, int height);

    void depthUp();
    void depthDown();

    bool isDepthMax();
    bool isDepthMin();

    RoofDepth depth() const
    { return mDepth; }

    int actualWidth() const;
    int actualHeight() const;

    int slopeThickness() const;
    bool isHalfDepth() const;

    bool isCappedW() const
    { return mCappedW; }

    bool isCappedN() const
    { return mCappedN; }

    bool isCappedE() const
    { return mCappedE; }

    bool isCappedS() const
    { return mCappedS; }

    void toggleCappedW();
    void toggleCappedN();
    void toggleCappedE();
    void toggleCappedS();

    void setDefaultCaps();

    enum RoofTile {
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,
#if 0
        FlatTopW1, FlatTopW2, FlatTopW3,
        FlatTopN1, FlatTopN2, FlatTopN3,
#endif

        // Corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,

        // Caps
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3
    };

    int getOffset(RoofTile getOffset) const;

    QRect westEdge();
    QRect northEdge();
    QRect eastEdge();
    QRect southEdge();
    QRect westGap(RoofDepth depth);
    QRect northGap(RoofDepth depth);
    QRect eastGap(RoofDepth depth);
    QRect southGap(RoofDepth depth);
    QRect flatTop();

    QString typeToString() const
    { return typeToString(mType); }

    static QString typeToString(RoofType type);
    static RoofType typeFromString(const QString &s);

    QString depthToString() const
    { return depthToString(mDepth); }

    static QString depthToString(RoofDepth depth);
    static RoofDepth depthFromString(const QString &s);

private:
    int mWidth;
    int mHeight;
    RoofType mType;
    RoofDepth mDepth;
    bool mCappedW;
    bool mCappedN;
    bool mCappedE;
    bool mCappedS;
    BuildingTileEntry *mCapTiles;
    BuildingTileEntry *mSlopeTiles;
    BuildingTileEntry *mTopTiles;
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
