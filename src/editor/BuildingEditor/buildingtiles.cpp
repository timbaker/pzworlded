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

#include "buildingtiles.h"

#include "preferences.h"
#include "simplefile.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "BuildingTiles.txt";

/////

BuildingTilesMgr *BuildingTilesMgr::mInstance = 0;

BuildingTilesMgr *BuildingTilesMgr::instance()
{
    if (!mInstance)
        mInstance = new BuildingTilesMgr;
    return mInstance;
}

void BuildingTilesMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTilesMgr::BuildingTilesMgr() :
    mMissingTile(0),
    mNoneTiledTile(0),
    mNoneBuildingTile(0),
    mNoneCategory(0),
    mNoneTileEntry(0)
{
    mCatCurtains = new BTC_Curtains(QLatin1String("Curtains"));
    mCatDoors = new BTC_Doors(QLatin1String("Doors"));
    mCatDoorFrames = new BTC_DoorFrames(QLatin1String("Door Frames"));
    mCatFloors = new BTC_Floors(QLatin1String("Floors"));
    mCatEWalls = new BTC_EWalls(QLatin1String("Exterior Walls"));
    mCatIWalls = new BTC_IWalls(QLatin1String("Interior Walls"));
    mCatEWallTrim = new BTC_EWallTrim(QLatin1String("Trim - Exterior Walls"));
    mCatIWallTrim = new BTC_IWallTrim(QLatin1String("Trim - Interior Walls"));
    mCatStairs = new BTC_Stairs(QLatin1String("Stairs"));
    mCatShutters = new BTC_Shutters(QLatin1String("Shutters"));
    mCatWindows = new BTC_Windows(QLatin1String("Windows"));
    mCatGrimeFloor = new BTC_GrimeFloor(QLatin1String("Grime - Floors"));
    mCatGrimeWall = new BTC_GrimeWall(QLatin1String("Grime - Walls"));
    mCatRoofCaps = new BTC_RoofCaps(QLatin1String("Roof Caps"));
    mCatRoofSlopes = new BTC_RoofSlopes(QLatin1String("Roof Slopes"));
    mCatRoofTops = new BTC_RoofTops(QLatin1String("Roof Tops"));

    mCategories << mCatEWalls << mCatIWalls << mCatEWallTrim << mCatIWallTrim
                   << mCatFloors << mCatDoors << mCatDoorFrames << mCatWindows
                   << mCatCurtains << mCatShutters << mCatStairs
                   << mCatGrimeFloor << mCatGrimeWall
                   << mCatRoofCaps << mCatRoofSlopes << mCatRoofTops;

    foreach (BuildingTileCategory *category, mCategories)
        mCategoryByName[category->name()] = category;

    mCatEWalls->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_walls.png")));
    mCatIWalls->setShadowImage(mCatEWalls->shadowImage());
    mCatEWallTrim->setShadowImage(mCatEWalls->shadowImage());
    mCatIWallTrim->setShadowImage(mCatEWalls->shadowImage());
    mCatFloors->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_floors.png")));
    mCatDoors->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_doors.png")));
    mCatDoorFrames->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_door_frames.png")));
    mCatWindows->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_windows.png")));
    mCatCurtains->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_curtains.png")));
    mCatShutters->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_shutters.png")));
    mCatStairs->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_stairs.png")));
    mCatGrimeFloor->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_grime_floor.png")));
    mCatGrimeWall->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_grime_wall.png")));
    mCatRoofCaps->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_caps.png")));
    mCatRoofSlopes->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_slopes.png")));
    mCatRoofTops->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_tops.png")));

    mMissingTile = TilesetManager::instance().missingTile();

    Tileset *tileset = new Tileset(QLatin1String("none"), 64, 128);
    tileset->setTransparentColor(Qt::white);
    QString fileName = QLatin1String(":/BuildingEditor/icons/none-tile.png");
    if (tileset->loadFromImage(QImage(fileName), fileName))
        mNoneTiledTile = tileset->tileAt(0);
    else {
        QImage image(64, 128, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, fileName);
        mNoneTiledTile = tileset->tileAt(0);
    }

    mNoneBuildingTile = new NoneBuildingTile();

    mNoneCategory = new NoneBuildingTileCategory();
    mNoneTileEntry = new NoneBuildingTileEntry(mNoneCategory);

    // Forward these signals (backwards compatibility).
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SIGNAL(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
             SIGNAL(tilesetRemoved(Tiled::Tileset*)));
}

BuildingTilesMgr::~BuildingTilesMgr()
{
    qDeleteAll(mCategories);
    if (mNoneTiledTile)
        delete mNoneTiledTile->tileset();
    delete mNoneBuildingTile;
}

BuildingTile *BuildingTilesMgr::add(const QString &tileName)
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

BuildingTile *BuildingTilesMgr::get(const QString &tileName, int offset)
{
    if (tileName.isEmpty())
        return noneTile();

    QString adjustedName = adjustTileNameIndex(tileName, offset); // also normalized

    if (!mTileByName.contains(adjustedName))
        add(adjustedName);
    return mTileByName[adjustedName];
}

QString BuildingTilesMgr::nameForTile(const QString &tilesetName, int index)
{
    // The only reason I'm padding the tile index is so that the tiles are sorted
    // by increasing tileset name and index.
    return tilesetName + QLatin1Char('_') +
            QString(QLatin1String("%1")).arg(index, 3, 10, QLatin1Char('0'));
}

QString BuildingTilesMgr::nameForTile2(const QString &tilesetName, int index)
{
    return tilesetName + QLatin1Char('_') + QString::number(index);
}

QString BuildingTilesMgr::nameForTile(Tile *tile)
{
    return nameForTile(tile->tileset()->name(), tile->id());
}

bool BuildingTilesMgr::parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    int n = tileName.lastIndexOf(QLatin1Char('_'));
    if (n == -1)
        return false;
    tilesetName = tileName.mid(0, n);
    QString indexString = tileName.mid(n + 1);
    // Strip leading zeroes from the tile index
#if 1
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);
#else
    indexString.remove( QRegExp(QLatin1String("^[0]*")) );
#endif
    bool ok;
    index = indexString.toUInt(&ok);
    return !tilesetName.isEmpty() && ok;
}

bool BuildingTilesMgr::legalTileName(const QString &tileName)
{
    QString tilesetName;
    int index;
    return parseTileName(tileName, tilesetName, index);
}

QString BuildingTilesMgr::adjustTileNameIndex(const QString &tileName, int offset)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);

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

    index += offset;
    return nameForTile(tilesetName, index);
}

QString BuildingTilesMgr::normalizeTileName(const QString &tileName)
{
    if (tileName.isEmpty())
        return tileName;
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    return nameForTile(tilesetName, index);
}

void BuildingTilesMgr::entryTileChanged(BuildingTileEntry *entry, int e)
{
    Q_UNUSED(e)
    emit entryTileChanged(entry);
}

QString BuildingTilesMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTilesMgr::txtPath()
{
#ifdef WORLDED
    return Preferences::instance()->configPath(txtName());
#else
    return BuildingPreferences::instance()->configPath(txtName());
#endif
}

static void writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry)
{
    BuildingTileCategory *category = entry->category();
    SimpleFileBlock block;
    block.name = QLatin1String("entry");
//    block.addValue("category", category->name());
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

static BuildingTileEntry *readTileEntry(BuildingTileCategory *category,
                                        SimpleFileBlock &block,
                                        QString &error)
{
    BuildingTileEntry *entry = new BuildingTileEntry(category);

    foreach (SimpleFileKeyValue kv, block.values) {
        if (kv.name == QLatin1String("offset")) {
            QStringList split = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
            if (split.size() != 3) {
                error = BuildingTilesMgr::instance()->tr("Expected 'offset = name x y', got '%1'").arg(kv.value);
                delete entry;
                return 0;
            }
            int e = category->enumFromString(split[0]);
            if (e == BuildingTileCategory::Invalid) {
                error = BuildingTilesMgr::instance()->tr("Unknown %1 enum name '%2'")
                        .arg(category->name()).arg(split[0]);
                delete entry;
                return 0;
            }
            entry->mOffsets[e] = QPoint(split[1].toInt(), split[2].toInt());
            continue;
        }
        int e = category->enumFromString(kv.name);
        if (e == BuildingTileCategory::Invalid) {
            error = BuildingTilesMgr::instance()->tr("Unknown %1 enum name '%2'")
                    .arg(category->name()).arg(kv.name);
            delete entry;
            return 0;
        }
        entry->mTiles[e] = BuildingTilesMgr::instance()->get(kv.value);
    }

    return entry;
}

// VERSION0: original format without 'version' keyvalue
#define VERSION0 0

// VERSION1
// added 'version' keyvalue
// added 'curtains' category
#define VERSION1 1

// VERSION2
// massive rewrite!
#define VERSION2 2
#define VERSION_LATEST VERSION2

bool BuildingTilesMgr::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }
#if !defined(WORLDED)
    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;
#endif
    QString path = info.canonicalFilePath();
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

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            if (!mCategoryByName.contains(categoryName)) {
                mError = tr("Unknown category '%1' in BuildingTiles.txt.").arg(categoryName);
                return false;
            }
            BuildingTileCategory *category = this->category(categoryName);
            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("entry")) {
                    if (BuildingTileEntry *entry = readTileEntry(category, block2, mError)) {
                        // read offset = a b c here too
                        category->insertEntry(category->entryCount(), entry);
                    } else
                        return false;
                } else {
                    mError = tr("Unknown block name '%1'.\n%2")
                            .arg(block2.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }
#if 0
    // Check that all the tiles exist
    foreach (BuildingTileCategory *category, categories()) {
        foreach (BuildingTileEntry *entry, category->entries()) {
            for (int i = 0; i < category->enumCount(); i++) {
                if (tileFor(entry->tile(i)) == mMissingTile) {
                    mError = tr("Tile %1 #%2 doesn't exist.")
                            .arg(entry->tile(i)->mTilesetName)
                            .arg(entry->tile(i)->mIndex);
                    return false;
                }
            }
        }
    }
#endif
    foreach (BuildingTileCategory *category, mCategories)
        category->setDefaultEntry(category->entry(0));
    mCatCurtains->setDefaultEntry(noneTileEntry());

    return true;
}

void BuildingTilesMgr::writeTxt(QWidget *parent)
{
#ifdef WORLDED
    return;
#endif

    SimpleFile simpleFile;
    foreach (BuildingTileCategory *category, categories()) {
        SimpleFileBlock categoryBlock;
        categoryBlock.name = QLatin1String("category");
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("label"),
                                                   category->label());
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("name"),
                                                   category->name());
        foreach (BuildingTileEntry *entry, category->entries()) {
            writeTileEntry(categoryBlock, entry);
        }

        simpleFile.blocks += categoryBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

BuildingTileEntry *BuildingTilesMgr::defaultCategoryTile(int e) const
{
    return mCategories[e]->defaultEntry();
}

static SimpleFileBlock findCategoryBlock(const SimpleFileBlock &parent,
                                         const QString &categoryName)
{
    foreach (SimpleFileBlock block, parent.blocks) {
        if (block.name == QLatin1String("category")) {
            if (block.value("name") == categoryName)
                return block;
        }
    }
    return SimpleFileBlock();
}

bool BuildingTilesMgr::upgradeTxt()
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

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    if (userVersion == VERSION0) {
        userFile.blocks += findCategoryBlock(sourceFile, QLatin1String("curtains"));
    }

    if (userVersion < VERSION2) {
        SimpleFileBlock newFile;
        // Massive rewrite -> BuildingTileEntry stuff
        foreach (SimpleFileBlock block, userFile.blocks) {
            if (block.name == QLatin1String("category")) {
                QString categoryName = block.value(QLatin1String("name"));
                SimpleFileBlock newCatBlock;
                newCatBlock.name = block.name;
                newCatBlock.values += SimpleFileKeyValue(QLatin1String("name"),
                                                         categoryName);
                BuildingTileCategory *category = this->category(categoryName);
                foreach (SimpleFileKeyValue kv, block.block("tiles").values) {
                    QString tileName = kv.value;
                    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
                    SimpleFileBlock newEntryBlock;
                    newEntryBlock.name = QLatin1String("entry");
                    for (int i = 0; i < category->enumCount(); i++) {
                        newEntryBlock.values += SimpleFileKeyValue(category->enumToString(i),
                                                                   entry->tile(i)->name());
                        if (!entry->offset(i).isNull())
                            newEntryBlock.values += SimpleFileKeyValue(QLatin1String("offset"),
                                                                       QLatin1String("FIXME"));
                    }
                    newCatBlock.blocks += newEntryBlock;
                }
                newFile.blocks += newCatBlock;
            }
        }
        userFile.blocks = newFile.blocks;
        userFile.values = newFile.values;
    }

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTilesMgr::mergeTxt()
{
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

    QMap<QString,SimpleFileBlock> userCategoriesByName;
    QMap<QString,int> userCategoryIndexByName;
    QMap<QString,QStringList> userEntriesByCategoryName;
    int index = 0;
    foreach (SimpleFileBlock b, userFile.blocks) {
        QString name = b.value("name");
        userCategoriesByName[name] = b;
        userCategoryIndexByName[name] = index++;
        foreach (SimpleFileBlock b2, b.blocks)
            userEntriesByCategoryName[name] += b2.toString();
    }

    QMap<QString,SimpleFileBlock> sourceCategoriesByName;
    QMap<QString,QStringList> sourceEntriesByCategoryName;
    foreach (SimpleFileBlock b, sourceFile.blocks) {
        QString name = b.value("name");
        sourceCategoriesByName[name] = b;
        foreach (SimpleFileBlock b2, b.blocks)
            sourceEntriesByCategoryName[name] += b2.toString();
    }

    foreach (QString categoryName, sourceCategoriesByName.keys()) {
        if (userCategoriesByName.contains(categoryName)) {
            // A user-category with the same name as a source-category exists.
            // Copy unique source-entries to the user-category.
            int userGroupIndex = userCategoryIndexByName[categoryName];
            int userEntryIndex = userEntriesByCategoryName[categoryName].size();
            int sourceEntryIndex = 0;
            foreach (QString f, sourceEntriesByCategoryName[categoryName]) {
                if (userEntriesByCategoryName[categoryName].contains(f)) {
                    userEntryIndex = userEntriesByCategoryName[categoryName].indexOf(f) + 1;
                } else {
                    userEntriesByCategoryName[categoryName].insert(userEntryIndex, f);
                    SimpleFileBlock entryBlock = sourceCategoriesByName[categoryName].blocks.at(sourceEntryIndex);
                    userFile.blocks[userGroupIndex].blocks.insert(userEntryIndex, entryBlock);
                    qDebug() << "BuildingTiles.txt merge: inserted entry in category" << categoryName << "at" << userEntryIndex;
                    userEntryIndex++;
                }
                ++sourceEntryIndex;
            }
        } else {
            // The source-category doesn't exist in the user-file.
            // Copy the source-category to the user-file.
            userCategoriesByName[categoryName] = sourceCategoriesByName[categoryName];
            int index = userCategoriesByName.keys().indexOf(categoryName); // insert group alphabetically
            userFile.blocks.insert(index, userCategoriesByName[categoryName]);
            foreach (QString label, userCategoriesByName.keys()) {
                if (userCategoryIndexByName[label] >= index)
                    userCategoryIndexByName[label]++;
            }
            userCategoryIndexByName[categoryName] = index;
            qDebug() << "BuildingTiles.txt merge: inserted category" << categoryName << "at" << index;
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

Tiled::Tile *BuildingTilesMgr::tileFor(const QString &tileName)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tilesetName);
    if (!tileset)
        return mMissingTile;
    if (index >= tileset->tileCount())
        return mMissingTile;
    return tileset->tileAt(index);
}

Tile *BuildingTilesMgr::tileFor(BuildingTile *tile, int offset)
{
    if (tile->isNone())
        return mNoneTiledTile;
    Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tile->mTilesetName);
    if (!tileset)
        return mMissingTile;
    if (tile->mIndex + offset >= tileset->tileCount())
        return tileset->isMissing() ? tileset->tileAt(0) : mMissingTile;
    return tileset->tileAt(tile->mIndex + offset);
}

BuildingTile *BuildingTilesMgr::fromTiledTile(Tile *tile)
{
    if (tile == mNoneTiledTile)
        return mNoneBuildingTile;
    return get(nameForTile(tile));
}

BuildingTileEntry *BuildingTilesMgr::defaultExteriorWall() const
{
    return mCatEWalls->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultInteriorWall() const
{
    return mCatIWalls->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultExteriorWallTrim() const
{
    return mCatEWallTrim->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultInteriorWallTrim() const
{
    return mCatIWallTrim->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultFloorTile() const
{
    return mCatFloors->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorTile() const
{
    return mCatDoors->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorFrameTile() const
{
    return mCatDoorFrames->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultWindowTile() const
{
    return mCatWindows->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultCurtainsTile() const
{
    return mCatCurtains->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultStairsTile() const
{
    return mCatStairs->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofCapTiles() const
{
    return mCatRoofCaps->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofSlopeTiles() const
{
    return mCatRoofSlopes->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofTopTiles() const
{
    return mCatRoofTops->defaultEntry();
}

/////

QString BuildingTile::name() const
{
    return BuildingTilesMgr::nameForTile(mTilesetName, mIndex);
}

/////

BuildingTileEntry::BuildingTileEntry(BuildingTileCategory *category) :
    mCategory(category)
{
    if (category) {
        mTiles.resize(category->enumCount());
        mOffsets.resize(category->enumCount());
        for (int i = 0; i < mTiles.size(); i++)
            mTiles[i] = BuildingTilesMgr::instance()->noneTile();
    }
}

BuildingTile *BuildingTileEntry::displayTile() const
{
    return tile(mCategory->displayIndex());
}

void BuildingTileEntry::setTile(int e, BuildingTile *btile)
{
    Q_ASSERT(btile);
    mTiles[e] = btile;
}

BuildingTile *BuildingTileEntry::tile(int n) const
{
    if (n < 0 || n >= mTiles.size())
        return BuildingTilesMgr::instance()->noneTile();
    return mTiles[n];
}

void BuildingTileEntry::setOffset(int e, const QPoint &offset)
{
    mOffsets[e] = offset;
}

QPoint BuildingTileEntry::offset(int e) const
{
    if (isNone())
        return QPoint();
    if (e < 0 || e >= mOffsets.size()) {
        Q_ASSERT(false);
        return QPoint();
    }
    return mOffsets[e];
}

bool BuildingTileEntry::usesTile(BuildingTile *btile) const
{
    return mTiles.contains(btile);
}

bool BuildingTileEntry::equals(BuildingTileEntry *other) const
{
    return (mCategory == other->mCategory) &&
            (mTiles == other->mTiles) &&
            (mOffsets == other->mOffsets);
}

BuildingTileEntry *BuildingTileEntry::asCategory(int n)
{
    return (mCategory->name() == BuildingTilesMgr::instance()->category(n)->name())
            ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asExteriorWall()
{
    return mCategory->asExteriorWalls() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asInteriorWall()
{
    return mCategory->asInteriorWalls() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asExteriorWallTrim()
{
    return mCategory->asExteriorWallTrim() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asInteriorWallTrim()
{
    return mCategory->asInteriorWallTrim() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asFloor()
{
    return mCategory->asFloors() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asDoor()
{
    return mCategory->asDoors() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asDoorFrame()
{
    return mCategory->asDoorFrames() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asWindow()
{
    return mCategory->asWindows() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asCurtains()
{
    return mCategory->asCurtains() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asShutters()
{
    return mCategory->asShutters() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asStairs()
{
    return mCategory->asStairs() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofCap()
{
    return mCategory->asRoofCaps() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofSlope()
{
    return mCategory->asRoofSlopes() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofTop()
{
    return mCategory->asRoofTops() ? this : 0;
}

/////

BTC_Curtains::BTC_Curtains(const QString &label) :
    BuildingTileCategory(QLatin1String("curtains"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("East");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("South");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Curtains::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[East] = getTile(tileName, 1);
    entry->mTiles[North] = getTile(tileName, 2);
    entry->mTiles[South] = getTile(tileName, 3);
    return entry;
}

int BTC_Curtains::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, East, North, South
    };
    return map[shadowIndex];
}

/////

BTC_Shutters::BTC_Shutters(const QString &label) :
    BuildingTileCategory(QLatin1String("shutters"), label, NorthLeft)
{
    mEnumNames += QLatin1String("WestBelow");
    mEnumNames += QLatin1String("WestAbove");
    mEnumNames += QLatin1String("NorthLeft");
    mEnumNames += QLatin1String("NorthRight");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Shutters::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[WestBelow] = getTile(tileName, 0);
    entry->mTiles[WestAbove] = getTile(tileName, 1);
    entry->mTiles[NorthLeft] = getTile(tileName, 2);
    entry->mTiles[NorthRight] = getTile(tileName, 3);
    return entry;
}

int BTC_Shutters::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        WestBelow, WestAbove, NorthLeft, NorthRight
    };
    return map[shadowIndex];
}

/////

BTC_Doors::BTC_Doors(const QString &label) :
    BuildingTileCategory(QLatin1String("doors"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("WestOpen");
    mEnumNames += QLatin1String("NorthOpen");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Doors::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[WestOpen] = getTile(tileName, 2);
    entry->mTiles[NorthOpen] = getTile(tileName, 3);
    return entry;
}

int BTC_Doors::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North, WestOpen, NorthOpen
    };
    return map[shadowIndex];
}

/////

BTC_DoorFrames::BTC_DoorFrames(const QString &label) :
    BuildingTileCategory(QLatin1String("door_frames"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_DoorFrames::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    return entry;
}

int BTC_DoorFrames::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North
    };
    return map[shadowIndex];
}

/////

BTC_Floors::BTC_Floors(const QString &label) :
    BuildingTileCategory(QLatin1String("floors"), label, Floor)
{
    mEnumNames += QLatin1String("Floor");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Floors::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[Floor] = getTile(tileName);
    return entry;
}

int BTC_Floors::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        Floor
    };
    return map[shadowIndex];
}

/////

BTC_Stairs::BTC_Stairs(const QString &label) :
    BuildingTileCategory(QLatin1String("stairs"), label, West1)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Stairs::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = getTile(tileName);
    entry->mTiles[West2] = getTile(tileName, 1);
    entry->mTiles[West3] = getTile(tileName, 2);
    entry->mTiles[North1] = getTile(tileName, 8);
    entry->mTiles[North2] = getTile(tileName, 9);
    entry->mTiles[North3] = getTile(tileName, 10);
    return entry;
}

int BTC_Stairs::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West1, West2, West3, North1, North2, North3
    };
    return map[shadowIndex];
}

/////

BTC_Walls::BTC_Walls(const QString &name, const QString &label) :
    BuildingTileCategory(name, label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("SouthEast");
    mEnumNames += QLatin1String("WestWindow");
    mEnumNames += QLatin1String("NorthWindow");
    mEnumNames += QLatin1String("WestDoor");
    mEnumNames += QLatin1String("NorthDoor");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Walls::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[SouthEast] = getTile(tileName, 3);
    entry->mTiles[WestWindow] = getTile(tileName, 8);
    entry->mTiles[NorthWindow] = getTile(tileName, 9);
    entry->mTiles[WestDoor] = getTile(tileName, 10);
    entry->mTiles[NorthDoor] = getTile(tileName, 11);
    return entry;
}

int BTC_Walls::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North, NorthWest, SouthEast, WestWindow, NorthWindow, WestDoor, NorthDoor
    };
    return map[shadowIndex];
}

/////

BTC_Windows::BTC_Windows(const QString &label) :
    BuildingTileCategory(QLatin1String("windows"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Windows::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    return entry;
}

int BTC_Windows::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North
    };
    return map[shadowIndex];
}

/////

BTC_RoofCaps::BTC_RoofCaps(const QString &label) :
    BuildingTileCategory(QLatin1String("roof_caps"), label, CapRiseE3)
{
    mEnumNames += QLatin1String("CapRiseE1");
    mEnumNames += QLatin1String("CapRiseE2");
    mEnumNames += QLatin1String("CapRiseE3");
    mEnumNames += QLatin1String("CapFallE1");
    mEnumNames += QLatin1String("CapFallE2");
    mEnumNames += QLatin1String("CapFallE3");

    mEnumNames += QLatin1String("CapRiseS1");
    mEnumNames += QLatin1String("CapRiseS2");
    mEnumNames += QLatin1String("CapRiseS3");
    mEnumNames += QLatin1String("CapFallS1");
    mEnumNames += QLatin1String("CapFallS2");
    mEnumNames += QLatin1String("CapFallS3");

    mEnumNames += QLatin1String("PeakPt5S");
    mEnumNames += QLatin1String("PeakPt5E");
    mEnumNames += QLatin1String("PeakOnePt5S");
    mEnumNames += QLatin1String("PeakOnePt5E");
    mEnumNames += QLatin1String("PeakTwoPt5S");
    mEnumNames += QLatin1String("PeakTwoPt5E");

    mEnumNames += QLatin1String("CapGapS1");
    mEnumNames += QLatin1String("CapGapS2");
    mEnumNames += QLatin1String("CapGapS3");
    mEnumNames += QLatin1String("CapGapE1");
    mEnumNames += QLatin1String("CapGapE2");
    mEnumNames += QLatin1String("CapGapE3");

    mEnumNames += QLatin1String("CapShallowRiseS1");
    mEnumNames += QLatin1String("CapShallowRiseS2");
    mEnumNames += QLatin1String("CapShallowFallS1");
    mEnumNames += QLatin1String("CapShallowFallS2");
    mEnumNames += QLatin1String("CapShallowRiseE1");
    mEnumNames += QLatin1String("CapShallowRiseE2");
    mEnumNames += QLatin1String("CapShallowFallE1");
    mEnumNames += QLatin1String("CapShallowFallE2");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofCaps::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[CapRiseE1] = getTile(tileName, 0);
    entry->mTiles[CapRiseE2] = getTile(tileName, 1);
    entry->mTiles[CapRiseE3] = getTile(tileName, 2);
    entry->mTiles[CapFallE1] = getTile(tileName, 8);
    entry->mTiles[CapFallE2] = getTile(tileName, 9);
    entry->mTiles[CapFallE3] = getTile(tileName, 10);
    entry->mTiles[CapRiseS1] = getTile(tileName, 13);
    entry->mTiles[CapRiseS2] = getTile(tileName, 12);
    entry->mTiles[CapRiseS3] = getTile(tileName, 11);
    entry->mTiles[CapFallS1] = getTile(tileName, 5);
    entry->mTiles[CapFallS2] = getTile(tileName, 4);
    entry->mTiles[CapFallS3] = getTile(tileName, 3);
    entry->mTiles[PeakPt5S] = getTile(tileName, 7);
    entry->mTiles[PeakPt5E] = getTile(tileName, 15);
    entry->mTiles[PeakOnePt5S] = getTile(tileName, 6);
    entry->mTiles[PeakOnePt5E] = getTile(tileName, 14);
    entry->mTiles[PeakTwoPt5S] = getTile(tileName, 17);
    entry->mTiles[PeakTwoPt5E] = getTile(tileName, 16);
#if 0 // impossible to guess these
    entry->mTiles[CapGapS1] = getTile(tileName, );
    entry->mTiles[CapGapS2] = getTile(tileName, );
    entry->mTiles[CapGapS3] = getTile(tileName, );
    entry->mTiles[CapGapE1] = getTile(tileName, );
    entry->mTiles[CapGapE2] = getTile(tileName, );
    entry->mTiles[CapGapE3] = getTile(tileName, );
#endif
    return entry;
}

int BTC_RoofCaps::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount + 4] = {
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallS3, CapFallS2, CapFallS1,
        CapFallE1, CapFallE2, CapFallE3, CapRiseS3, CapRiseS2, CapRiseS1,
        PeakPt5E, PeakOnePt5E, PeakTwoPt5E, PeakTwoPt5S, PeakOnePt5S, PeakPt5S,
        CapGapE1, CapGapE2, CapGapE3, CapGapS3, CapGapS2, CapGapS1,
        CapShallowRiseE1, CapShallowRiseE2, -1, -1, CapShallowFallS2, CapShallowFallS1,
        CapShallowFallE1, CapShallowFallE2, -1, -1, CapShallowRiseS2, CapShallowRiseS1,
    };
    return map[shadowIndex];
}

/////

BTC_RoofSlopes::BTC_RoofSlopes(const QString &label) :
    BuildingTileCategory(QLatin1String("roof_slopes"), label, SlopeS2)
{
    mEnumNames += QLatin1String("SlopeS1");
    mEnumNames += QLatin1String("SlopeS2");
    mEnumNames += QLatin1String("SlopeS3");
    mEnumNames += QLatin1String("SlopeE1");
    mEnumNames += QLatin1String("SlopeE2");
    mEnumNames += QLatin1String("SlopeE3");

    mEnumNames += QLatin1String("SlopePt5S");
    mEnumNames += QLatin1String("SlopePt5E");
    mEnumNames += QLatin1String("SlopeOnePt5S");
    mEnumNames += QLatin1String("SlopeOnePt5E");
    mEnumNames += QLatin1String("SlopeTwoPt5S");
    mEnumNames += QLatin1String("SlopeTwoPt5E");

    mEnumNames += QLatin1String("ShallowSlopeW1");
    mEnumNames += QLatin1String("ShallowSlopeW2");
    mEnumNames += QLatin1String("ShallowSlopeE1");
    mEnumNames += QLatin1String("ShallowSlopeE2");
    mEnumNames += QLatin1String("ShallowSlopeN1");
    mEnumNames += QLatin1String("ShallowSlopeN2");
    mEnumNames += QLatin1String("ShallowSlopeS1");
    mEnumNames += QLatin1String("ShallowSlopeS2");

    mEnumNames += QLatin1String("Inner1");
    mEnumNames += QLatin1String("Inner2");
    mEnumNames += QLatin1String("Inner3");
    mEnumNames += QLatin1String("Outer1");
    mEnumNames += QLatin1String("Outer2");
    mEnumNames += QLatin1String("Outer3");
    mEnumNames += QLatin1String("InnerPt5");
    mEnumNames += QLatin1String("InnerOnePt5");
    mEnumNames += QLatin1String("InnerTwoPt5");
    mEnumNames += QLatin1String("OuterPt5");
    mEnumNames += QLatin1String("OuterOnePt5");
    mEnumNames += QLatin1String("OuterTwoPt5");
    mEnumNames += QLatin1String("CornerSW1");
    mEnumNames += QLatin1String("CornerSW2");
    mEnumNames += QLatin1String("CornerSW3");
    mEnumNames += QLatin1String("CornerNE1");
    mEnumNames += QLatin1String("CornerNE2");
    mEnumNames += QLatin1String("CornerNE3");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofSlopes::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[SlopeS1] = getTile(tileName, 0);
    entry->mTiles[SlopeS2] = getTile(tileName, 1);
    entry->mTiles[SlopeS3] = getTile(tileName, 2);
    entry->mTiles[SlopeE1] = getTile(tileName, 5);
    entry->mTiles[SlopeE2] = getTile(tileName, 4);
    entry->mTiles[SlopeE3] = getTile(tileName, 3);
    entry->mTiles[SlopePt5S] = getTile(tileName, 15);
    entry->mTiles[SlopePt5E] = getTile(tileName, 14);
    entry->mTiles[SlopeOnePt5S] = getTile(tileName, 15);
    entry->mTiles[SlopeOnePt5E] = getTile(tileName, 14);
    entry->mTiles[SlopeTwoPt5S] = getTile(tileName, 15);
    entry->mTiles[SlopeTwoPt5E] = getTile(tileName, 14);
    entry->mTiles[Inner1] = getTile(tileName, 11);
    entry->mTiles[Inner2] = getTile(tileName, 12);
    entry->mTiles[Inner3] = getTile(tileName, 13);
    entry->mTiles[Outer1] = getTile(tileName, 8);
    entry->mTiles[Outer2] = getTile(tileName, 9);
    entry->mTiles[Outer3] = getTile(tileName, 10);
    entry->mTiles[CornerSW1] = getTile(tileName, 24);
    entry->mTiles[CornerSW2] = getTile(tileName, 25);
    entry->mTiles[CornerSW3] = getTile(tileName, 26);
    entry->mTiles[CornerNE1] = getTile(tileName, 29);
    entry->mTiles[CornerNE2] = getTile(tileName, 28);
    entry->mTiles[CornerNE3] = getTile(tileName, 27);

    entry->mOffsets[SlopePt5S] = QPoint(1, 1);
    entry->mOffsets[SlopePt5E] = QPoint(1, 1);
    entry->mOffsets[SlopeTwoPt5S] = QPoint(-1, -1);
    entry->mOffsets[SlopeTwoPt5E] = QPoint(-1, -1);
    return entry;
}

int BTC_RoofSlopes::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount + 4] = {
        SlopeS1, SlopeS2, SlopeS3, SlopeE3, SlopeE2, SlopeE1,
        SlopePt5S, SlopeOnePt5S, SlopeTwoPt5S, SlopeTwoPt5E, SlopeOnePt5E, SlopePt5E,
        Outer1, Outer2, Outer3, Inner1, Inner2, Inner3,
        OuterPt5, OuterOnePt5, OuterTwoPt5, InnerPt5, InnerOnePt5, InnerTwoPt5,
        CornerSW1, CornerSW2, CornerSW3, CornerNE3, CornerNE2, CornerNE1,
        ShallowSlopeW1, ShallowSlopeW2, -1, -1, ShallowSlopeE2, ShallowSlopeE1,
        ShallowSlopeN1, ShallowSlopeN2, -1, -1, ShallowSlopeS2, ShallowSlopeS1,
    };
    return map[shadowIndex];
}

/////

BTC_RoofTops::BTC_RoofTops(const QString &label) :
    BuildingTileCategory(QLatin1String("roof_tops"), label, West2)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofTops::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = getTile(tileName);
    entry->mTiles[West2] = getTile(tileName);
    entry->mTiles[West3] = getTile(tileName);
    entry->mTiles[North1] = getTile(tileName);
    entry->mTiles[North2] = getTile(tileName);
    entry->mTiles[North3] = getTile(tileName);
    entry->mOffsets[West1] = QPoint(-1, -1);
    entry->mOffsets[West2] = QPoint(-2, -2);
    entry->mOffsets[North1] = QPoint(-1, -1);
    entry->mOffsets[North2] = QPoint(-2, -2);
    return entry;
}

int BTC_RoofTops::shadowToEnum(int shadowIndex)
{
    const int map[3] = {
        West3, West1, West2
    };
    return map[shadowIndex];
}

/////

BTC_GrimeFloor::BTC_GrimeFloor(const QString &label) :
    BuildingTileCategory(QLatin1String("grime_floor"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("East");
    mEnumNames += QLatin1String("South");
    mEnumNames += QLatin1String("SouthWest");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("NorthEast");
    mEnumNames += QLatin1String("SouthEast");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_GrimeFloor::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName, 0);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[East] = getTile(tileName, 9);
    entry->mTiles[South] = getTile(tileName, 8);
    entry->mTiles[SouthWest] = getTile(tileName, 11);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[NorthEast] = getTile(tileName, 3);
    entry->mTiles[SouthEast] = getTile(tileName, 10);
    return entry;
}

int BTC_GrimeFloor::shadowToEnum(int shadowIndex)
{
    const int map[8] = {
        West, North, East, South,
        SouthWest, NorthWest, NorthEast, SouthEast
    };
    return map[shadowIndex];
}

/////

BTC_GrimeWall::BTC_GrimeWall(const QString &label) :
    BuildingTileCategory(QLatin1String("grime_wall"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("SouthEast");
    mEnumNames += QLatin1String("WestWindow");
    mEnumNames += QLatin1String("NorthWindow");
    mEnumNames += QLatin1String("WestDoor");
    mEnumNames += QLatin1String("NorthDoor");
    mEnumNames += QLatin1String("WestTrim");
    mEnumNames += QLatin1String("NorthTrim");
    mEnumNames += QLatin1String("NorthWestTrim");
    mEnumNames += QLatin1String("SouthEastTrim");
    mEnumNames += QLatin1String("WestDoubleLeft");
    mEnumNames += QLatin1String("WestDoubleRight");
    mEnumNames += QLatin1String("NorthDoubleLeft");
    mEnumNames += QLatin1String("NorthDoubleRight");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_GrimeWall::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName, 0);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[SouthEast] = getTile(tileName, 3);
    entry->mTiles[WestWindow] = getTile(tileName, 8);
    entry->mTiles[NorthWindow] = getTile(tileName, 9);
    entry->mTiles[WestDoor] = getTile(tileName, 10);
    entry->mTiles[NorthDoor] = getTile(tileName, 11);
    return entry;
}

int BTC_GrimeWall::shadowToEnum(int shadowIndex)
{
    const int map[16] = {
        West, North, NorthWest, SouthEast,
        WestWindow, NorthWindow, WestDoor, NorthDoor,
        WestTrim, NorthTrim, NorthWestTrim, SouthEastTrim,
        WestDoubleLeft, WestDoubleRight, NorthDoubleLeft, NorthDoubleRight
    };
    return map[shadowIndex];
}

/////

BuildingTileCategory::BuildingTileCategory(const QString &name,
                                           const QString &label,
                                           int displayIndex) :
    mName(name),
    mLabel(label),
    mDisplayIndex(displayIndex),
    mDefaultEntry(0),
    mNoneTileEntry(0)
{
}

BuildingTileEntry *BuildingTileCategory::entry(int index) const
{
    if (index < 0 || index >= mEntries.size())
        return BuildingTilesMgr::instance()->noneTileEntry();
    return mEntries[index];
}

void BuildingTileCategory::insertEntry(int index, BuildingTileEntry *entry)
{
    Q_ASSERT(entry && !entry->isNone());
    Q_ASSERT(!mEntries.contains(entry));
    Q_ASSERT(entry->category() == this);
    mEntries.insert(index, entry);
}

BuildingTileEntry *BuildingTileCategory::removeEntry(int index)
{
    return mEntries.takeAt(index);
}

BuildingTileEntry *BuildingTileCategory::noneTileEntry()
{
    if (!mNoneTileEntry && canAssignNone())
        mNoneTileEntry = new NoneBuildingTileEntry(this);
    return mNoneTileEntry;
}

QString BuildingTileCategory::enumToString(int index) const
{
    if (index < 0 || index >= mEnumNames.size())
        return QLatin1String("Invalid");
    return mEnumNames[index];
}

int BuildingTileCategory::enumFromString(const QString &s) const
{
    if (mEnumNames.contains(s))
        return mEnumNames.indexOf(s);
    return Invalid;
}

int BuildingTileCategory::enumToShadow(int e)
{
    QVector<int> map(100);
    for (int i = 0; i < enumCount(); i++)
        map[i] = -1;
    for (int i = 0; i < shadowCount(); i++) {
        int e = shadowToEnum(i);
        if (e != -1)
            map[e] = i;
    }
    return map[e];
}

BuildingTileEntry *BuildingTileCategory::findMatch(BuildingTileEntry *entry) const
{
    foreach (BuildingTileEntry *candidate, mEntries) {
        if (candidate->equals(entry))
            return candidate;
    }
    return 0;
}

bool BuildingTileCategory::usesTile(Tile *tile) const
{
    BuildingTile *btile = BuildingTilesMgr::instance()->fromTiledTile(tile);
    foreach (BuildingTileEntry *entry, mEntries) {
        if (entry->usesTile(btile))
            return true;
    }
    return false;
}

BuildingTile *BuildingTileCategory::getTile(const QString &tilesetName, int offset)
{
    return BuildingTilesMgr::instance()->get(tilesetName, offset);
}

BuildingTileEntry *BuildingTileCategory::createEntryFromSingleTile(const QString &tileName)
{
    Q_UNUSED(tileName)
    return 0;
}

/////
