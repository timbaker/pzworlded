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

#include "buildingtemplates.h"

#include "preferences.h"

#include "buildingtiles.h"
#include "furnituregroups.h"
#include "simplefile.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>

using namespace BuildingEditor;

static const char *TXT_FILE = "BuildingTemplates.txt";

/////

BuildingTemplates *BuildingTemplates::mInstance = 0;

BuildingTemplates *BuildingTemplates::instance()
{
    if (!mInstance)
        mInstance = new BuildingTemplates;
    return mInstance;
}

void BuildingTemplates::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTemplates::BuildingTemplates()
{
}

BuildingTemplates::~BuildingTemplates()
{
    qDeleteAll(mTemplates);
}

void BuildingTemplates::addTemplate(BuildingTemplate *btemplate)
{
    mTemplates += btemplate;
}

void BuildingTemplates::replaceTemplates(const QList<BuildingTemplate *> &templates)
{
    qDeleteAll(mTemplates);
    mTemplates.clear();
    foreach (BuildingTemplate *btemplate, templates)
        mTemplates += new BuildingTemplate(btemplate);
}

QString BuildingTemplates::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTemplates::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

// this code is almost the same as BuildingTilesMgr::writeTileEntry
static void writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry)
{
    BuildingTileCategory *category = entry->category();
    SimpleFileBlock block;
    block.name = QLatin1String("TileEntry");
    block.addValue("category", category->name());
    for (int i = 0; i < category->enumCount(); i++) {
        block.addValue(category->enumToString(i), entry->tile(i)->name());
    }
    for (int i = 0; i < category->enumCount(); i++) {
        QPoint p = entry->offset(i);
        if (p.isNull())
            continue;
        block.addValue("offset", QString(QLatin1String("%1 %2 %3"))
                       .arg(category->enumToString(i)).arg(p.x()).arg(p.y()));
    }
    parentBlock.blocks += block;
}

// this code is almost the same as BuildingTilesMgr::readTileEntry
static BuildingTileEntry *readTileEntry(SimpleFileBlock &block, QString &error)
{
    QString categoryName = block.value("category");
    if (BuildingTileCategory *category = BuildingTilesMgr::instance()->category(categoryName)) {
        BuildingTileEntry *entry = new BuildingTileEntry(category);
        foreach (SimpleFileKeyValue kv, block.values) {
            if (kv.name == QLatin1String("category"))
                continue;
            if (kv.name == QLatin1String("offset")) {
                QStringList split = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                if (split.size() != 3) {
                    error = BuildingTilesMgr::instance()->tr("Expected 'offset = name x y', got '%1'").arg(kv.value);
                    delete entry;
                    return false;
                }
                int e = category->enumFromString(split[0]);
                if (e == BuildingTileCategory::Invalid) {
                    error = BuildingTilesMgr::instance()->tr("Unknown %1 enum '%2'")
                            .arg(categoryName).arg(split[0]);
                    delete entry;
                    return 0;
                }
                entry->mOffsets[e] = QPoint(split[1].toInt(), split[2].toInt());
                continue;
            }
            int e = category->enumFromString(kv.name);
            if (e == BuildingTileCategory::Invalid) {
                error = BuildingTilesMgr::instance()->tr("Unknown %1 enum %2")
                        .arg(categoryName).arg(kv.name);
                return 0;
            }
            entry->mTiles[e] = BuildingTilesMgr::instance()->get(kv.value);
        }

        if (BuildingTileEntry *match = category->findMatch(entry)) {
            delete entry;
            entry = match;
        }

        return entry;
    }
    error = BuildingTilesMgr::instance()->tr("Unknown tile category '%1'")
            .arg(categoryName);
    return 0;
}

static FurnitureTiles *readFurnitureTiles(SimpleFileBlock &block, QString &error)
{
    extern FurnitureTiles *furnitureTilesFromSFB(SimpleFileBlock &block, QString &error);
    if (FurnitureTiles *result = furnitureTilesFromSFB(block, error)) {
        FurnitureTiles *match = FurnitureGroups::instance()->findMatch(result);
        if (match) {
            delete result;
            result = match;
        }
        return result;
    }
    return 0;
}

static void writeFurnitureTiles(SimpleFileBlock &block, FurnitureTiles *ftiles)
{
    extern SimpleFileBlock furnitureTilesToSFB(FurnitureTiles *ftiles);
    block.blocks += furnitureTilesToSFB(ftiles);
}

#define VERSION0 0

// VERSION1
// Massive rewrite -> BuildingTileEntry
#define VERSION1 1

// VERSION2
// Renamed Room.Wall -> Room.InteriorWall
#define VERSION2 2
#define VERSION_LATEST VERSION2

bool BuildingTemplates::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }
#if 0
    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;
#endif
    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    mEntries.clear();
    mEntriesByCategoryName.clear();
    mFurnitureTiles.clear();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("TileEntry")) {
            if (BuildingTileEntry *entry = readTileEntry(block, mError))
                addEntry(entry, false);
            else
                return false;
            continue;
        }
        if (block.name == QLatin1String("furniture")) {
            if (FurnitureTiles *ftiles = readFurnitureTiles(block, mError))
                addFurniture(ftiles);
            else
                return false;
            continue;
        }
        if (block.name == QLatin1String("Template")) {
            BuildingTemplate *def = new BuildingTemplate;
            def->setName(block.value("Name"));
            for (int i = 0; i < BuildingTemplate::TileCount; i++)
                def->setTile(i, getEntry(block.value(def->enumToString(i))));

            QList<BuildingTileEntry*> usedTiles;
            foreach (QString s, block.value("UsedTiles").split(QLatin1Char(' '),
                                                               QString::SkipEmptyParts)) {
                if (BuildingTileEntry *entry = getEntry(s, false))
                    usedTiles += entry;
            }
            def->setUsedTiles(usedTiles);

            QList<FurnitureTiles*> usedFurniture;
            foreach (QString s, block.value("UsedFurniture").split(QLatin1Char(' '),
                                                                   QString::SkipEmptyParts)) {
                if (FurnitureTiles *ftiles = getFurnitureTiles(s))
                    usedFurniture += ftiles;
            }
            def->setUsedFurniture(usedFurniture);

            foreach (SimpleFileBlock roomBlock, block.blocks) {
                if (roomBlock.name == QLatin1String("Room")) {
                    Room *room = new Room;
                    room->Name = roomBlock.value("Name");
                    QStringList rgb = roomBlock.value("Color").split(QLatin1String(" "),
                                                                     QString::SkipEmptyParts);
                    room->Color = qRgb(rgb.at(0).toInt(),
                                       rgb.at(1).toInt(),
                                       rgb.at(2).toInt());
                    for (int i = 0; i < Room::TileCount; i++)
                        room->setTile(i, getEntry(roomBlock.value(room->enumToString(i))));
                    room->internalName = roomBlock.value("InternalName");
                    def->addRoom(room);
                } else {
                    mError = tr("Unknown block name '%1': expected 'Room'.\n%2")
                            .arg(roomBlock.name)
                            .arg(path);
                    return false;
                }
            }
            addTemplate(def);
        } else {
            mError = tr("Unknown block name '%1': expected 'Template'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

void BuildingTemplates::writeTxt(QWidget *parent)
{
    return;

    mEntries.clear();
    mEntriesByCategoryName.clear();
    mFurnitureTiles.clear();
    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        foreach (BuildingTileEntry *entry, btemplate->tiles())
            addEntry(entry);
        foreach (Room *room, btemplate->rooms()) {
            foreach (BuildingTileEntry *entry, room->tiles())
                addEntry(entry);
        }
        foreach (BuildingTileEntry *entry, btemplate->usedTiles())
            addEntry(entry);
        foreach (FurnitureTiles *ftiles, btemplate->usedFurniture())
            addFurniture(ftiles);
    }

    SimpleFile simpleFile;
    foreach (BuildingTileEntry *entry, mEntries)
        writeTileEntry(simpleFile, entry);
    foreach (FurnitureTiles *ftiles, mFurnitureTiles)
        writeFurnitureTiles(simpleFile, ftiles);

    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        SimpleFileBlock templateBlock;
        templateBlock.name = QLatin1String("Template");
        templateBlock.addValue("Name", btemplate->name());
        for (int i = 0; i < BuildingTemplate::TileCount; i++)
            templateBlock.addValue(btemplate->enumToString(i), entryIndex(btemplate->tile(i)));
        foreach (Room *room, btemplate->rooms()) {
            SimpleFileBlock roomBlock;
            roomBlock.name = QLatin1String("Room");
            roomBlock.addValue("Name", room->Name);
            QString colorString = QString(QLatin1String("%1 %2 %3"))
                    .arg(qRed(room->Color))
                    .arg(qGreen(room->Color))
                    .arg(qBlue(room->Color));
            roomBlock.addValue("Color", colorString);
            roomBlock.addValue("InternalName", room->internalName);
            for (int i = 0; i < Room::TileCount; i++)
                roomBlock.addValue(room->enumToString(i), entryIndex(room->tile(i)));
            templateBlock.blocks += roomBlock;
        }

        QStringList usedTiles;
        foreach (BuildingTileEntry *entry, btemplate->usedTiles())
            usedTiles += entryIndex(entry);
        templateBlock.addValue("UsedTiles", usedTiles.join(QLatin1String(" ")));

        QStringList usedFurniture;
        foreach (FurnitureTiles *ftiles, btemplate->usedFurniture())
            usedFurniture += furnitureIndex(ftiles);
        templateBlock.addValue("UsedFurniture", usedFurniture.join(QLatin1String(" ")));

        simpleFile.blocks += templateBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

QString BuildingTemplates::entryIndex(BuildingTileCategory *category,
                                      const QString &tileName)
{
    if (tileName.isEmpty())
        return QString::number(0);
    BuildingTileEntry *entry = mEntryMap[qMakePair(category,tileName)];
    return QString::number(mEntries.indexOf(entry) + 1);
}

void BuildingTemplates::addFurniture(FurnitureTiles *ftiles)
{
    if (!ftiles)
        return;
    if (!mFurnitureTiles.contains(ftiles))
        mFurnitureTiles += ftiles;
}

QString BuildingTemplates::furnitureIndex(FurnitureTiles *ftiles)
{
    int index = mFurnitureTiles.indexOf(ftiles);
    return QString::number(index + 1);
}

FurnitureTiles *BuildingTemplates::getFurnitureTiles(const QString &s)
{
    int index = s.toInt();
    if (index >= 1 && index <= mFurnitureTiles.size())
        return mFurnitureTiles[index - 1];
    return 0;
}

bool BuildingTemplates::upgradeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    int userVersion = userFile.version(); // may be zero for unversioned file
    if (userVersion == VERSION_LATEST)
        return true;

    if (userVersion > VERSION_LATEST) {
        mError = tr("%1 is from a newer version of TileZed").arg(txtName());
        return false;
    }

    // Not the latest version -> upgrade it.

    QString sourcePath = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + txtName();

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    if (userVersion < VERSION1) {
        // Massive rewrite -> BuildingTileEntry stuff

        // Step 1: read all the single-tile assignments and convert them to
        // BuildingTileEntry.
        mEntries.clear();
        mEntryMap.clear();
        mEntriesByCategoryName.clear();
        BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
        foreach (SimpleFileBlock block, userFile.blocks) {
            if (block.name == QLatin1String("Template")) {
                addEntry(btiles->catEWalls(), block.value("Wall"));
                addEntry(btiles->catDoors(), block.value("Door"));
                addEntry(btiles->catDoorFrames(), block.value("DoorFrame"));
                addEntry(btiles->catWindows(), block.value("Window"));
                addEntry(btiles->catCurtains(), block.value("Curtains"));
                addEntry(btiles->catStairs(), block.value("Stairs"));
                foreach (SimpleFileBlock roomBlock, block.blocks) {
                    if (roomBlock.name == QLatin1String("Room")) {
                        addEntry(btiles->category(QLatin1String("interior_walls")), roomBlock.value("Wall"));
                        addEntry(btiles->category(QLatin1String("floors")), roomBlock.value("Floor"));
                    }
                }
            }
        }

        // Step 3: add the TileEntry blocks
        SimpleFileBlock newFile;
        foreach (BuildingTileEntry *entry, mEntries) {
            SimpleFileBlock block;
            block.name = QLatin1String("TileEntry");
            block.addValue("category", entry->category()->name());
            for (int i = 0; i < entry->category()->enumCount(); i++)
                block.addValue(entry->category()->enumToString(i), entry->tile(i)->name());
            newFile.blocks += block;
        }

        // Step 3: replace tile names with entry indices
        foreach (SimpleFileBlock block, userFile.blocks) {
            SimpleFileBlock newBlock;
            newBlock.name = block.name;
            newBlock.values = block.values;
            if (block.name == QLatin1String("Template")) {
                newBlock.replaceValue("Wall", entryIndex(btiles->catEWalls(), block.value("Wall")));
                newBlock.renameValue("Wall", QLatin1String("ExteriorWall"));
                newBlock.replaceValue("Door", entryIndex(btiles->catDoors(), block.value("Door")));
                newBlock.replaceValue("DoorFrame",  entryIndex(btiles->catDoorFrames(), block.value("DoorFrame")));
                newBlock.replaceValue("Window", entryIndex(btiles->catWindows(), block.value("Window")));
                newBlock.replaceValue("Curtains", entryIndex(btiles->catCurtains(), block.value("Curtains")));
                newBlock.replaceValue("Stairs", entryIndex(btiles->catStairs(), block.value("Stairs")));

                foreach (SimpleFileBlock roomBlock, block.blocks) {
                    SimpleFileBlock newRoomBlock = roomBlock;
                    if (roomBlock.name == QLatin1String("Room")) {
                        newRoomBlock.replaceValue("Wall", entryIndex(btiles->catIWalls(), roomBlock.value("Wall")));
                        newRoomBlock.replaceValue("Floor", entryIndex(btiles->catFloors(), roomBlock.value("Floor")));
                    }
                    newBlock.blocks += newRoomBlock;
                }
            }
            newFile.blocks += newBlock;
        }

        userFile.blocks = newFile.blocks;
        userFile.values = newFile.values;
    }

    if (userVersion < VERSION2) {
        // Rename Room.Wall -> Room.InteriorWall
        for (int i = 0; i < userFile.blocks.size(); i++) {
            SimpleFileBlock &block = userFile.blocks[i];
            if (block.name == QLatin1String("Template")) {
                for (int j = 0; j < block.blocks.size(); j++) {
                    SimpleFileBlock &roomBlock = block.blocks[j];
                    if (roomBlock.name == QLatin1String("Room")) {
                        roomBlock.renameValue("Wall", QLatin1String("InteriorWall"));
                    }
                }
            }
        }
    }

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTemplates::mergeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    Q_ASSERT(userFile.version() == VERSION_LATEST);

    QString sourcePath = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + txtName();

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    int userSourceRevision = userFile.value("source_revision").toInt();
    int sourceRevision = sourceFile.value("revision").toInt();
    if (sourceRevision == userSourceRevision)
        return true;

    // MERGE HERE

    QStringList userEntries;
    QMap<QString,int> userEntryIndexMap;
    QList<SimpleFileBlock> userTemplates;
    QMap<QString,int> userTemplateIndexByName;
    foreach (SimpleFileBlock b, userFile.blocks) {
        if (b.name == QLatin1String("TileEntry")) {
            QString entryText = b.toString();
            userEntries += entryText;
            userEntryIndexMap[entryText] = userEntries.size() - 1;
        }
        if (b.name == QLatin1String("Template")) {
            userTemplates += b;
            userTemplateIndexByName[b.value("Name")] = userTemplates.size() - 1;
        }
    }

    QStringList sourceEntries;
    QList<SimpleFileBlock> sourceTemplates;
    foreach (SimpleFileBlock b, sourceFile.blocks) {
        if (b.name == QLatin1String("TileEntry"))
            sourceEntries += b.toString();
        if (b.name == QLatin1String("Template"))
            sourceTemplates += b;
    }

    QMap<int,int> sourceToUserEntryIndexMap;
    foreach (SimpleFileBlock b, sourceTemplates) {
        if (!userTemplateIndexByName.contains(b.value("Name"))) {
            // The user-file doesn't have any template with the same name as
            // a source-template.  Append the source-template to the user-file
            // and append any missing TileEntrys to the user-file.
            for (int i = 0; i < BuildingTemplate::TileCount; i++) {
                QString key = BuildingTemplate::enumToString(i);
                int entryIndex = b.value(key).toInt() - 1;
                if (entryIndex < 0) continue; // handle "0"
                QString entryText = sourceEntries.at(entryIndex);
                if (userEntryIndexMap.contains(entryText)) {
                    sourceToUserEntryIndexMap[entryIndex] = userEntryIndexMap[entryText];
                } else {
                    userFile.blocks.insert(userEntries.size(), sourceFile.blocks.at(entryIndex));
                    userEntries += entryText;
                    userEntryIndexMap[entryText] = userEntries.size() - 1;
                    sourceToUserEntryIndexMap[entryIndex] = userEntries.size() - 1;
                    qDebug() << "BuildingTemplates.txt merge: added entry" << key << "for template" << b.value("Name");
                }
                b.replaceValue(key, QString::number(sourceToUserEntryIndexMap[entryIndex] + 1));
            }
            int blockIndex = 0;
            foreach (SimpleFileBlock b2, b.blocks) {
                if (b2.name == QLatin1String("Room")) {
                    QStringList keys;
                    keys << QLatin1String("Wall") << QLatin1String("Floor");
                    foreach (QString key, keys) {
                        int entryIndex = b2.value(key).toInt() - 1;
                        if (entryIndex < 0) continue; // handle "0"
                        QString entryText = sourceEntries.at(entryIndex);
                        if (userEntryIndexMap.contains(entryText)) {
                            sourceToUserEntryIndexMap[entryIndex] = userEntryIndexMap[entryText];
                        } else {
                            userFile.blocks.insert(userEntries.size(), sourceFile.blocks.at(entryIndex));
                            userEntries += entryText;
                            userEntryIndexMap[entryText] = userEntries.size() - 1;
                            sourceToUserEntryIndexMap[entryIndex] = userEntries.size() - 1;
                            qDebug() << "BuildingTemplates.txt merge: added entry" << key << "for room" << b2.value("Name") << "in template" << b.value("Name");
                        }
                        b2.replaceValue(key, QString::number(sourceToUserEntryIndexMap[entryIndex] + 1));
                    }
                    b.blocks.replace(blockIndex, b2);
                }
                ++blockIndex;
            }
            userFile.blocks += b;
            qDebug() << "BuildingTemplates.txt merge: added template" << b.value("Name");
        }
    }

    userFile.replaceValue("revision", QString::number(sourceRevision + 1));
    userFile.replaceValue("source_revision", QString::number(sourceRevision));

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

QString BuildingTemplates::nameForEntry(BuildingTileEntry *entry)
{
    QString name = entry->category()->name();
    for (int i = 0; i < entry->category()->enumCount(); i++)
        name += entry->category()->enumToString(i)
                + entry->tile(i)->name();

    QString key = name + QLatin1Char('#');
    int n = 1;
    while (mEntriesByCategoryName.contains(key + QString::number(n)))
        n++;

    return name;
}

void BuildingTemplates::addEntry(BuildingTileEntry *entry, bool sort)
{
    if (entry && !entry->isNone() && !mEntries.contains(entry)) {
        mEntriesByCategoryName[nameForEntry(entry)] = entry;
        if (sort)
            mEntries = mEntriesByCategoryName.values();
        else
            mEntries += entry;
    }
}

void BuildingTemplates::addEntry(BuildingTileCategory *category,
                                 const QString &tileName)
{
    if (tileName.isEmpty())
        return;
    QPair<BuildingTileCategory*,QString> p = qMakePair(category, tileName);
    if (mEntryMap.contains(p))
        return;

    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
    mEntriesByCategoryName[category->name() + tileName] = entry;
    mEntries = mEntriesByCategoryName.values(); // sorted
    mEntryMap[p] = entry;
}

QString BuildingTemplates::entryIndex(BuildingTileEntry *entry)
{
    if (!entry || entry->isNone())
        return QString::number(0);
    return QString::number(mEntries.indexOf(entry) + 1);
}

BuildingTileEntry *BuildingTemplates::getEntry(const QString &s, bool orNone)
{
    int index = s.toInt();
    if (index >= 1 && index <= mEntries.size())
        return mEntries[index - 1];
    return orNone ? BuildingTilesMgr::instance()->noneTileEntry() : 0;
}

/////

QStringList BuildingTemplate::mEnumNames;

BuildingTemplate::BuildingTemplate() :
    mTiles(TileCount)
{
}

BuildingTemplate::BuildingTemplate(BuildingTemplate *other)
{
    Name = other->Name;
    mTiles = other->mTiles;
    foreach (Room *room, other->RoomList)
        RoomList += new Room(room);
    mUsedTiles = other->mUsedTiles;
    mUsedFurniture = other->mUsedFurniture;
}

void BuildingTemplate::setTile(int n, BuildingTileEntry *entry)
{
    if (entry)
        entry = entry->asCategory(categoryEnum(n));

    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();

    mTiles[n] = entry;
}

void BuildingTemplate::setTiles(const QVector<BuildingTileEntry *> &entries)
{
    for (int i = 0; i < entries.size(); i++)
        setTile(i, entries[i]);
}

int BuildingTemplate::categoryEnum(int n)
{
    switch (n) {
    case ExteriorWall: return BuildingTilesMgr::ExteriorWalls;
    case Door: return BuildingTilesMgr::Doors;
    case DoorFrame: return BuildingTilesMgr::DoorFrames;
    case Window: return BuildingTilesMgr::Windows;
    case Curtains: return BuildingTilesMgr::Curtains;
    case Stairs: return BuildingTilesMgr::Stairs;
    case RoofCap: return BuildingTilesMgr::RoofCaps;
    case RoofSlope: return BuildingTilesMgr::RoofSlopes;
    case RoofTop: return BuildingTilesMgr::RoofTops;
    default:
        qFatal("Invalid enum passed to BuildingTemplate::categoryEnum");
        break;
    }
    return 0;
}

QString BuildingTemplate::enumToString(int n)
{
    initNames();
    return mEnumNames[n];
}

void BuildingTemplate::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
    mEnumNames.reserve(TileCount);
    mEnumNames += QLatin1String("ExteriorWall");
    mEnumNames += QLatin1String("Door");
    mEnumNames += QLatin1String("DoorFrame");
    mEnumNames += QLatin1String("Window");
    mEnumNames += QLatin1String("Curtains");
    mEnumNames += QLatin1String("Stairs");
    mEnumNames += QLatin1String("RoofCap");
    mEnumNames += QLatin1String("RoofSlope");
    mEnumNames += QLatin1String("RoofTop");
    Q_ASSERT(mEnumNames.size() == TileCount);
}

/////

QStringList Room::mEnumNames;

void Room::setTile(int n, BuildingTileEntry *entry)
{
    if (entry)
        entry = entry->asCategory(categoryEnum(n));

    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();

    mTiles[n] = entry;
}

QString Room::enumToString(int n)
{
    initNames();
    return mEnumNames[n];
}

int Room::categoryEnum(int n)
{
    switch (n) {
    case InteriorWall: return BuildingTilesMgr::InteriorWalls;
    case Floor: return BuildingTilesMgr::Floors;
    case GrimeFloor: return BuildingTilesMgr::GrimeFloor;
    case GrimeWall: return BuildingTilesMgr::GrimeWall;
    default:
        qFatal("Invalid enum passed to Room::categoryEnum");
        break;
    }
    return 0;
}

void Room::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
    mEnumNames.reserve(TileCount);
    mEnumNames += QLatin1String("InteriorWall");
    mEnumNames += QLatin1String("Floor");
    mEnumNames += QLatin1String("GrimeFloor");
    mEnumNames += QLatin1String("GrimeWall");
    Q_ASSERT(mEnumNames.size() == TileCount);
}
