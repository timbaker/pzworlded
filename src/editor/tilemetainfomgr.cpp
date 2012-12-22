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

#include "tilemetainfomgr.h"

#include "simplefile.h"

#include "mainwindow.h"
#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

#include <QDir>
#include <QImage>

using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "TileMetaInfo.txt";

TileMetaInfoMgr* TileMetaInfoMgr::mInstance = 0;

TileMetaInfoMgr* TileMetaInfoMgr::instance()
{
    if (!mInstance)
        mInstance = new TileMetaInfoMgr;
    return mInstance;
}

void TileMetaInfoMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

TileMetaInfoMgr::TileMetaInfoMgr(QObject *parent) :
    QObject(parent),
    mRevision(0),
    mSourceRevision(0)
{
}

TileMetaInfoMgr::~TileMetaInfoMgr()
{
    TilesetManager::instance()->removeReferences(tilesets());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
}

QString TileMetaInfoMgr::tilesDirectory() const
{
    return mTilesDirectory;
}

void TileMetaInfoMgr::setTilesDirectory(const QString &path)
{
    mTilesDirectory = path;

    // Try to load any tilesets that weren't found.
    loadTilesets();
}

QString TileMetaInfoMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString TileMetaInfoMgr::txtPath()
{
    return tilesDirectory() + QLatin1String("/") + txtName();
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool TileMetaInfoMgr::readTxt()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = this->tilesDirectory();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        mError = tr("The Tiles directory specified in the preferences doesn't exist!\n%1")
                .arg(tilesDirectory);
        return false;
    }

    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    QStringList missingTilesets;

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tileset")) {
                    // Just get the list of names.  Don't load the tilesets yet
                    // because the user might not have chosen the Tiles directory.
                    // The tilesets will be loaded when other code asks for them or
                    // when the Tiles directory is changed.
                    QFileInfo info(kv.value); // relative to Tiles directory
                    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
                    // We don't know how big the image actually is, so it only
                    // has one tile.
                    Tile *missingTile = TilesetManager::instance()->missingTile();
                    ts->loadFromImage(missingTile->image().toImage(),
                                      kv.value + QLatin1String(".png"));
                    ts->setMissing(true);
                    addTileset(ts);
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else if (block.name == QLatin1String("enums")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (mEnums.contains(kv.name)) {
                    mError = tr("Duplicate enum %1");
                    return false;
                }
                if (kv.name.contains(QLatin1String(" "))) {
                    mError = tr("No spaces allowed in enum name '%1'").arg(kv.name);
                    return false;
                }
                bool ok;
                int value = kv.value.toInt(&ok);
                if (!ok || value < 0 || value > 255
                        || mEnums.values().contains(value)) {
                    mError = tr("Invalid or duplicate enum value %1 = %2")
                            .arg(kv.name).arg(kv.value);
                    return false;
                }
                mEnumNames += kv.name; // preserve order
                mEnums.insert(kv.name, value);
            }
        } else if (block.name == QLatin1String("tileset")) {
            QString tilesetName = block.value("name");
            if (mTilesetInfo.contains(tilesetName)) {
                mError = tr("Duplicate tileset '%1'.").arg(tilesetName);
                return false;
            }
            TilesetMetaInfo *info = new TilesetMetaInfo;
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("name")) {
                    continue;
                } else if (kv.name == QLatin1String("tile")) {
                    QStringList values = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                    if (values.size() != 2) {
tileError:
                        mError = tr("Invalid %1 = %2").arg(kv.name).arg(kv.value);
                        return false;
                    }
                    QString coordString = values[0];
                    QStringList coords = coordString.split(QLatin1Char(','), QString::SkipEmptyParts);
                    if (coords.size() != 2) goto tileError;
                    bool ok;
                    int column = coords[0].toInt(&ok);
                    if (!ok || column < 0) goto tileError;
                    int row = coords[1].toInt(&ok);
                    if (!ok || row < 0) goto tileError;
                    QString enumName = values[1];
                    if (!mEnums.contains(enumName)) {
                        mError = tr("Unknown enum '%1'").arg(enumName);
                        return false;
                    }
                    info->mInfo[coordString].mMetaGameEnum = enumName;
                } else {
                    mError = tr("Unknown value name '%1'.").arg(kv.name);
                    return false;
                }
            }
            mTilesetInfo[tilesetName] = info;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }
#if 0
    if (missingTilesets.size()) {
        BuildingEditor::ListOfStringsDialog dialog(tr("The following tileset files were not found."),
                                                   missingTilesets,
                                                   0/*MainWindow::instance()*/);
        dialog.setWindowTitle(tr("Missing Tilesets"));
        dialog.exec();
    }
#endif
    return true;
}

bool TileMetaInfoMgr::writeTxt()
{
    SimpleFile simpleFile;

    QDir tilesDir(tilesDirectory());
    SimpleFileBlock tilesetBlock;
    tilesetBlock.name = QLatin1String("tilesets");
    foreach (Tiled::Tileset *tileset, tilesets()) {
        QString relativePath = tilesDir.relativeFilePath(tileset->imageSource());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        tilesetBlock.values += SimpleFileKeyValue(QLatin1String("tileset"), relativePath);
    }
    simpleFile.blocks += tilesetBlock;

    SimpleFileBlock enumsBlock;
    enumsBlock.name = QLatin1String("enums");
    foreach (QString name, mEnumNames) {
        enumsBlock.addValue(name, QString::number(mEnums[name]));
    }
    simpleFile.blocks += enumsBlock;

    foreach (Tiled::Tileset *tileset, tilesets()) {
        if (!mTilesetInfo.contains(tileset->name()))
            continue;
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("name", tileset->name());
        QMap<QString,TileMetaInfo> &info = mTilesetInfo[tileset->name()]->mInfo;
        foreach (QString key, info.keys()) {
            Q_ASSERT(info[key].mMetaGameEnum.isEmpty() == false);
            if (info[key].mMetaGameEnum.isEmpty())
                continue;
            QString value = key + QLatin1String(" ") + info[key].mMetaGameEnum;
            tilesetBlock.addValue("tile", value);
        }
        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool TileMetaInfoMgr::upgradeTxt()
{
    return true;
}

bool TileMetaInfoMgr::mergeTxt()
{
    return true;
}

void TileMetaInfoMgr::addOrReplaceTileset(Tileset *ts)
{
    if (mTilesetByName.contains(ts->name())) {
        Tileset *old = mTilesetByName[ts->name()];
        if (!mRemovedTilesets.contains(old))
            mRemovedTilesets += old;
        if (!mRemovedTilesets.contains(ts)) // always true
            TilesetManager::instance()->addReference(ts);
        mRemovedTilesets.removeAll(ts);
        mTilesetByName[ts->name()] = ts;
    } else {
        addTileset(ts);
    }
}

Tileset *TileMetaInfoMgr::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
    if (!loadTilesetImage(ts, source)) {
        delete ts;
        return 0;
    }
    return ts;
}

bool TileMetaInfoMgr::loadTilesetImage(Tileset *ts, const QString &source)
{
    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }

    return ts;
}

void TileMetaInfoMgr::addTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()) == false);
    mTilesetByName[tileset->name()] = tileset;
    if (!mRemovedTilesets.contains(tileset))
        TilesetManager::instance()->addReference(tileset);
    mRemovedTilesets.removeAll(tileset);
//    emit tilesetAdded(tileset);
}

void TileMetaInfoMgr::removeTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()));
    Q_ASSERT(mRemovedTilesets.contains(tileset) == false);
//    emit tilesetAboutToBeRemoved(tileset);
    mTilesetByName.remove(tileset->name());
//    emit tilesetRemoved(tileset);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += tileset;
    //    TilesetManager::instance()->removeReference(tileset);
}

void TileMetaInfoMgr::loadTilesets()
{
    foreach (Tileset *ts, tilesets()) {
        if (ts->isMissing()) {
            QString source = tilesDirectory() + QLatin1Char('/')
                    // This is the name that was saved in PathGenerators.txt,
                    // relative to Tiles directory, plus .png.
                    + ts->imageSource();
            QFileInfo info(source);
            if (info.exists()) {
                source = info.canonicalFilePath();
                if (loadTilesetImage(ts, source)) {
                    ts->setMissing(false); // Yay!
                    TilesetManager::instance()->tilesetSourceChanged(ts);
                }
            }
        }
    }
}

void TileMetaInfoMgr::setTileEnum(Tile *tile, const QString &enumName)
{
    QString key = TilesetMetaInfo::key(tile);
    QString tilesetName = tile->tileset()->name();
    if (enumName.isEmpty()) {
        if (mTilesetInfo.contains(tilesetName))
            mTilesetInfo[tilesetName]->mInfo.remove(key);
        return;
    }
    if (!mTilesetInfo.contains(tilesetName))
        mTilesetInfo[tilesetName] = new TilesetMetaInfo;
    TilesetMetaInfo *info = mTilesetInfo[tilesetName];
    info->mInfo[key].mMetaGameEnum = enumName;
}

QString TileMetaInfoMgr::tileEnum(Tile *tile)
{
    QString tilesetName = tile->tileset()->name();
    if (!mTilesetInfo.contains(tilesetName))
        return QString();
    QString key = TilesetMetaInfo::key(tile);
    TilesetMetaInfo *info = mTilesetInfo[tilesetName];
    if (!info->mInfo.contains(key))
        return QString();
    return info->mInfo[key].mMetaGameEnum;
}

int TileMetaInfoMgr::tileEnumValue(Tile *tile)
{
    QString enumName = tileEnum(tile);
    if (!enumName.isEmpty())
        return mEnums[enumName];
    return -1;
}

/////

QString TilesetMetaInfo::key(Tile *tile)
{
    int column = tile->id() % tile->tileset()->columnCount();
    int row = tile->id() / tile->tileset()->columnCount();
    return QString(QLatin1String("%1,%2")).arg(column).arg(row);
}
