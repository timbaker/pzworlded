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

#ifndef BUILDINGTILES_H
#define BUILDINGTILES_H

#include <QImage>
#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Tiled {
class Tile;
class Tileset;
}

namespace BuildingEditor {

class BuildingTileCategory;

class BuildingTile
{
public:
    BuildingTile(const QString &tilesetName, int index) :
        mTilesetName(tilesetName),
        mIndex(index)
    {}

    virtual bool isNone() const
    { return false; }

    virtual QString name() const;

    QString mTilesetName;
    int mIndex;
};

class NoneBuildingTile : public BuildingTile
{
public:
    NoneBuildingTile() :
        BuildingTile(QString(), 0)
    {}

    virtual bool isNone() const
    { return true; }

    virtual QString name() const
    { return QString(); }
};

class BuildingTileEntry
{
public:
    BuildingTileEntry(BuildingTileCategory *category);

    BuildingTileCategory *category() const
    { return mCategory; }

    BuildingTile *displayTile() const;

    void setTile(int e, BuildingTile *btile);
    BuildingTile *tile(int n) const;

    int tileCount() const
    { return mTiles.size(); }

    /* NOTE about these offsets.  Some roof tiles must be placed at an x,y
      offset from their actual position in order be displayed in the expected
      *visual* position.  Ideally, every roof tile would be created so that
      it doesn't need to be offset from its actual x,y position in the map.
      */
    void setOffset(int e, const QPoint &offset);
    QPoint offset(int e) const;

    bool usesTile(BuildingTile *btile) const;

    // Check for the universal null entry
    virtual bool isNone() const { return false; }
    virtual BuildingTileEntry *asNone() { return 0; }

    bool equals(BuildingTileEntry *other) const;

    BuildingTileEntry *asCategory(int n);
    BuildingTileEntry *asExteriorWall();
    BuildingTileEntry *asInteriorWall();
    BuildingTileEntry *asFloor();
    BuildingTileEntry *asDoor();
    BuildingTileEntry *asDoorFrame();
    BuildingTileEntry *asWindow();
    BuildingTileEntry *asCurtains();
    BuildingTileEntry *asStairs();
    BuildingTileEntry *asRoofCap();
    BuildingTileEntry *asRoofSlope();
    BuildingTileEntry *asRoofTop();

    BuildingTileCategory *mCategory;
    QVector<BuildingTile*> mTiles;
    QVector<QPoint> mOffsets;
};

class NoneBuildingTileEntry : public BuildingTileEntry
{
public:
    NoneBuildingTileEntry(BuildingTileCategory *category) :
        BuildingTileEntry(category)
    {}
    BuildingTileEntry *asNone() { return this; }
    bool isNone() const { return true; }
};

class BuildingTileCategory
{
public:
    enum TileEnum
    {
        Invalid = -1
    };

    BuildingTileCategory(const QString &name, const QString &label,
                         int displayIndex);

    QString name() const
    { return mName; }

    void setLabel(const QString &label)
    { mLabel = label; }

    QString label() const
    { return mLabel; }

    int displayIndex() const
    { return mDisplayIndex; }

    void setDefaultEntry(BuildingTileEntry *entry)
    { mDefaultEntry = entry; }

    BuildingTileEntry *defaultEntry() const
    { return mDefaultEntry; }

    const QList<BuildingTileEntry*> &entries() const
    { return mEntries; }

    BuildingTileEntry *entry(int index) const;

    int entryCount() const
    { return mEntries.size(); }

    int indexOf(BuildingTileEntry *entry) const
    { return mEntries.indexOf(entry); }

    void insertEntry(int index, BuildingTileEntry *entry);
    BuildingTileEntry *removeEntry(int index);

    BuildingTileEntry *noneTileEntry();

    int enumCount() const
    { return mEnumNames.size(); }

    QString enumToString(int index) const;
    int enumFromString(const QString &s) const;

    void setShadowImage(const QImage &image)
    { mShadowImage = image; }

    QImage shadowImage() const
    { return mShadowImage; }

    virtual int shadowCount() const { return enumCount(); }
    virtual int shadowToEnum(int shadowIndex) { return shadowIndex; }
    virtual int enumToShadow(int e);

    /*
     * This is the method used to fill in all the tiles in an entry from a single
     * tile. For example, given a door tile, all the different door tiles for each
     * of the enumeration values (west, north, southeast, etc) used by the doors
     * category are assigned to a new entry.
     */
    virtual BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileEntry *findMatch(BuildingTileEntry *entry) const;
    bool usesTile(Tiled::Tile *tile) const;

    virtual bool canAssignNone() const
    { return false; }

    virtual bool isNone() const { return false; }

    virtual BuildingTileCategory *asNone() { return 0; }
    virtual BuildingTileCategory *asExteriorWalls() { return 0; }
    virtual BuildingTileCategory *asInteriorWalls() { return 0; }
    virtual BuildingTileCategory *asFloors() { return 0; }
    virtual BuildingTileCategory *asDoors() { return 0; }
    virtual BuildingTileCategory *asDoorFrames() { return 0; }
    virtual BuildingTileCategory *asWindows() { return 0; }
    virtual BuildingTileCategory *asCurtains() { return 0; }
    virtual BuildingTileCategory *asStairs() { return 0; }
    virtual BuildingTileCategory *asGrimeFloor() { return 0; }
    virtual BuildingTileCategory *asGrimeWall() { return 0; }
    virtual BuildingTileCategory *asRoofCaps() { return 0; }
    virtual BuildingTileCategory *asRoofSlopes() { return 0; }
    virtual BuildingTileCategory *asRoofTops() { return 0; }

protected:
    QString mName;
    QString mLabel;
    int mDisplayIndex;
    QList<BuildingTileEntry*> mEntries;
    QStringList mEnumNames;
    BuildingTileEntry *mDefaultEntry;
    NoneBuildingTileEntry *mNoneTileEntry;
    QImage mShadowImage;
};

class NoneBuildingTileCategory : public BuildingTileCategory
{
public:
    NoneBuildingTileCategory()
        : BuildingTileCategory(QString(), QString(), 0)
    {}

    bool isNone() const { return true; }
    BuildingTileCategory *asNone() { return this; }
};

class BTC_Curtains : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        East,
        North,
        South,
        // Could add separate open/closed, but I want the user to be able to
        // choose opened/closed from the browser
        EnumCount
    };

    BTC_Curtains(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    virtual BuildingTileCategory *asCurtains() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Doors : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        WestOpen,
        NorthOpen,
        EnumCount
    };

    BTC_Doors(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asDoors() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_DoorFrames : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        EnumCount
    };

    BTC_DoorFrames(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asDoorFrames() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Floors : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        Floor,
        EnumCount
    };

    BTC_Floors(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileCategory *asFloors() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Stairs : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West1,
        West2,
        West3,
        North1,
        North2,
        North3,
        EnumCount
    };

    BTC_Stairs(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileCategory *asStairs() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Walls : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        EnumCount
    };

    BTC_Walls(const QString &name, const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    int shadowToEnum(int shadowIndex);
};

class BTC_EWalls : public BTC_Walls
{
public:
    BTC_EWalls(const QString &label) :
        BTC_Walls(QLatin1String("exterior_walls"), label)
    {}

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asExteriorWalls() { return this; }
};

class BTC_IWalls : public BTC_Walls
{
public:
    BTC_IWalls(const QString &label) :
        BTC_Walls(QLatin1String("interior_walls"), label)
    {}

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asInteriorWalls() { return this; }
};

class BTC_Windows : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        // TODO: open/broken
        EnumCount
    };

    BTC_Windows(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asWindows() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_RoofCaps : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        EnumCount
    };

    BTC_RoofCaps(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileCategory *asRoofCaps() { return this; }

    int shadowCount() { return EnumCount; }
    int shadowToEnum(int shadowIndex);
};

class BTC_RoofSlopes : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        // Sloped sides
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,
#if 0
        // Flat rooftops
        FlatTopW1, FlatTopW2, FlatTopW3,
        FlatTopN1, FlatTopN2, FlatTopN3,
#endif
        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,

        EnumCount
    };

    BTC_RoofSlopes(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileCategory *asRoofSlopes() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_RoofTops : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West1,
        West2,
        West3,
        North1,
        North2,
        North3,
        EnumCount
    };

    BTC_RoofTops(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asRoofTops() { return this; }

    int shadowCount() const { return 3; }
    int shadowToEnum(int shadowIndex);
};

class BTC_GrimeFloor : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        East,
        South,
        SouthWest,
        NorthWest,
        NorthEast,
        SouthEast,
        EnumCount
    };

    BTC_GrimeFloor(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asGrimeFloor() { return this; }

    int shadowCount() const { return 8; }
    int shadowToEnum(int shadowIndex);
};


class BTC_GrimeWall : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        EnumCount
    };

    BTC_GrimeWall(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asGrimeWall() { return this; }

    int shadowCount() const { return 8; }
    int shadowToEnum(int shadowIndex);
};

class BuildingTilesMgr : public QObject
{
    Q_OBJECT
public:
    enum CategoryEnum {
        ExteriorWalls,
        InteriorWalls,
        Floors,
        Doors,
        DoorFrames,
        Windows,
        Curtains,
        Stairs,
        GrimeFloor,
        GrimeWall,
        RoofCaps,
        RoofSlopes,
        RoofTops,
        Count
    };

    static BuildingTilesMgr *instance();
    static void deleteInstance();

    BuildingTilesMgr();
    ~BuildingTilesMgr();

    BuildingTile *add(const QString &tileName);

    BuildingTile *get(const QString &tileName, int offset = 0);

    const QList<BuildingTileCategory*> &categories() const
    { return mCategories; }

    int categoryCount() const
    { return mCategories.count(); }

    BuildingTileCategory *category(int index) const
    {
        if (index < 0 || index >= mCategories.count())
            return 0;
        return mCategories[index];
    }

    BuildingTileCategory *category(const QString &name) const
    {
        if (mCategoryByName.contains(name))
            return mCategoryByName[name];
        return 0;
    }

    int indexOf(BuildingTileCategory *category) const
    { return mCategories.indexOf(category); }

    int indexOf(const QString &name) const
    {
        if (BuildingTileCategory *category = this->category(name))
            return mCategories.indexOf(category);
        return -1;
    }

    static QString nameForTile(const QString &tilesetName, int index);
    static QString nameForTile(Tiled::Tile *tile);
    static bool parseTileName(const QString &tileName, QString &tilesetName, int &index);
    static QString adjustTileNameIndex(const QString &tileName, int offset);
    static QString normalizeTileName(const QString &tileName);

    Tiled::Tile *tileFor(const QString &tileName);
    Tiled::Tile *tileFor(BuildingTile *tile, int offset = 0);

    BuildingTile *fromTiledTile(Tiled::Tile *tile);

    BuildingTile *noneTile() const
    { return mNoneBuildingTile; }

    BuildingTileEntry *noneTileEntry() const
    { return mNoneTileEntry; }

    Tiled::Tile *noneTiledTile() const
    { return mNoneTiledTile; }

    void entryTileChanged(BuildingTileEntry *entry, int e);

    QString txtName();
    QString txtPath();

    bool readTxt();
    void writeTxt(QWidget *parent = 0);

    BuildingTileCategory *catEWalls() const { return mCatEWalls; }
    BuildingTileCategory *catIWalls() const { return mCatIWalls; }
    BuildingTileCategory *catFloors() const { return mCatFloors; }
    BuildingTileCategory *catDoors() const { return mCatDoors; }
    BuildingTileCategory *catDoorFrames() const { return mCatDoorFrames; }
    BuildingTileCategory *catWindows() const { return mCatWindows; }
    BuildingTileCategory *catCurtains() const { return mCatCurtains; }
    BuildingTileCategory *catStairs() const { return mCatStairs; }
    BuildingTileCategory *catRoofCaps() const { return mCatRoofCaps; }
    BuildingTileCategory *catRoofSlopes() const { return mCatRoofSlopes; }
    BuildingTileCategory *catRoofTops() const { return mCatRoofTops; }

    BuildingTileEntry *defaultCategoryTile(int e) const;

    BuildingTileEntry *defaultExteriorWall() const;
    BuildingTileEntry *defaultInteriorWall() const;
    BuildingTileEntry *defaultFloorTile() const;
    BuildingTileEntry *defaultDoorTile() const;
    BuildingTileEntry *defaultDoorFrameTile() const;
    BuildingTileEntry *defaultWindowTile() const;
    BuildingTileEntry *defaultCurtainsTile() const;
    BuildingTileEntry *defaultStairsTile() const;

    BuildingTileEntry *defaultRoofCapTiles() const;
    BuildingTileEntry *defaultRoofSlopeTiles() const;
    BuildingTileEntry *defaultRoofTopTiles() const;

    QString errorString() const
    { return mError; }

private:
    bool upgradeTxt();
    bool mergeTxt();

signals:
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void entryTileChanged(BuildingTileEntry *entry);

private:
    static BuildingTilesMgr *mInstance;

    QList<BuildingTileCategory*> mCategories;
    QMap<QString,BuildingTileCategory*> mCategoryByName;

    QList<BuildingTile*> mTiles;
    QMap<QString,BuildingTile*> mTileByName;

    Tiled::Tile *mMissingTile;
    Tiled::Tile *mNoneTiledTile;
    BuildingTile *mNoneBuildingTile;

    BuildingTileCategory *mNoneCategory;
    BuildingTileEntry *mNoneTileEntry;

    int mRevision;
    int mSourceRevision;

    QString mError;

    // The categories
    BTC_Curtains *mCatCurtains;
    BTC_Doors *mCatDoors;
    BTC_DoorFrames *mCatDoorFrames;
    BTC_Floors *mCatFloors;
    BTC_Stairs *mCatStairs;
    BTC_EWalls *mCatEWalls;
    BTC_IWalls *mCatIWalls;
    BTC_Windows *mCatWindows;
    BTC_GrimeFloor *mCatGrimeFloor;
    BTC_GrimeWall *mCatGrimeWall;

    BTC_RoofCaps *mCatRoofCaps;
    BTC_RoofSlopes *mCatRoofSlopes;
    BTC_RoofTops *mCatRoofTops;
};

} // namespace BuildingEditor

#endif // BUILDINGTILES_H
