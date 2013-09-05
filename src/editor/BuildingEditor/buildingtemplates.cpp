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

#include "buildingtemplates.h"

#include "preferences.h"

#include "building.h"
#include "buildingtiles.h"
#include "furnituregroups.h"
#include "simplefile.h"

#include "preferences.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>

using namespace BuildingEditor;

static const char *TXT_FILE = "BuildingTemplates.txt";

/////

namespace BuildingEditor {

class TemplatesFile
{
    Q_DECLARE_TR_FUNCTIONS(TemplatesFile)

public:
    TemplatesFile();
    ~TemplatesFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<BuildingTemplate*> &templates);

    void setRevision(int revision, int sourceRevision)
    { mRevision = revision, mSourceRevision = sourceRevision; }

    int revision() const { return mRevision; }
    int sourceRevision() const { return mSourceRevision; }

    const QList<BuildingTemplate*> &templates() const
    { return mTemplates; }

    void addTemplate(BuildingTemplate *btemplate)
    { mTemplates += btemplate; }

    QString errorString() const
    { return mError; }

private:
    QString nameForEntry(BuildingTileEntry *entry);
    void addEntry(BuildingTileEntry *entry, bool sort = true);
    QString entryIndex(BuildingTileEntry *entry);
    BuildingTileEntry *getEntry(const QString &s, bool orNone = true);

    BuildingTileEntry *convertOldEntry(BuildingTileCategory *category,
                                       const QString &tileName);
//    QString entryIndex(BuildingTileCategory *category, const QString &tileName);

    void addFurniture(FurnitureTiles *ftiles);
    QString furnitureIndex(FurnitureTiles *ftiles);
    FurnitureTiles *getFurnitureTiles(const QString &s);

    BuildingTileEntry *readTileEntry(SimpleFileBlock &block, QString &error);
    FurnitureTiles *readFurnitureTiles(SimpleFileBlock &block, QString &error);

    void writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry);
    void writeFurnitureTiles(SimpleFileBlock &block, FurnitureTiles *ftiles);

private:
    QList<BuildingTemplate*> mTemplates;

    int mVersion;
    int mRevision;
    int mSourceRevision;
    QString mError;

    // Used during readTxt()/writeTxt()
    QList<BuildingTileEntry*> mEntries;
    QMap<QString,BuildingTileEntry*> mEntriesByCategoryName;
    QMap<QPair<BuildingTileCategory*,QString>,BuildingTileEntry*> mEntryMap;
    QList<FurnitureTiles*> mFurnitureTiles;
};

} // namespace BuildingEditor


#define VERSION0 0

// VERSION1
// Massive rewrite -> BuildingTileEntry
#define VERSION1 1

// VERSION2
// Renamed Room.Wall -> Room.InteriorWall
#define VERSION2 2
#define VERSION_LATEST VERSION2

TemplatesFile::TemplatesFile() :
    mVersion(0),
    mRevision(0),
    mSourceRevision(0)
{
}

TemplatesFile::~TemplatesFile()
{
    qDeleteAll(mTemplates);
    // Some of mEntries and mFurnitureTiles will leak
}

bool TemplatesFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mVersion = simple.version();
    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    qDeleteAll(mTemplates);
    mTemplates.clear();

    mEntries.clear(); // delete?
    mEntriesByCategoryName.clear();
    mEntryMap.clear();
    mFurnitureTiles.clear(); // delete?

    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();

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

            if (mVersion < VERSION1) {
                def->setTile(BuildingTemplate::ExteriorWall,
                             convertOldEntry(btiles->catEWalls(), block.value("Wall")));
                def->setTile(BuildingTemplate::Door,
                             convertOldEntry(btiles->catDoors(), block.value("Door")));
                def->setTile(BuildingTemplate::DoorFrame,
                             convertOldEntry(btiles->catDoorFrames(), block.value("DoorFrame")));
                def->setTile(BuildingTemplate::Window,
                             convertOldEntry(btiles->catWindows(), block.value("Window")));
                def->setTile(BuildingTemplate::Curtains,
                             convertOldEntry(btiles->catCurtains(), block.value("Curtains")));
                def->setTile(BuildingTemplate::Stairs,
                             convertOldEntry(btiles->catStairs(), block.value("Stairs")));
            } else {
                for (int i = 0; i < BuildingTemplate::TileCount; i++)
                    def->setTile(i, getEntry(block.value(def->enumToString(i))));
            }

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

                    if (mVersion < VERSION1) {
                        room->setTile(Room::InteriorWall,
                                      convertOldEntry(btiles->catIWalls(), roomBlock.value("Wall")));
                        room->setTile(Room::Floor,
                                      convertOldEntry(btiles->catFloors(), roomBlock.value("Floor")));
                    } else {
                        if (mVersion < VERSION2) {
                            roomBlock.renameValue("Wall", QLatin1String("InteriorWall"));
                        }
                        for (int i = 0; i < Room::TileCount; i++)
                            room->setTile(i, getEntry(roomBlock.value(room->enumToString(i))));
                    }

                    room->internalName = roomBlock.value("InternalName");
                    def->addRoom(room);
                } else {
                    mError = tr("Unknown block name '%1': expected 'Room'.\n%2")
                            .arg(roomBlock.name)
                            .arg(path);
                    return false;
                }
            }
            mTemplates += def;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

bool TemplatesFile::write(const QString &fileName)
{
    return write(fileName, mTemplates);
}

bool TemplatesFile::write(const QString &fileName,
                          const QList<BuildingTemplate *> &templates)
{
    mEntries.clear();
    mEntriesByCategoryName.clear();
    mFurnitureTiles.clear();

    foreach (BuildingTemplate *btemplate, templates) {
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

    foreach (BuildingTemplate *btemplate, templates) {
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
    simpleFile.replaceValue("revision", QString::number(mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }

    return true;
}

// this code is almost the same as BuildingTilesMgr::readTileEntry
BuildingTileEntry *TemplatesFile::readTileEntry(SimpleFileBlock &block, QString &error)
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
                    error = tr("Expected 'offset = name x y', got '%1'").arg(kv.value);
                    delete entry;
                    return 0;
                }
                int e = category->enumFromString(split[0]);
                if (e == BuildingTileCategory::Invalid) {
                    error = tr("Unknown %1 enum '%2'").arg(categoryName).arg(split[0]);
                    delete entry;
                    return 0;
                }
                entry->mOffsets[e] = QPoint(split[1].toInt(), split[2].toInt());
                continue;
            }
            int e = category->enumFromString(kv.name);
            if (e == BuildingTileCategory::Invalid) {
                error = tr("Unknown %1 enum %2").arg(categoryName).arg(kv.name);
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

    error = tr("Unknown tile category '%1'").arg(categoryName);
    return 0;
}

FurnitureTiles *TemplatesFile::readFurnitureTiles(SimpleFileBlock &block, QString &error)
{
    FurnitureGroups *fg = FurnitureGroups::instance();
    if (FurnitureTiles *result = fg->furnitureTilesFromSFB(block, error)) {
        FurnitureTiles *match = fg->findMatch(result);
        if (match) {
            delete result;
            result = match;
        }
        return result;
    }
    return 0;
}

// this code is almost the same as BuildingTilesMgr::writeTileEntry
void TemplatesFile::writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry)
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

void TemplatesFile::writeFurnitureTiles(SimpleFileBlock &block, FurnitureTiles *ftiles)
{
    block.blocks += FurnitureGroups::instance()->furnitureTilesToSFB(ftiles);
}

QString TemplatesFile::nameForEntry(BuildingTileEntry *entry)
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

void TemplatesFile::addEntry(BuildingTileEntry *entry, bool sort)
{
    if (entry && !entry->isNone() && !mEntries.contains(entry)) {
        mEntriesByCategoryName[nameForEntry(entry)] = entry;
        if (sort)
            mEntries = mEntriesByCategoryName.values();
        else
            mEntries += entry;
    }
}

QString TemplatesFile::entryIndex(BuildingTileEntry *entry)
{
    if (!entry || entry->isNone())
        return QString::number(0);
    return QString::number(mEntries.indexOf(entry) + 1);
}

BuildingTileEntry *TemplatesFile::getEntry(const QString &s, bool orNone)
{
    int index = s.toInt();
    if (index >= 1 && index <= mEntries.size())
        return mEntries[index - 1];
    return orNone ? BuildingTilesMgr::instance()->noneTileEntry() : 0;
}

BuildingTileEntry *TemplatesFile::convertOldEntry(BuildingTileCategory *category,
                                                  const QString &tileName)
{
    if (tileName.isEmpty())
        return 0;
    QPair<BuildingTileCategory*,QString> p = qMakePair(category, tileName);
    if (mEntryMap.contains(p))
        return mEntryMap[p];

    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
//    mEntriesByCategoryName[category->name() + tileName] = entry;
//    mEntries = mEntriesByCategoryName.values(); // sorted
    mEntryMap[p] = entry;
    return entry;
}

void TemplatesFile::addFurniture(FurnitureTiles *ftiles)
{
    if (!ftiles)
        return;
    if (!mFurnitureTiles.contains(ftiles))
        mFurnitureTiles += ftiles;
}

QString TemplatesFile::furnitureIndex(FurnitureTiles *ftiles)
{
    int index = mFurnitureTiles.indexOf(ftiles);
    return QString::number(index + 1);
}

FurnitureTiles *TemplatesFile::getFurnitureTiles(const QString &s)
{
    int index = s.toInt();
    if (index >= 1 && index <= mFurnitureTiles.size())
        return mFurnitureTiles[index - 1];
    return 0;
}

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
#ifdef WORLDED
    return Preferences::instance()->configPath(txtName());
#else
    return BuildingPreferences::instance()->configPath(txtName());
#endif
}

bool BuildingTemplates::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!mergeTxt())
        return false;

    TemplatesFile file;
    if (!file.read(txtPath())) {
        mError = file.errorString();
        return false;
    }

    mRevision = file.revision();
    mSourceRevision = file.sourceRevision();

    foreach (BuildingTemplate *btemplate, file.templates())
        addTemplate(new BuildingTemplate(btemplate));

    return true;
}

void BuildingTemplates::writeTxt(QWidget *parent)
{
    TemplatesFile file;
    file.setRevision(++mRevision, mSourceRevision);
    if (!file.write(txtPath(), templates())) {
        mError = file.errorString();
        QMessageBox::warning(parent, tr("It's no good, Jim!"), mError);
        return;
    }
}

bool BuildingTemplates::importTemplates(const QString &fileName,
                                        QList<BuildingTemplate *> &templates)
{
    TemplatesFile file;
    if (!file.read(fileName)) {
        mError = file.errorString();
        return false;
    }
    foreach (BuildingTemplate *btemplate, file.templates())
        templates += new BuildingTemplate(btemplate);
    return true;
}

bool BuildingTemplates::exportTemplates(const QString &fileName,
                                        const QList<BuildingTemplate *> &templates)
{
    TemplatesFile file;
    file.setRevision(0, 0);
    if (!file.write(fileName, templates)) {
        mError = file.errorString();
        return false;
    }
    return true;
}

bool BuildingTemplates::mergeTxt()
{
#ifdef WORLDED
    return true;
#endif
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    Q_ASSERT(userFile.version() == VERSION_LATEST);

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

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

    TemplatesFile sourceFileX;
    if (!sourceFileX.read(sourcePath)) {
        mError = sourceFileX.errorString();
        return false;
    }

    TemplatesFile userFileX;
    if (!userFileX.read(userPath)) {
        mError = userFileX.errorString();
        return false;
    }

    QStringList userTemplateNames;
    foreach (BuildingTemplate *btemplate, userFileX.templates())
        userTemplateNames += btemplate->name();

    foreach (BuildingTemplate *btemplate, sourceFileX.templates()) {
        if (!userTemplateNames.contains(btemplate->name())) {
            userFileX.addTemplate(new BuildingTemplate(btemplate));
        }
    }

    userFileX.setRevision(sourceRevision + 1, sourceRevision);
    if (!userFileX.write(userPath)) {
        mError = userFileX.errorString();
        return false;
    }

    return true;
}

/////

QStringList BuildingTemplate::mEnumNames;
QStringList BuildingTemplate::mTileNames;

BuildingTemplate::BuildingTemplate() :
    mTiles(TileCount)
{
    Q_ASSERT(TileCount == Building::TileCount);
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
    case ExteriorWallTrim: return BuildingTilesMgr::ExteriorWallTrim;
    case Door: return BuildingTilesMgr::Doors;
    case DoorFrame: return BuildingTilesMgr::DoorFrames;
    case Window: return BuildingTilesMgr::Windows;
    case Curtains: return BuildingTilesMgr::Curtains;
    case Shutters: return BuildingTilesMgr::Shutters;
    case Stairs: return BuildingTilesMgr::Stairs;
    case RoofCap: return BuildingTilesMgr::RoofCaps;
    case RoofSlope: return BuildingTilesMgr::RoofSlopes;
    case RoofTop: return BuildingTilesMgr::RoofTops;
    case GrimeWall: return BuildingTilesMgr::GrimeWall;
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

QString BuildingTemplate::enumToTileName(int n)
{
    initNames();
    return mTileNames[n];
}

void BuildingTemplate::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
    mEnumNames.reserve(TileCount);
    mEnumNames += QLatin1String("ExteriorWall");
    mEnumNames += QLatin1String("ExteriorWallTrim");
    mEnumNames += QLatin1String("Door");
    mEnumNames += QLatin1String("DoorFrame");
    mEnumNames += QLatin1String("Window");
    mEnumNames += QLatin1String("Curtains");
    mEnumNames += QLatin1String("Shutters");
    mEnumNames += QLatin1String("Stairs");
    mEnumNames += QLatin1String("RoofCap");
    mEnumNames += QLatin1String("RoofSlope");
    mEnumNames += QLatin1String("RoofTop");
    mEnumNames += QLatin1String("GrimeWall");
    Q_ASSERT(mEnumNames.size() == TileCount);

    // These are displayed in the UI (templates dialog for example).
    mTileNames.reserve(TileCount);
    mTileNames += QCoreApplication::tr("Exterior wall");
    mTileNames += QCoreApplication::tr("Trim - Exterior wall");
    mTileNames += QCoreApplication::tr("Door");
    mTileNames += QCoreApplication::tr("Door frame");
    mTileNames += QCoreApplication::tr("Window");
    mTileNames += QCoreApplication::tr("Curtains");
    mTileNames += QCoreApplication::tr("Shutters");
    mTileNames += QCoreApplication::tr("Stairs");
    mTileNames += QCoreApplication::tr("Roof Cap");
    mTileNames += QCoreApplication::tr("Roof Slope");
    mTileNames += QCoreApplication::tr("Roof Top");
    mTileNames += QCoreApplication::tr("Grime - Walls");
    Q_ASSERT(mTileNames.size() == TileCount);
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

QStringList Room::enumLabels()
{
    return QStringList()
            << BuildingTilesMgr::instance()->catIWalls()->label()
            << BuildingTilesMgr::instance()->catIWallTrim()->label()
            << BuildingTilesMgr::instance()->catFloors()->label()
            << BuildingTilesMgr::instance()->catGrimeFloor()->label()
            << BuildingTilesMgr::instance()->catGrimeWall()->label();
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
    case InteriorWallTrim: return BuildingTilesMgr::InteriorWallTrim;
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
    mEnumNames += QLatin1String("InteriorWallTrim");
    mEnumNames += QLatin1String("Floor");
    mEnumNames += QLatin1String("GrimeFloor");
    mEnumNames += QLatin1String("GrimeWall");
    Q_ASSERT(mEnumNames.size() == TileCount);
}
