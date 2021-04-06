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

#include "buildingreader.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include "qtlockedfile.h"
using namespace SharedTools;

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QXmlStreamReader>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

using namespace BuildingEditor;

// version="1.0"
#define VERSION1 1

// version="2"
// BuildingTileEntry rewrite
// added FurnitureTiles::mCorners
#define VERSION2 2

// Renamed some layers
// Reversed WallObject direction
// user_tile index encodes rotation
#define VERSION3 3

#define VERSION_LATEST VERSION3

static const uint RotateFlag90 = 0x80000000;
static const uint RotateFlag180 = 0x40000000;
static const uint RotateFlag270 = 0x20000000;

namespace BuildingEditor {

class FakeBuildingTilesMgr;

#define DECLARE_FAKE_CATEGORY(C) \
class Fake_##C : public BTC_##C \
{ \
public: \
    Fake_##C(FakeBuildingTilesMgr *btiles) : \
        BTC_##C(QString()), \
        mFakeBuildingTilesMgr(btiles) \
    { } \
private: \
    BuildingTile *getTile(const QString &tilesetName, int offset); \
    FakeBuildingTilesMgr *mFakeBuildingTilesMgr; \
};
DECLARE_FAKE_CATEGORY(Curtains)
DECLARE_FAKE_CATEGORY(Doors)
DECLARE_FAKE_CATEGORY(DoorFrames)
DECLARE_FAKE_CATEGORY(Floors)
DECLARE_FAKE_CATEGORY(EWalls)
DECLARE_FAKE_CATEGORY(IWalls)
DECLARE_FAKE_CATEGORY(EWallTrim)
DECLARE_FAKE_CATEGORY(IWallTrim)
DECLARE_FAKE_CATEGORY(Stairs)
DECLARE_FAKE_CATEGORY(Shutters)
DECLARE_FAKE_CATEGORY(Windows)
DECLARE_FAKE_CATEGORY(GrimeFloor)
DECLARE_FAKE_CATEGORY(GrimeWall)
DECLARE_FAKE_CATEGORY(RoofCaps)
DECLARE_FAKE_CATEGORY(RoofSlopes)
DECLARE_FAKE_CATEGORY(RoofTops)

#define NEW_FAKE_CATEGORY(C) \
    new Fake_##C(this);

class FakeBuildingTilesMgr
{
public:
    FakeBuildingTilesMgr()
    {
        mCatCurtains = NEW_FAKE_CATEGORY(Curtains);
        mCatDoors = NEW_FAKE_CATEGORY(Doors);
        mCatDoorFrames = NEW_FAKE_CATEGORY(DoorFrames);
        mCatFloors = NEW_FAKE_CATEGORY(Floors);
        mCatEWalls = NEW_FAKE_CATEGORY(EWalls);
        mCatIWalls = NEW_FAKE_CATEGORY(IWalls);
        mCatEWallTrim = NEW_FAKE_CATEGORY(EWallTrim);
        mCatIWallTrim = NEW_FAKE_CATEGORY(IWallTrim);
        mCatStairs = NEW_FAKE_CATEGORY(Stairs);
        mCatShutters = NEW_FAKE_CATEGORY(Shutters);
        mCatWindows = NEW_FAKE_CATEGORY(Windows);
        mCatGrimeFloor = NEW_FAKE_CATEGORY(GrimeFloor);
        mCatGrimeWall = NEW_FAKE_CATEGORY(GrimeWall);
        mCatRoofCaps = NEW_FAKE_CATEGORY(RoofCaps);
        mCatRoofSlopes = NEW_FAKE_CATEGORY(RoofSlopes);
        mCatRoofTops = NEW_FAKE_CATEGORY(RoofTops);

        mCategories << mCatEWalls << mCatIWalls << mCatEWallTrim << mCatIWallTrim
                       << mCatFloors << mCatDoors
                       << mCatDoorFrames << mCatWindows << mCatCurtains
                       << mCatShutters << mCatStairs
                       << mCatGrimeFloor << mCatGrimeWall
                       << mCatRoofCaps << mCatRoofSlopes << mCatRoofTops;

        foreach (BuildingTileCategory *category, mCategories) {
            mCategoryByName[category->name()] = category;
            mUsedCategories[category] = false;
        }

        mNoneBuildingTile = new NoneBuildingTile();

        mNoneCategory = new NoneBuildingTileCategory();
        mNoneTileEntry = new NoneBuildingTileEntry(mNoneCategory);
    }

    ~FakeBuildingTilesMgr()
    {
        // Some of these may have been deleted in fix().
        foreach (BuildingTileCategory *category, mCategories)
            if (!mUsedCategories[category])
                delete category;
        /*
        qDeleteAll(mCategories);
        delete mNoneBuildingTile;
        delete mNoneCategory;
        delete mNoneTileEntry;
        */
    }

    BuildingTile *add(const QString &tileName)
    {
        QString tilesetName;
        int tileIndex;
        parseTileName(tileName, tilesetName, tileIndex);
        BuildingTile *btile = new BuildingTile(tilesetName, tileIndex);
        Q_ASSERT(!mTileByName.contains(btile->name()));
        mTileByName[btile->name()] = btile;
        mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
        return btile;
    }

    BuildingTile *get(const QString &tileName, int offset = 0)
    {
        if (tileName.isEmpty())
            return noneTile();

        QString adjustedName = adjustTileNameIndex(tileName, offset); // also normalized

        if (!mTileByName.contains(adjustedName))
            add(adjustedName);
        return mTileByName[adjustedName];
    }

    BuildingTileCategory *category(const QString &name) const
    {
        if (mCategoryByName.contains(name))
            return mCategoryByName[name];
        return nullptr;
    }

    static QString nameForTile(const QString &tilesetName, int index)
    {
        // The only reason I'm padding the tile index is so that the tiles are sorted
        // by increasing tileset name and index.
        return tilesetName + QLatin1Char('_') +
                QString(QLatin1String("%1")).arg(index, 3, 10, QLatin1Char('0'));
    }

    static bool parseTileName(const QString &tileName, QString &tilesetName, int &index)
    {
        tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
        QString indexString = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1);
        // Strip leading zeroes from the tile index
        int i = 0;
        while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
            i++;
        indexString.remove(0, i);
        index = indexString.toInt();
        return true;
    }

    QString adjustTileNameIndex(const QString &tileName, int offset)
    {
        QString tilesetName;
        int index;
        parseTileName(tileName, tilesetName, index);
#if 0 // NOT REENTRANT, but not needed for reading old .tbx files
        // Currently, the only place this gets called with offset > 0 is by the
        // createEntryFromSingleTile() methods.  Those methods assume the tilesets
        // are 8 tiles wide.  Remap the offset onto the tileset's actual number of
        // columns.
        if (offset > 0) {
            if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
                TileMetaInfoMgr::instance()->loadTilesets(QList<Tileset*>() << ts);
                int rows = offset / 8;
                offset = rows * ts->columnCount() + offset % 8;
            }
        }
#endif
        index += offset;
        return nameForTile(tilesetName, index);
    }

    BuildingTile *noneTile() const
    { return mNoneBuildingTile; }

    BuildingTileEntry *noneTileEntry() const
    { return mNoneTileEntry; }

    BuildingTileCategory *catEWalls() const { return mCatEWalls; }
    BuildingTileCategory *catIWalls() const { return mCatIWalls; }
    BuildingTileCategory *catFloors() const { return mCatFloors; }
    BuildingTileCategory *catDoors() const { return mCatDoors; }
    BuildingTileCategory *catDoorFrames() const { return mCatDoorFrames; }
    BuildingTileCategory *catWindows() const { return mCatWindows; }
    BuildingTileCategory *catCurtains() const { return mCatCurtains; }
    BuildingTileCategory *catShutters() const { return mCatShutters; }
    BuildingTileCategory *catStairs() const { return mCatStairs; }
    BuildingTileCategory *catRoofCaps() const { return mCatRoofCaps; }
    BuildingTileCategory *catRoofSlopes() const { return mCatRoofSlopes; }
    BuildingTileCategory *catRoofTops() const { return mCatRoofTops; }

    QList<BuildingTileCategory*> mCategories;
    QMap<QString,BuildingTileCategory*> mCategoryByName;

    QList<BuildingTile*> mTiles;
    QMap<QString,BuildingTile*> mTileByName;

    BuildingTile *mNoneBuildingTile;

    BuildingTileCategory *mNoneCategory;
    BuildingTileEntry *mNoneTileEntry;

    Fake_Curtains *mCatCurtains;
    Fake_Doors *mCatDoors;
    Fake_DoorFrames *mCatDoorFrames;
    Fake_Floors *mCatFloors;
    Fake_Stairs *mCatStairs;
    Fake_EWalls *mCatEWalls;
    Fake_IWalls *mCatIWalls;
    Fake_EWallTrim *mCatEWallTrim;
    Fake_IWallTrim *mCatIWallTrim;
    Fake_Shutters *mCatShutters;
    Fake_Windows *mCatWindows;
    Fake_GrimeFloor *mCatGrimeFloor;
    Fake_GrimeWall *mCatGrimeWall;
    Fake_RoofCaps *mCatRoofCaps;
    Fake_RoofSlopes *mCatRoofSlopes;
    Fake_RoofTops *mCatRoofTops;

    QMap<BuildingTileCategory*,bool> mUsedCategories;
};

#define DECLARE_FAKE_GETTILE(C) \
BuildingTile *Fake_##C::getTile(const QString &tilesetName, int offset) \
{ \
    return mFakeBuildingTilesMgr->get(tilesetName, offset); \
}

DECLARE_FAKE_GETTILE(Curtains)
DECLARE_FAKE_GETTILE(Doors)
DECLARE_FAKE_GETTILE(DoorFrames)
DECLARE_FAKE_GETTILE(Floors)
DECLARE_FAKE_GETTILE(Stairs)
DECLARE_FAKE_GETTILE(EWalls)
DECLARE_FAKE_GETTILE(IWalls)
DECLARE_FAKE_GETTILE(EWallTrim)
DECLARE_FAKE_GETTILE(IWallTrim)
DECLARE_FAKE_GETTILE(Shutters)
DECLARE_FAKE_GETTILE(Windows)
DECLARE_FAKE_GETTILE(GrimeFloor)
DECLARE_FAKE_GETTILE(GrimeWall)
DECLARE_FAKE_GETTILE(RoofCaps)
DECLARE_FAKE_GETTILE(RoofSlopes)
DECLARE_FAKE_GETTILE(RoofTops)

} // namespace BuildingEditor

class BuildingEditor::BuildingReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(BuildingReaderPrivate)

public:
    BuildingReaderPrivate(BuildingReader *reader):
        p(reader),
        mBuilding(nullptr)
    {}

    Building *readBuilding(QIODevice *device, const QString &path);

    bool openFile(QtLockedFile *file);

    QString errorString() const;

private:
    void readUnknownElement();

    Building *readBuilding();

    FurnitureTiles *readFurnitureTiles();
    FurnitureTile *readFurnitureTile(FurnitureTiles *ftiles);
    BuildingTile *readFurnitureTile(FurnitureTile *ftile, QPoint &pos);

    BuildingTileEntry *readTileEntry();

    void readUserTiles();

    QList<BuildingTileEntry*> readUsedTiles();
    QList<FurnitureTiles*> readUsedFurniture();

    Room *readRoom();

    BuildingFloor *readFloor();
    void decodeCSVFloorData(BuildingFloor *floor, const QString &text);
    Room *getRoom(BuildingFloor *floor, int x, int y, int index);

    void decodeCSVTileData(BuildingFloor *floor, const QString &layerName, const QString &text);
    BuildingCell getUserTile(BuildingFloor *floor, int x, int y, int index);

    BuildingObject *readObject(BuildingFloor *floor);

    bool booleanFromString(const QString &s, bool &result);
    bool readPoint(const QString &name, QPoint &result);

    BuildingTileEntry *getEntry(const QString &s);
    QString version1TileToEntry(BuildingTileCategory *category,
                                const QString &tileName);

    FurnitureTiles *getFurniture(const QString &s);

    BuildingReader *p;

    QString mError;
    QString mPath;
    Building *mBuilding;
    QList<FurnitureTiles*> mFurnitureTiles;
    QList<BuildingTileEntry*> mEntries;
    QMap<QString,BuildingTileEntry*> mEntryMap;
    QStringList mUserTiles;
    int mVersion;

    FakeBuildingTilesMgr mFakeBuildingTilesMgr;
    FurnitureGroup mFakeFurnitureGroup;

    friend class BuildingReader;
    void fix(Building *building);

    FurnitureTiles *fixFurniture(FurnitureTiles *ftiles);
    QMap<FurnitureTiles*,FurnitureTiles*> fixedFurniture;
    QSet<FurnitureTiles*> deadFurniture;

    BuildingTileEntry *fixEntry(BuildingTileEntry *entry);
    QMap<BuildingTileEntry*,BuildingTileEntry*> fixedEntries;
    QSet<BuildingTileEntry*> deadEntries;

    QSet<BuildingTile*> deadTiles;

    QSet<BuildingTileCategory*> deadCategories;

    QXmlStreamReader xml;
};

bool BuildingReaderPrivate::openFile(QtLockedFile *file)
{
    if (!file->exists()) {
        mError = tr("File not found: %1").arg(file->fileName());
        return false;
    }
    if (!file->open(QFile::ReadOnly | QFile::Text)) {
        mError = tr("Unable to read file: %1").arg(file->fileName());
        return false;
    }
    if (!file->lock(QtLockedFile::ReadLock)) {
        mError = tr("Unable to lock file for reading: %1").arg(file->fileName());
        return false;
    }

    return true;
}

QString BuildingReaderPrivate::errorString() const
{
    if (!mError.isEmpty()) {
        return mError;
    } else {
        return tr("%3\n\nLine %1, column %2")
                .arg(xml.lineNumber())
                .arg(xml.columnNumber())
                .arg(xml.errorString());
    }
}

Building *BuildingReaderPrivate::readBuilding(QIODevice *device, const QString &path)
{
    mError.clear();
    mPath = path;
    Building *building = nullptr;

    xml.setDevice(device);

    if (xml.readNextStartElement() && xml.name() == QLatin1String("building")) {
        building = readBuilding();
    } else {
        xml.raiseError(tr("Not a building file."));
    }

    return building;
}

Building *BuildingReaderPrivate::readBuilding()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("building"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString versionString = atts.value(QLatin1String("version")).toString();
    if (versionString == QLatin1String("1.0"))
        mVersion = VERSION1;
    else {
        mVersion = versionString.toInt();
        if (mVersion <= 0 || mVersion > VERSION_LATEST) {
            xml.raiseError(tr("Unknown building version '%1'").arg(versionString));
            return nullptr;
        }
    }
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    mBuilding = new Building(width, height);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("furniture")) {
            if (FurnitureTiles *tiles = readFurnitureTiles()) {
                mFurnitureTiles += tiles;
                mFakeFurnitureGroup.mTiles += tiles;
            }
        } else if (xml.name() == QLatin1String("tile_entry")) {
            if (BuildingTileEntry *entry = readTileEntry())
                mEntries += entry;
        } else if (xml.name() == QLatin1String("user_tiles")) {
            readUserTiles();
        } else if (xml.name() == QLatin1String("used_tiles")) {
            mBuilding->setUsedTiles(readUsedTiles());
        } else if (xml.name() == QLatin1String("used_furniture")) {
            mBuilding->setUsedFurniture(readUsedFurniture());
        } else if (xml.name() == QLatin1String("room")) {
            if (Room *room = readRoom())
                mBuilding->insertRoom(mBuilding->roomCount(), room);
        } else if (xml.name() == QLatin1String("floor")) {
            if (BuildingFloor *floor = readFloor())
                mBuilding->insertFloor(mBuilding->floorCount(), floor);
        } else
            readUnknownElement();
    }

    FakeBuildingTilesMgr *btiles = &mFakeBuildingTilesMgr;
    for (int i = 0; i < Building::TileCount; i++) {
        QString entryString = atts.value(mBuilding->enumToString(i)).toString();
        if (mVersion == VERSION1) {
            if (i == Building::ExteriorWall)
                entryString = version1TileToEntry(btiles->catEWalls(), entryString);
            else if (i == Building::Door)
                entryString = version1TileToEntry(btiles->catDoors(), entryString);
            else if (i == Building::DoorFrame)
                entryString = version1TileToEntry(btiles->catDoorFrames(), entryString);
            else if (i == Building::Window)
                entryString = version1TileToEntry(btiles->catWindows(), entryString);
            else if (i == Building::Stairs)
                entryString = version1TileToEntry(btiles->catStairs(), entryString);
        }
        mBuilding->setTile(i, getEntry(entryString)->asCategory(mBuilding->categoryEnum(i)));
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete mBuilding;
        mBuilding = nullptr;
    }

    return mBuilding;
}

FurnitureTiles *BuildingReaderPrivate::readFurnitureTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("furniture"));

    const QXmlStreamAttributes atts = xml.attributes();
    bool corners = false;
    FurnitureTiles::FurnitureLayer layer = FurnitureTiles::LayerFurniture;
    if (mVersion >= VERSION2) {
        QString cornersString = atts.value(QLatin1String("corners")).toString();
        if (cornersString.length() && !booleanFromString(cornersString, corners))
            return nullptr;

        QString layerString = atts.value(QLatin1String("layer")).toString();
        if (layerString.length()) {
            layer = FurnitureTiles::layerFromString(layerString);
            if (layer == FurnitureTiles::InvalidLayer) {
                xml.raiseError(tr("Unknown furniture layer '%1'").arg(layerString));
                return nullptr;
            }
        }
    }

    FurnitureTiles *ftiles = new FurnitureTiles(corners);
    ftiles->setLayer(layer);

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("entry")) {
            if (FurnitureTile *ftile = readFurnitureTile(ftiles))
                ftiles->setTile(ftile);
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftiles;
        return nullptr;
    }

    if (FurnitureTiles *match = mFakeFurnitureGroup.findMatch(ftiles)) {
        delete ftiles;
        return match;
    }

    return ftiles;
}

FurnitureTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTiles *ftiles)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("entry"));

    const QXmlStreamAttributes atts = xml.attributes();
    QString orientString = atts.value(QLatin1String("orient")).toString();
    FurnitureTile::FurnitureOrientation orient =
            FurnitureGroups::orientFromString(orientString);
    if (orient == FurnitureTile::FurnitureUnknown) {
        xml.raiseError(tr("invalid furniture tile orientation '%1'").arg(orientString));
        return nullptr;
    }
    QString grimeString = atts.value(QLatin1String("grime")).toString();
    bool grime = true;
    if (grimeString.length() && !booleanFromString(grimeString, grime))
        return nullptr;

    FurnitureTile *ftile = new FurnitureTile(ftiles, orient);
    ftile->setAllowGrime(grime);

    if (mVersion == VERSION1) {
        // v1 didn't have FurnitureTiles::mCorners, it had either W/N/E/S
        // or SW/NW/NE/SE.
        if (FurnitureTile::isCornerOrient(orient) && !ftiles->hasCorners())
            ftiles->toggleCorners();
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tile")) {
            QPoint pos;
            if (BuildingTile *btile = readFurnitureTile(ftile, pos))
                ftile->setTile(pos.x(), pos.y(), btile);
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftile;
        return nullptr;
    }

    return ftile;
}

BuildingTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTile *ftile, QPoint &pos)
{
    Q_UNUSED(ftile)
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("tile"));
    const QXmlStreamAttributes atts = xml.attributes();
    int x = atts.value(QLatin1String("x")).toString().toInt();
    int y = atts.value(QLatin1String("y")).toString().toInt();
    if (x < 0 || y < 0) {
        xml.raiseError(tr("invalid furniture tile coordinates (%1,%2)")
                       .arg(x).arg(y));
        return nullptr;
    }
    pos.setX(x);
    pos.setY(y);
    QString tileName = atts.value(QLatin1String("name")).toString();
    BuildingTile *btile = mFakeBuildingTilesMgr.get(tileName);
    xml.skipCurrentElement();
    return btile;
}

BuildingTileEntry *BuildingReaderPrivate::readTileEntry()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("tile_entry"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString categoryName = atts.value(QLatin1String("category")).toString();
    BuildingTileCategory *category = mFakeBuildingTilesMgr.category(categoryName);
    if (!category) {
        xml.raiseError(tr("unknown category '%1'").arg(categoryName));
        return nullptr;
    }

    QScopedPointer<BuildingTileEntry> entry(new BuildingTileEntry(category));
    mFakeBuildingTilesMgr.mUsedCategories[category] = true;

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tile")) {
            const QXmlStreamAttributes atts = xml.attributes();
            const QString enumName = atts.value(QLatin1String("enum")).toString();
            int e = category->enumFromString(enumName);
            if (e == BuildingTileCategory::Invalid) {
                xml.raiseError(tr("Unknown %1 enum '%2'").arg(categoryName).arg(enumName));
                return nullptr;
            }
            const QString tileName = atts.value(QLatin1String("tile")).toString();
            BuildingTile *btile = mFakeBuildingTilesMgr.get(tileName);

            QPoint offset;
            if (!readPoint(QLatin1String("offset"), offset))
                return nullptr;

            entry->mTiles[e] = btile;
            entry->mOffsets[e] = offset;
            xml.skipCurrentElement();
        } else
            readUnknownElement();
    }

    if (BuildingTileEntry *match = category->findMatch(entry.data())) {
        return match;
    }

    return entry.take();
}

void BuildingReaderPrivate::readUserTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("user_tiles"));

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("tile")) {
            const QXmlStreamAttributes atts = xml.attributes();
            const QString tileName = atts.value(QLatin1String("tile")).toString();
            QString tilesetName;
            int tileID;
            if (tileName.isEmpty() || !mFakeBuildingTilesMgr.
                    parseTileName(tileName, tilesetName, tileID)) {
                xml.raiseError(tr("Invalid tile name '%1' in <user_tiles>")
                               .arg(tileName));
                return;
            }
            mUserTiles += tileName;
            xml.skipCurrentElement();
        } else
            readUnknownElement();
    }
}

QList<BuildingTileEntry *> BuildingReaderPrivate::readUsedTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("used_tiles"));

    QList<BuildingTileEntry *> result;

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement())
            break;
        if (xml.isCharacters() && !xml.isWhitespace()) {
            QStringList used = xml.text().toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            foreach (QString s, used) {
                if (BuildingTileEntry *entry = getEntry(s))
                    if (!entry->isNone())
                        result += entry;
            }
            xml.skipCurrentElement();
            break;
        }
    }

    return result;
}

QList<FurnitureTiles *> BuildingReaderPrivate::readUsedFurniture()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("used_furniture"));

    QList<FurnitureTiles *> result;

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement())
            break;
        if (xml.isCharacters() && !xml.isWhitespace()) {
            QStringList used = xml.text().toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
            foreach (QString s, used) {
                if (FurnitureTiles *ftiles = getFurniture(s))
                    result += ftiles;
            }
            xml.skipCurrentElement();
            break;
        }
    }

    return result;
}

Room *BuildingReaderPrivate::readRoom()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("room"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("Name")).toString();
    const QString internalName = atts.value(QLatin1String("InternalName")).toString();
    const QString color = atts.value(QLatin1String("Color")).toString();
    QMap<int,QString> tiles;
    for (int i = 0; i < Room::TileCount; i++)
        tiles[i] = atts.value(Room::enumToString(i)).toString();

    if (mVersion == VERSION1) {
        FakeBuildingTilesMgr *btiles = &mFakeBuildingTilesMgr;
        tiles[Room::InteriorWall] = version1TileToEntry(btiles->catIWalls(),
                                                        tiles[Room::InteriorWall]);
        tiles[Room::Floor] = version1TileToEntry(btiles->catFloors(),
                                                 tiles[Room::Floor]);
    }

    Room *room = new Room();
    room->Name = name;
    room->internalName = internalName;
    QStringList rgb = color.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    room->Color = qRgb(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt());
    for (int i = 0; i < Room::TileCount; i++)
        room->setTile(i, getEntry(tiles[i]));

    xml.skipCurrentElement();

    return room;
}

BuildingFloor *BuildingReaderPrivate::readFloor()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("floor"));

    BuildingFloor *floor = new BuildingFloor(mBuilding, mBuilding->floorCount());

    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("object")) {
            if (BuildingObject *object = readObject(floor))
                floor->insertObject(floor->objectCount(), object);
        } else if (xml.name() == QLatin1String("rooms")) {
            while (xml.readNext() != QXmlStreamReader::Invalid) {
                if (xml.isEndElement())
                    break;
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    decodeCSVFloorData(floor, xml.text().toString());
                }
            }
        } else if (xml.name() == QLatin1String("tiles")) {
            const QXmlStreamAttributes atts = xml.attributes();
            QString layerName = atts.value(QLatin1String("layer")).toString();
            if (layerName.isEmpty()) {
                xml.raiseError(tr("Empty/missing layer name for <tiles> on floor %1")
                               .arg(floor->level()));
                delete floor;
                return nullptr;
            }
            if (mVersion == VERSION2) {
                QMap<QString, QString> renameLookup;
                renameLookup[QLatin1String("Curtains2")] = QLatin1String("Curtains3");
                renameLookup[QLatin1String("Doors")] = QLatin1String("Door");
                renameLookup[QLatin1String("Frames")] = QLatin1String("Frame");
                renameLookup[QLatin1String("Walls")] = QLatin1String("Wall");
                renameLookup[QLatin1String("Walls2")] = QLatin1String("Wall2");
                renameLookup[QLatin1String("Windows")] = QLatin1String("Window");
                if (renameLookup.contains(layerName)) {
                    layerName = renameLookup[layerName];
                }
            }
            while (xml.readNext() != QXmlStreamReader::Invalid) {
                if (xml.isEndElement())
                    break;
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    decodeCSVTileData(floor, layerName, xml.text().toString());
                }
            }
        } else
            readUnknownElement();
    }

    return floor;
}

BuildingObject *BuildingReaderPrivate::readObject(BuildingFloor *floor)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == QLatin1String("object"));

    const QXmlStreamAttributes atts = xml.attributes();
    const QString type = atts.value(QLatin1String("type")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const QString dirString = atts.value(QLatin1String("dir")).toString();
    QString tile = atts.value(QLatin1String("Tile")).toString();

    if (x < 0 || x >= mBuilding->width() + 1 || y < 0 || y >= mBuilding->height() + 1) {
        xml.raiseError(tr("Invalid object coordinates (%1,%2")
                       .arg(x).arg(y));
        return nullptr;
    }

    bool readDir = true;
    if (type == QLatin1String("furniture") ||
            type == QLatin1String("roof"))
        readDir = false;

    BuildingObject::Direction dir = BuildingObject::dirFromString(dirString);
    if (readDir && dir == BuildingObject::Direction::Invalid) {
        xml.raiseError(tr("Invalid object direction '%1'").arg(dirString));
        return nullptr;
    }

    FakeBuildingTilesMgr *btiles = &mFakeBuildingTilesMgr;
    if (mVersion == VERSION1) {
        // tile name -> BuildingTileEntry
        if (type == QLatin1String("door")) {
            tile = version1TileToEntry(btiles->catDoors(), tile);
        } else if (type == QLatin1String("stairs")) {
            tile = version1TileToEntry(btiles->catStairs(), tile);
        } else if (type == QLatin1String("window")) {
            // There was no curtains tile to worry about in v1
            tile = version1TileToEntry(btiles->catWindows(), tile);
        }
        // There was no RoofObject to worry about in v1
    }

    BuildingObject *object = nullptr;
    if (type == QLatin1String("door")) {
        Door *door = new Door(floor, x, y, dir);
        door->setTile(getEntry(tile)->asDoor());
        QString frame = atts.value(QLatin1String("FrameTile")).toString();
        if (mVersion == VERSION1)
            frame = version1TileToEntry(btiles->catDoorFrames(), frame);
        door->setTile(getEntry(frame)->asDoorFrame(), 1);
        object = door;
    } else if (type == QLatin1String("stairs")) {
        object = new Stairs(floor, x, y, dir);
        object->setTile(getEntry(tile)->asStairs());
    } else if (type == QLatin1String("window")) {
        object = new Window(floor, x, y, dir);
        object->setTile(getEntry(tile)->asWindow());
        const QString curtains = atts.value(QLatin1String("CurtainsTile")).toString();
        object->setTile(getEntry(curtains)->asCurtains(), Window::TileCurtains);
        const QString shutters = atts.value(QLatin1String("ShuttersTile")).toString();
        object->setTile(getEntry(shutters)->asShutters(), Window::TileShutters);
    } else if (type == QLatin1String("furniture")) {
        FurnitureObject *furniture = new FurnitureObject(floor, x, y);
        int index = atts.value(QLatin1String("FurnitureTiles")).toString().toInt();
        if (index < 0 || index >= mFurnitureTiles.count()) {
            xml.raiseError(tr("Furniture index %1 out of range").arg(index));
            delete furniture;
            return nullptr;
        }
        QString orientString = atts.value(QLatin1String("orient")).toString();
        FurnitureTile::FurnitureOrientation orient =
                FurnitureGroups::orientFromString(orientString);
        if (orient == FurnitureTile::FurnitureUnknown) {
            xml.raiseError(tr("Unknown furniture orientation '%1'").arg(orientString));
            delete furniture;
            return nullptr;
        }
        furniture->setFurnitureTile(mFurnitureTiles.at(index)->tile(orient));
        object = furniture;
    } else if (type == QLatin1String("roof")) {
        int width = atts.value(QLatin1String("width")).toString().toInt();
        int height = atts.value(QLatin1String("height")).toString().toInt();
        QString typeString = atts.value(QLatin1String("RoofType")).toString();
        RoofObject::RoofType roofType = RoofObject::typeFromString(typeString);
        if (roofType == RoofObject::InvalidType) {
            xml.raiseError(tr("Invalid roof type '%1'").arg(typeString));
            return nullptr;
        }

        QString depthString = atts.value(QLatin1String("Depth")).toString();
        RoofObject::RoofDepth depth = RoofObject::depthFromString(depthString);
        if (depth == RoofObject::InvalidDepth) {
            xml.raiseError(tr("Invalid roof depth '%1'").arg(depthString));
            return nullptr;
        }

        QString capTilesString = atts.value(QLatin1String("CapTiles")).toString();
        BuildingTileEntry *capTiles = getEntry(capTilesString)->asRoofCap();

        QString slopeTileString = atts.value(QLatin1String("SlopeTiles")).toString();
        BuildingTileEntry *slopeTiles = getEntry(slopeTileString)->asRoofSlope();

        QString topTileString = atts.value(QLatin1String("TopTiles")).toString();
        BuildingTileEntry *topTiles = getEntry(topTileString)->asRoofTop();

        bool cappedW, cappedN, cappedE, cappedS;
        if (!booleanFromString(atts.value(QLatin1String("cappedW")).toString(), cappedW))
            return nullptr;
        if (!booleanFromString(atts.value(QLatin1String("cappedN")).toString(), cappedN))
            return nullptr;
        if (!booleanFromString(atts.value(QLatin1String("cappedE")).toString(), cappedE))
            return nullptr;
        if (!booleanFromString(atts.value(QLatin1String("cappedS")).toString(), cappedS))
            return nullptr;
        RoofObject *roof = new RoofObject(floor, x, y, width, height,
                                          roofType, depth,
                                          cappedW, cappedN, cappedE, cappedS);
        roof->setCapTiles(capTiles);
        roof->setSlopeTiles(slopeTiles);
        roof->setTopTiles(topTiles);
        object = roof;
    } else if (type == QLatin1String("wall")) {
        int length = atts.value(QLatin1String("length")).toString().toInt();

        if (mVersion < VERSION3) {
            dir = (dir == BuildingObject::Direction::N) ? BuildingObject::Direction::W : BuildingObject::Direction::N;
        }

        WallObject *wall = new WallObject(floor, x, y, dir, length);

        BuildingTileEntry *entry = getEntry(tile);
        if (!entry->asExteriorWall())
            entry = mFakeBuildingTilesMgr.noneTileEntry();
        wall->setTile(entry);

        QString interiorTileString = atts.value(QLatin1String("InteriorTile")).toString();
        entry = getEntry(interiorTileString);
        if (!entry->asInteriorWall())
            entry = mFakeBuildingTilesMgr.noneTileEntry();
        wall->setTile(entry, WallObject::TileInterior);

        QString exteriorTrimString = atts.value(QLatin1String("ExteriorTrim")).toString();
        entry = getEntry(exteriorTrimString);
        if (!entry->asExteriorWallTrim())
            entry = mFakeBuildingTilesMgr.noneTileEntry();
        wall->setTile(entry, WallObject::TileExteriorTrim);

        QString interiorTrimString = atts.value(QLatin1String("InteriorTrim")).toString();
        entry = getEntry(interiorTrimString);
        if (!entry->asInteriorWallTrim())
            entry = mFakeBuildingTilesMgr.noneTileEntry();
        wall->setTile(entry, WallObject::TileInteriorTrim);

        object = wall;
    } else {
        xml.raiseError(tr("Unknown object type '%1'").arg(type));
        return nullptr;
    }

    xml.skipCurrentElement();

    return object;
}

bool BuildingReaderPrivate::booleanFromString(const QString &s, bool &result)
{
    if (s == QLatin1String("true")) {
        result = true;
        return true;
    }
    if (s == QLatin1String("false")) {
        result = false;
        return true;
    }
    xml.raiseError(tr("Expected boolean but got '%1'").arg(s));
    return false;
}

bool BuildingReaderPrivate::readPoint(const QString &name, QPoint &result)
{
    const QXmlStreamAttributes atts = xml.attributes();
    QString s = atts.value(name).toString();
    if (s.isEmpty()) {
        result = QPoint();
        return true;
    }
    QStringList split = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (split.size() != 2) {
        xml.raiseError(tr("expected point, got '%1'").arg(s));
        return false;
    }
    result.setX(split[0].toInt());
    result.setY(split[1].toInt());
    return true;
}

BuildingTileEntry *BuildingReaderPrivate::getEntry(const QString &s)
{
    int index = s.toInt();
    if (index >= 1 && index <= mEntries.size())
        return mEntries[index - 1];
    return mFakeBuildingTilesMgr.noneTileEntry();
}

QString BuildingReaderPrivate::version1TileToEntry(BuildingTileCategory *category,
                                                   const QString &tileName)
{
    QString key = category->name() + tileName;
    if (mEntryMap.contains(key))
        return QString::number(mEntries.indexOf(mEntryMap[key]) + 1);
    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
    mFakeBuildingTilesMgr.mUsedCategories[category] = true;

    if (BuildingTileEntry *match = category->findMatch(entry)) {
        delete entry;
        entry = match;
    }

    mEntryMap[key] = entry;
    mEntries += entry;
    return QString::number(mEntries.indexOf(entry) + 1);
}

FurnitureTiles *BuildingReaderPrivate::getFurniture(const QString &s)
{
    int index = s.toInt();
    if (index >= 0 && index < mFurnitureTiles.size())
        return mFurnitureTiles[index];
    return nullptr;
}

void BuildingReaderPrivate::decodeCSVFloorData(BuildingFloor *floor,
                                               const QString &text)
{
    int start = 0;
    int end = text.length();
    while (start < end && text.at(start).isSpace())
        start++;
    int x = 0, y = 0;
    const QChar sep(QLatin1Char(','));
    const QChar nullChar(QLatin1Char('0'));
    while ((end = text.indexOf(sep, start, Qt::CaseSensitive)) != -1) {
        if (end - start == 1 && text.at(start) == nullChar) {
            floor->SetRoomAt(x, y, nullptr);
        } else {
            bool conversionOk;
            uint index = text.midRef(start, end - start).toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse room at (%1,%2) on floor %3")
                               .arg(x + 1).arg(y + 1).arg(floor->level()));
                return;
            }
            floor->SetRoomAt(x, y, getRoom(floor, x, y, index));
        }
        start = end + 1;
        if (++x == floor->width()) {
            ++y;
            if (y >= floor->height()) {
                xml.raiseError(tr("Corrupt <rooms> for floor %1")
                               .arg(floor->level()));
                return;
            }
            x = 0;
        }
    }
    end = text.size();
    while (start < end && text.at(end-1).isSpace())
        end--;
    if (end - start == 1 && text.at(start) == nullChar) {
        floor->SetRoomAt(x, y, nullptr);
    } else {
        bool conversionOk;
        uint index = text.midRef(start, end - start).toUInt(&conversionOk);
        if (!conversionOk) {
            xml.raiseError(
                    tr("Unable to parse room at (%1,%2) on floor %3")
                           .arg(x + 1).arg(y + 1).arg(floor->level()));
            return;
        }
        floor->SetRoomAt(x, y, getRoom(floor, x, y, index));
    }
}

Room *BuildingReaderPrivate::getRoom(BuildingFloor *floor, int x, int y, int index)
{
    if (!index)
        return nullptr;
    if (index > 0 && index - 1 < mBuilding->roomCount())
        return mBuilding->room(index - 1);
    xml.raiseError(tr("Invalid room index at (%1,%2) on floor %3")
                   .arg(x).arg(y).arg(floor->level()));
    return nullptr;
}

void BuildingReaderPrivate::decodeCSVTileData(BuildingFloor *floor,
                                              const QString &layerName,
                                              const QString &text)
{
    int start = 0;
    int end = text.length();
    while (start < end && text.at(start).isSpace())
        start++;
    int x = 0, y = 0;
    const QChar sep(QLatin1Char(','));
    const QChar nullChar(QLatin1Char('0'));
    while ((end = text.indexOf(sep, start, Qt::CaseSensitive)) != -1) {
        if (end - start == 1 && text.at(start) == nullChar) {
            ; //floor->setGrime(layerName, x, y, QString());
        } else {
            bool conversionOk;
            uint index = text.midRef(start, end - start).toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse user-tile at (%1,%2) on floor %3")
                               .arg(x + 1).arg(y + 1).arg(floor->level()));
                return;
            }
            floor->setGrime(layerName, x, y, getUserTile(floor, x, y, index));
        }
        start = end + 1;
        if (++x == floor->width() + 1) {
            ++y;
            if (y >= floor->height() + 1) {
                xml.raiseError(tr("Corrupt <tiles> for floor %1")
                               .arg(floor->level()));
                return;
            }
            x = 0;
        }
    }
    end = text.size();
    while (start < end && text.at(end-1).isSpace())
        end--;
    if (end - start == 1 && text.at(start) == nullChar) {
        ; //floor->setGrime(layerName, x, y, QString());
    } else {
        bool conversionOk;
        uint index = text.midRef(start, end - start).toUInt(&conversionOk);
        if (!conversionOk) {
            xml.raiseError(
                    tr("Unable to parse user-tile at (%1,%2) on floor %3")
                           .arg(x + 1).arg(y + 1).arg(floor->level()));
            return;
        }
        floor->setGrime(layerName, x, y, getUserTile(floor, x, y, index));
    }
}

BuildingCell BuildingReaderPrivate::getUserTile(BuildingFloor *floor, int x, int y, int index)
{
    if (!index)
        return BuildingCell();

    Tiled::MapRotation rotation = Tiled::MapRotation::NotRotated;
    if (mVersion >= VERSION3) {
        if (uint(index) & RotateFlag90)
            rotation = Tiled::MapRotation::Clockwise90;
        else if (uint(index) & RotateFlag180)
            rotation = Tiled::MapRotation::Clockwise180;
        else if (uint(index) & RotateFlag270)
            rotation = Tiled::MapRotation::Clockwise270;
        index &= ~(RotateFlag90 | RotateFlag180 | RotateFlag270);
    }

    if (index > 0 && index - 1 < mUserTiles.size()) {
        return BuildingCell(mUserTiles.at(index - 1), rotation);
    }
    xml.raiseError(tr("Invalid tile index at (%1,%2) on floor %3")
                   .arg(x).arg(y).arg(floor->level()));
    return BuildingCell();
}

void BuildingReaderPrivate::readUnknownElement()
{
    qDebug() << "Unknown element (fixme):" << xml.name();
    xml.skipCurrentElement();
}

/////

BuildingReader::BuildingReader()
    : d(new BuildingReaderPrivate(this))
{
}

BuildingReader::~BuildingReader()
{
    delete d;
}

Building *BuildingReader::read(QIODevice *device, const QString &path)
{
    return d->readBuilding(device, path);
}

Building *BuildingReader::read(const QString &fileName)
{
    QtLockedFile file(fileName);
    if (!d->openFile(&file))
        return nullptr;

    return read(&file, QFileInfo(fileName).absolutePath());
}

QString BuildingReader::errorString() const
{
    return d->errorString();
}

void BuildingReader::fix(Building *building)
{
    d->fix(building);
}

void BuildingReaderPrivate::fix(Building *building)
{
//    BuildingTileEntry *entry = BuildingTilesMgr::instance()->noneTileEntry();
//    fixedEntries[entry] = entry;

    foreach (BuildingFloor *floor, building->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (FurnitureObject *fo = object->asFurniture()) {
                if (FurnitureTile *ftile = fo->furnitureTile()) {
                    FurnitureTiles *ftiles = ftile->owner();
                    fo->setFurnitureTile(fixFurniture(ftiles)->tile(ftile->orient()));
                }
            } else {
                for (int i = 0; i < object->tiles().size(); i++) {
                    BuildingTileEntry *entry = object->tile(i);
                    if (entry && !entry->isNone()) {
                        object->setTile(fixEntry(entry), i);
                    }
                }
            }
        }
    }

    for (int i = 0; i < building->TileCount; i++) {
        BuildingTileEntry *entry = building->tile(i);
        if (entry && !entry->isNone())
            building->setTile(i, fixEntry(entry));
    }
    foreach (Room *room, building->rooms()) {
        for (int i = 0; i < room->TileCount; i++) {
            BuildingTileEntry *entry = room->tile(i);
            if (entry && !entry->isNone())
                room->setTile(i, fixEntry(entry));
        }
    }

    QList<FurnitureTiles*> usedFurniture;
    foreach (FurnitureTiles *ftiles, building->usedFurniture())
        usedFurniture += fixFurniture(ftiles);
    building->setUsedFurniture(usedFurniture);

    QList<BuildingTileEntry*> usedTiles;
    foreach (BuildingTileEntry *entry, building->usedTiles()) {
        if (entry && !entry->isNone())
            usedTiles += fixEntry(entry);
    }
    building->setUsedTiles(usedTiles);

    qDeleteAll(deadEntries);
    qDeleteAll(deadFurniture);
    qDeleteAll(deadCategories);
    qDeleteAll(deadTiles);
}

FurnitureTiles *BuildingReaderPrivate::fixFurniture(FurnitureTiles *ftiles)
{
    if (!fixedFurniture.contains(ftiles)) {
        foreach (FurnitureTile *ftile, ftiles->tiles()) {
            for (int y = 0; y < ftile->size().height(); y++) {
                for (int x = 0; x < ftile->size().width(); x++) {
                    if (BuildingTile *btile = ftile->tile(x, y)) {
                        if (btile != BuildingTilesMgr::instance()->noneTile())
                            deadTiles.insert(btile);
                        ftile->setTile(x, y, BuildingTilesMgr::instance()->get(btile->name()));
                    }
                }
            }
        }
        if (FurnitureTiles *match = FurnitureGroups::instance()->findMatch(ftiles)) {
            fixedFurniture[ftiles] = match;
            fixedFurniture[match] = match;
            deadFurniture.insert(ftiles);
        } else {
            fixedFurniture[ftiles] = ftiles;
        }
    }
    return fixedFurniture[ftiles];
}

BuildingTileEntry *BuildingReaderPrivate::fixEntry(BuildingTileEntry *entry)
{
    if (!entry || entry->isNone())
        return entry;

    if (!fixedEntries.contains(entry)) {
        Q_ASSERT(BuildingTilesMgr::instance()->indexOf(entry->mCategory) == -1);
        QString categoryName = entry->category()->name();
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(categoryName);
        deadCategories.insert(entry->mCategory);
        entry->mCategory = category;
        for (int i = 0; i < category->enumCount(); i++) {
            if (BuildingTile *btile = entry->tile(i)) {
                if (btile != BuildingTilesMgr::instance()->noneTile())
                    deadTiles.insert(btile);
                entry->setTile(i, BuildingTilesMgr::instance()->get(btile->name()));
            }
        }
        if (BuildingTileEntry *match = category->findMatch(entry)) {
            fixedEntries[entry] = match;
            fixedEntries[match] = match;
            deadEntries.insert(entry);
        } else {
            fixedEntries[entry] = entry;
        }
    }
    return fixedEntries[entry];
}
