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

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QXmlStreamReader>

using namespace BuildingEditor;

// version="1.0"
#define VERSION1 1

// version="2"
// BuildingTileEntry rewrite
// added FurnitureTiles::mCorners
#define VERSION2 2

#define VERSION_LATEST VERSION2

class BuildingEditor::BuildingReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(BuildingReaderPrivate)

public:
    BuildingReaderPrivate(BuildingReader *reader):
        p(reader),
        mBuilding(0)
    {}

    Building *readBuilding(QIODevice *device, const QString &path);

    bool openFile(QFile *file);

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
    QString getUserTile(BuildingFloor *floor, int x, int y, int index);

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

    QXmlStreamReader xml;
};

bool BuildingReaderPrivate::openFile(QFile *file)
{
    if (!file->exists()) {
        mError = tr("File not found: %1").arg(file->fileName());
        return false;
    } else if (!file->open(QFile::ReadOnly | QFile::Text)) {
        mError = tr("Unable to read file: %1").arg(file->fileName());
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
    Building *building = 0;

    xml.setDevice(device);

    if (xml.readNextStartElement() && xml.name() == "building") {
        building = readBuilding();
    } else {
        xml.raiseError(tr("Not a building file."));
    }

    return building;
}

Building *BuildingReaderPrivate::readBuilding()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "building");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString versionString = atts.value(QLatin1String("version")).toString();
    if (versionString == QLatin1String("1.0"))
        mVersion = VERSION1;
    else {
        mVersion = versionString.toInt();
        if (mVersion <= 0 || mVersion > VERSION_LATEST) {
            xml.raiseError(tr("Unknown building version '%1'").arg(versionString));
            return 0;
        }
    }
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    mBuilding = new Building(width, height);

    while (xml.readNextStartElement()) {
        if (xml.name() == "furniture") {
            if (FurnitureTiles *tiles = readFurnitureTiles())
                mFurnitureTiles += tiles;
        } else if (xml.name() == "tile_entry") {
            if (BuildingTileEntry *entry = readTileEntry())
                mEntries += entry;
        } else if (xml.name() == "user_tiles") {
            readUserTiles();
        } else if (xml.name() == "used_tiles") {
            mBuilding->setUsedTiles(readUsedTiles());
        } else if (xml.name() == "used_furniture") {
            mBuilding->setUsedFurniture(readUsedFurniture());
        } else if (xml.name() == "room") {
            if (Room *room = readRoom())
                mBuilding->insertRoom(mBuilding->roomCount(), room);
        } else if (xml.name() == "floor") {
            if (BuildingFloor *floor = readFloor())
                mBuilding->insertFloor(mBuilding->floorCount(), floor);
        } else
            readUnknownElement();
    }

    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
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
        mBuilding = 0;
    }

    return mBuilding;
}

FurnitureTiles *BuildingReaderPrivate::readFurnitureTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "furniture");

    const QXmlStreamAttributes atts = xml.attributes();
    bool corners = false;
    FurnitureTiles::FurnitureLayer layer = FurnitureTiles::LayerFurniture;
    if (mVersion >= VERSION2) {
        QString cornersString = atts.value(QLatin1String("corners")).toString();
        if (cornersString.length() && !booleanFromString(cornersString, corners))
            return 0;

        QString layerString = atts.value(QLatin1String("layer")).toString();
        if (layerString.length()) {
            layer = FurnitureTiles::layerFromString(layerString);
            if (layer == FurnitureTiles::InvalidLayer) {
                xml.raiseError(tr("Unknown furniture layer '%1'").arg(layerString));
                return 0;
            }
        }
    }

    FurnitureTiles *ftiles = new FurnitureTiles(corners);
    ftiles->setLayer(layer);

    while (xml.readNextStartElement()) {
        if (xml.name() == "entry") {
            if (FurnitureTile *ftile = readFurnitureTile(ftiles))
                ftiles->setTile(ftile);
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftiles;
        return 0;
    }

    if (FurnitureTiles *match = FurnitureGroups::instance()->findMatch(ftiles)) {
        delete ftiles;
        return match;
    }

    return ftiles;
}

FurnitureTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTiles *ftiles)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "entry");

    const QXmlStreamAttributes atts = xml.attributes();
    QString orientString = atts.value(QLatin1String("orient")).toString();
    FurnitureTile::FurnitureOrientation orient =
            FurnitureGroups::orientFromString(orientString);
    if (orient == FurnitureTile::FurnitureUnknown) {
        xml.raiseError(tr("invalid furniture tile orientation '%1'").arg(orientString));
        return 0;
    }

    FurnitureTile *ftile = new FurnitureTile(ftiles, orient);

    if (mVersion == VERSION1) {
        // v1 didn't have FurnitureTiles::mCorners, it had either W/N/E/S
        // or SW/NW/NE/SE.
        if (FurnitureTile::isCornerOrient(orient) && !ftiles->hasCorners())
            ftiles->toggleCorners();
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == "tile") {
            QPoint pos;
            if (BuildingTile *btile = readFurnitureTile(ftile, pos))
                ftile->setTile(pos.x(), pos.y(), btile);
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftile;
        return 0;
    }

    return ftile;
}

BuildingTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTile *ftile, QPoint &pos)
{
    Q_UNUSED(ftile)
    Q_ASSERT(xml.isStartElement() && xml.name() == "tile");
    const QXmlStreamAttributes atts = xml.attributes();
    int x = atts.value(QLatin1String("x")).toString().toInt();
    int y = atts.value(QLatin1String("y")).toString().toInt();
    if (x < 0 || y < 0) {
        xml.raiseError(tr("invalid furniture tile coordinates (%1,%2)")
                       .arg(x).arg(y));
    }
    pos.setX(x);
    pos.setY(y);
    QString tileName = atts.value(QLatin1String("name")).toString();
    BuildingTile *btile = BuildingTilesMgr::instance()->get(tileName);
    xml.skipCurrentElement();
    return btile;
}

BuildingTileEntry *BuildingReaderPrivate::readTileEntry()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "tile_entry");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString categoryName = atts.value(QLatin1String("category")).toString();
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(categoryName);
    if (!category) {
        xml.raiseError(tr("unknown category '%1'").arg(categoryName));
        return 0;
    }

    BuildingTileEntry *entry = new BuildingTileEntry(category);

    while (xml.readNextStartElement()) {
        if (xml.name() == "tile") {
            const QXmlStreamAttributes atts = xml.attributes();
            const QString enumName = atts.value(QLatin1String("enum")).toString();
            int e = category->enumFromString(enumName);
            if (e == BuildingTileCategory::Invalid) {
                xml.raiseError(tr("Unknown %1 enum '%2'").arg(categoryName).arg(enumName));
                return false;
            }
            const QString tileName = atts.value(QLatin1String("tile")).toString();
            BuildingTile *btile = BuildingTilesMgr::instance()->get(tileName);

            QPoint offset;
            if (!readPoint(QLatin1String("offset"), offset))
                return false;

            entry->mTiles[e] = btile;
            entry->mOffsets[e] = offset;
            xml.skipCurrentElement();
        } else
            readUnknownElement();
    }

    if (BuildingTileEntry *match = category->findMatch(entry)) {
        delete entry;
        return match;
    }

    return entry;
}

void BuildingReaderPrivate::readUserTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "user_tiles");

    while (xml.readNextStartElement()) {
        if (xml.name() == "tile") {
            const QXmlStreamAttributes atts = xml.attributes();
            const QString tileName = atts.value(QLatin1String("tile")).toString();
            QString tilesetName;
            int tileID;
            if (tileName.isEmpty() || !BuildingTilesMgr::instance()->
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
    Q_ASSERT(xml.isStartElement() && xml.name() == "used_tiles");

    QList<BuildingTileEntry *> result;

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement())
            break;
        if (xml.isCharacters() && !xml.isWhitespace()) {
            QStringList used = xml.text().toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
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
    Q_ASSERT(xml.isStartElement() && xml.name() == "used_furniture");

    QList<FurnitureTiles *> result;

    while (xml.readNext() != QXmlStreamReader::Invalid) {
        if (xml.isEndElement())
            break;
        if (xml.isCharacters() && !xml.isWhitespace()) {
            QStringList used = xml.text().toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
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
    Q_ASSERT(xml.isStartElement() && xml.name() == "room");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("Name")).toString();
    const QString internalName = atts.value(QLatin1String("InternalName")).toString();
    const QString color = atts.value(QLatin1String("Color")).toString();
    QMap<int,QString> tiles;
    for (int i = 0; i < Room::TileCount; i++)
        tiles[i] = atts.value(Room::enumToString(i)).toString();

    if (mVersion == VERSION1) {
        BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
        tiles[Room::InteriorWall] = version1TileToEntry(btiles->catIWalls(),
                                                        tiles[Room::InteriorWall]);
        tiles[Room::Floor] = version1TileToEntry(btiles->catFloors(),
                                                 tiles[Room::Floor]);
    }

    Room *room = new Room();
    room->Name = name;
    room->internalName = internalName;
    QStringList rgb = color.split(QLatin1Char(' '), QString::SkipEmptyParts);
    room->Color = qRgb(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt());
    for (int i = 0; i < Room::TileCount; i++)
        room->setTile(i, getEntry(tiles[i]));

    xml.skipCurrentElement();

    return room;
}

BuildingFloor *BuildingReaderPrivate::readFloor()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "floor");

    BuildingFloor *floor = new BuildingFloor(mBuilding, mBuilding->floorCount());

    while (xml.readNextStartElement()) {
        if (xml.name() == "object") {
            if (BuildingObject *object = readObject(floor))
                floor->insertObject(floor->objectCount(), object);
        } else if (xml.name() == "rooms") {
            while (xml.readNext() != QXmlStreamReader::Invalid) {
                if (xml.isEndElement())
                    break;
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    decodeCSVFloorData(floor, xml.text().toString());
                }
            }
        } else if (xml.name() == "tiles") {
            const QXmlStreamAttributes atts = xml.attributes();
            const QString layerName = atts.value(QLatin1String("layer")).toString();
            if (layerName.isEmpty()) {
                xml.raiseError(tr("Empty/missing layer name for <tiles> on floor %1")
                               .arg(floor->level()));
                delete floor;
                return 0;
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
    Q_ASSERT(xml.isStartElement() && xml.name() == "object");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString type = atts.value(QLatin1String("type")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const QString dirString = atts.value(QLatin1String("dir")).toString();
    QString tile = atts.value(QLatin1String("Tile")).toString();

    if (x < 0 || x >= mBuilding->width() + 1 || y < 0 || y >= mBuilding->height() + 1) {
        xml.raiseError(tr("Invalid object coordinates (%1,%2")
                       .arg(x).arg(y));
        return 0;
    }

    bool readDir = true;
    if (type == QLatin1String("furniture") ||
            type == QLatin1String("roof"))
        readDir = false;

    BuildingObject::Direction dir = BuildingObject::dirFromString(dirString);
    if (readDir && dir == BuildingObject::Invalid) {
        xml.raiseError(tr("Invalid object direction '%1'").arg(dirString));
        return 0;
    }

    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
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

    BuildingObject *object = 0;
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
        object->setTile(getEntry(curtains)->asCurtains(), 1);
    } else if (type == QLatin1String("furniture")) {
        FurnitureObject *furniture = new FurnitureObject(floor, x, y);
        int index = atts.value(QLatin1String("FurnitureTiles")).toString().toInt();
        if (index < 0 || index >= mFurnitureTiles.count()) {
            xml.raiseError(tr("Furniture index %1 out of range").arg(index));
            delete furniture;
            return 0;
        }
        QString orientString = atts.value(QLatin1String("orient")).toString();
        FurnitureTile::FurnitureOrientation orient =
                FurnitureGroups::orientFromString(orientString);
        if (orient == FurnitureTile::FurnitureUnknown) {
            xml.raiseError(tr("Unknown furniture orientation '%1'").arg(orientString));
            delete furniture;
            return 0;
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
            return 0;
        }

        QString depthString = atts.value(QLatin1String("Depth")).toString();
        RoofObject::RoofDepth depth = RoofObject::depthFromString(depthString);
        if (depth == RoofObject::InvalidDepth) {
            xml.raiseError(tr("Invalid roof depth '%1'").arg(depthString));
            return 0;
        }

        QString capTilesString = atts.value(QLatin1String("CapTiles")).toString();
        BuildingTileEntry *capTiles = getEntry(capTilesString)->asRoofCap();

        QString slopeTileString = atts.value(QLatin1String("SlopeTiles")).toString();
        BuildingTileEntry *slopeTiles = getEntry(slopeTileString)->asRoofSlope();

        QString topTileString = atts.value(QLatin1String("TopTiles")).toString();
        BuildingTileEntry *topTiles = getEntry(topTileString)->asRoofTop();

        bool cappedW, cappedN, cappedE, cappedS;
        if (!booleanFromString(atts.value(QLatin1String("cappedW")).toString(), cappedW))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedN")).toString(), cappedN))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedE")).toString(), cappedE))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedS")).toString(), cappedS))
            return 0;
        RoofObject *roof = new RoofObject(floor, x, y, width, height,
                                          roofType, depth,
                                          cappedW, cappedN, cappedE, cappedS);
        roof->setCapTiles(capTiles);
        roof->setSlopeTiles(slopeTiles);
        roof->setTopTiles(topTiles);
        object = roof;
    } else if (type == QLatin1String("wall")) {
        int length = atts.value(QLatin1String("length")).toString().toInt();
        WallObject *wall = new WallObject(floor, x, y, dir, length);

        BuildingTileEntry *entry = getEntry(tile);
        if (!entry->asExteriorWall())
            entry = BuildingTilesMgr::instance()->noneTileEntry();
        wall->setTile(entry);

        QString interiorTileString = atts.value(QLatin1String("InteriorTile")).toString();
        entry = getEntry(interiorTileString);
        if (!entry->asInteriorWall())
            entry = BuildingTilesMgr::instance()->noneTileEntry();
        wall->setTile(entry, 1);
        object = wall;
    } else {
        xml.raiseError(tr("Unknown object type '%1'").arg(type));
        return 0;
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
    QStringList split = s.split(QLatin1Char(','), QString::SkipEmptyParts);
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
    return BuildingTilesMgr::instance()->noneTileEntry();
}

QString BuildingReaderPrivate::version1TileToEntry(BuildingTileCategory *category,
                                                   const QString &tileName)
{
    QString key = category->name() + tileName;
    if (mEntryMap.contains(key))
        return QString::number(mEntries.indexOf(mEntryMap[key]) + 1);
    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);

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
    return 0;
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
            floor->SetRoomAt(x, y, 0);
        } else {
            bool conversionOk;
            uint index = text.mid(start, end - start).toUInt(&conversionOk);
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
        floor->SetRoomAt(x, y, 0);
    } else {
        bool conversionOk;
        uint index = text.mid(start, end - start).toUInt(&conversionOk);
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
        return 0;
    if (index > 0 && index - 1 < mBuilding->roomCount())
        return mBuilding->room(index - 1);
    xml.raiseError(tr("Invalid room index at (%1,%2) on floor %3")
                   .arg(x).arg(y).arg(floor->level()));
    return 0;
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
            uint index = text.mid(start, end - start).toUInt(&conversionOk);
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
        uint index = text.mid(start, end - start).toUInt(&conversionOk);
        if (!conversionOk) {
            xml.raiseError(
                    tr("Unable to parse user-tile at (%1,%2) on floor %3")
                           .arg(x + 1).arg(y + 1).arg(floor->level()));
            return;
        }
        floor->setGrime(layerName, x, y, getUserTile(floor, x, y, index));
    }
}

QString BuildingReaderPrivate::getUserTile(BuildingFloor *floor, int x, int y, int index)
{
    if (!index)
        return QString();
    if (index > 0 && index - 1 < mUserTiles.size())
        return mUserTiles.at(index - 1);
    xml.raiseError(tr("Invalid tile index at (%1,%2) on floor %3")
                   .arg(x).arg(y).arg(floor->level()));
    return 0;
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
    QFile file(fileName);
    if (!d->openFile(&file))
        return 0;

    return read(&file, QFileInfo(fileName).absolutePath());
}

QString BuildingReader::errorString() const
{
    return d->errorString();
}
