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

#include "mainwindow.h"
#include "preferences.h"
#include "simplefile.h"
#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

#include <QDir>
#include <QImage>
#include <QImageReader>

using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "Tilesets.txt";

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

/**
  * Change the directory to look for tileset images in.
  * Any tilesets that were already found relative to the old directory
  * are searched for again relative to the new directory.
  */
void TileMetaInfoMgr::changeTilesDirectory(const QString &path)
{
    QDir tilesDir(tilesDirectory());
    foreach (Tileset *ts, tilesets()) {
        if (ts->isMissing())
            continue; // keep the relative path
        QString relativePath = tilesDir.relativeFilePath(ts->imageSource());
        if (!QDir::isRelativePath(relativePath))
            continue;
        QString source = path + QLatin1Char('/') + relativePath;
        QFileInfo finfo(source);
        QString oldSource = ts->imageSource();
#if 1
        QImageReader reader(source);
        if (reader.size().isValid()) {
            TilesetManager::instance()->changeTilesetSource(ts, finfo.canonicalFilePath(), false);
            TilesetManager::instance()->loadTileset(ts, ts->imageSource());
        } else {
            // There was a valid image in the old directory, but not in the new one.
            Tile *missingTile = TilesetManager::instance()->missingTile();
            for (int i = 0; i < ts->tileCount(); i++)
                ts->tileAt(i)->setImage(missingTile->image());
            TilesetManager::instance()->changeTilesetSource(ts, relativePath, true);
        }
#else
        if (finfo.exists() && loadTilesetImage(ts, finfo.canonicalFilePath())) {
            TilesetManager::instance()->tilesetSourceChanged(ts, oldSource, false);
        } else {
            Tile *missingTile = TilesetManager::instance()->missingTile();
            for (int i = 0; i < ts->tileCount(); i++)
                ts->tileAt(i)->setImage(missingTile->image());
            TilesetManager::instance()->changeTilesetSource(ts, relativePath, true);
        }
#endif
    }
    Preferences::instance()->setTilesDirectory(path);
    loadTilesets();
}

TileMetaInfoMgr::TileMetaInfoMgr(QObject *parent) :
    QObject(parent),
    mRevision(0),
    mSourceRevision(0),
    mHasReadTxt(false)
{
    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));
}

TileMetaInfoMgr::~TileMetaInfoMgr()
{
    TilesetManager::instance()->removeReferences(tilesets());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
    qDeleteAll(mTilesetInfo);
}

QString TileMetaInfoMgr::tilesDirectory() const
{
    return Preferences::instance()->tilesDirectory();
}

QString TileMetaInfoMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString TileMetaInfoMgr::txtPath()
{
    return QDir::homePath() + QLatin1String("/.TileZed/") + txtName();
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool TileMetaInfoMgr::readTxt()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = this->tilesDirectory();
    QDir dir(tilesDirectory);
    if (tilesDirectory.isEmpty() || !dir.exists()) {
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

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("meta-enums")) {
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
            QString tilesetFileName = block.value("file");
            if (tilesetFileName.isEmpty()) {
                mError = tr("No-name tilesets aren't allowed.");
                return false;
            }
            tilesetFileName += QLatin1String(".png");
            QFileInfo finfo(tilesetFileName); // relative to Tiles directory
            QString tilesetName = finfo.completeBaseName();
            if (mTilesetInfo.contains(tilesetName)) {
                mError = tr("Duplicate tileset '%1'.").arg(tilesetName);
                return false;
            }
            Tileset *tileset = new Tileset(tilesetName, 64, 128);
            {
                QString size = block.value("size");
                int columns, rows;
                if (!parse2Ints(size, &columns, &rows) ||
                        (columns < 1) || (rows < 1)) {
                    mError = tr("Invalid tileset size '%1' for tileset '%2'")
                            .arg(size).arg(tilesetName);
                    return false;
                }

                // Don't load the tilesets yet because the user might not have
                // chosen the Tiles directory. The tilesets will be loaded when
                // other code asks for them or when the Tiles directory is changed.
                int width = columns * 64, height = rows * 128;
#if 1
                tileset->loadFromNothing(QSize(width, height), tilesetFileName);
#else
                QImage image(width, height, QImage::Format_ARGB32);
                image.fill(Qt::red);
                tileset->loadFromImage(image, tilesetFileName);
#endif
                Tile *missingTile = TilesetManager::instance()->missingTile();
                for (int i = 0; i < tileset->tileCount(); i++)
                    tileset->tileAt(i)->setImage(missingTile->image());
                tileset->setMissing(true);
            }
            addTileset(tileset);

            TilesetMetaInfo *info = new TilesetMetaInfo;
            foreach (SimpleFileBlock tileBlock, block.blocks) {
                if (tileBlock.name == QLatin1String("tile")) {
                    QString coordString;
                    foreach (SimpleFileKeyValue kv, tileBlock.values) {
                        if (kv.name == QLatin1String("xy")) {
                            int column, row;
                            if (!parse2Ints(kv.value, &column, &row) ||
                                    (column < 0) || (row < 0)) {
                                mError = tr("Invalid %1 = %2").arg(kv.name).arg(kv.value);
                                return false;
                            }
                            coordString = kv.value;
                        } else if (kv.name == QLatin1String("meta-enum")) {
                            QString enumName = kv.value;
                            if (!mEnums.contains(enumName)) {
                                mError = tr("Unknown enum '%1'").arg(enumName);
                                return false;
                            }
                            Q_ASSERT(!coordString.isEmpty());
                            info->mInfo[coordString].mMetaGameEnum = enumName;
                        } else {
                            mError = tr("Unknown value name '%1'.").arg(kv.name);
                            return false;
                        }
                    }
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

    foreach (QString enumName, mEnumNames) {
        if (isEnumWest(enumName) || isEnumNorth(enumName)) {
            if (mEnums.values().contains(mEnums[enumName] + 1)) {
                QString enumImplicit = enumName;
                enumImplicit.replace(
                            QLatin1Char(isEnumWest(enumName) ? 'W' : 'N'),
                            QLatin1String(isEnumWest(enumName) ? "E" : "S"));
                mError = tr("Meta-enum %1=%2 requires an implicit %3=%4 but that value is used by %5=%6.")
                        .arg(enumName).arg(mEnums[enumName])
                        .arg(enumImplicit).arg(mEnums[enumName]+1)
                        .arg(mEnums.key(mEnums[enumName] + 1)).arg(mEnums[enumName] + 1);
                return false;
            }
        }
    }

    mHasReadTxt = true;

    return true;
}

bool TileMetaInfoMgr::writeTxt()
{
    SimpleFile simpleFile;

    SimpleFileBlock enumsBlock;
    enumsBlock.name = QLatin1String("meta-enums");
    foreach (QString name, mEnumNames) {
        enumsBlock.addValue(name, QString::number(mEnums[name]));
    }
    simpleFile.blocks += enumsBlock;

    QDir tilesDir(tilesDirectory());
    foreach (Tiled::Tileset *tileset, tilesets()) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");

        QString relativePath = tilesDir.relativeFilePath(tileset->imageSource());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        tilesetBlock.addValue("file", relativePath);

        int columns = tileset->columnCount();
        int rows = tileset->tileCount() / columns;
        tilesetBlock.addValue("size", QString(QLatin1String("%1,%2")).arg(columns).arg(rows));

        if (mTilesetInfo.contains(tileset->name())) {
            QMap<QString,TileMetaInfo> &info = mTilesetInfo[tileset->name()]->mInfo;
            foreach (QString key, info.keys()) {
                Q_ASSERT(info[key].mMetaGameEnum.isEmpty() == false);
                if (info[key].mMetaGameEnum.isEmpty())
                    continue;
                SimpleFileBlock tileBlock;
                tileBlock.name = QLatin1String("tile");
                tileBlock.addValue("xy", key);
                tileBlock.addValue("meta-enum", info[key].mMetaGameEnum);
                tilesetBlock.blocks += tileBlock;
            }
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
    emit tilesetAdded(tileset);
}

void TileMetaInfoMgr::removeTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()));
    Q_ASSERT(mRemovedTilesets.contains(tileset) == false);
    emit tilesetAboutToBeRemoved(tileset);
    mTilesetByName.remove(tileset->name());
    emit tilesetRemoved(tileset);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += tileset;
    //    TilesetManager::instance()->removeReference(tileset);
}

void TileMetaInfoMgr::loadTilesets(const QList<Tileset *> &tilesets)
{
    QList<Tileset *> _tilesets = tilesets;
    if (_tilesets.isEmpty())
        _tilesets = this->tilesets();

    foreach (Tileset *ts, _tilesets) {
        if (ts->isMissing()) {
            QString oldSource = ts->imageSource();
            QString source = tilesDirectory() + QLatin1Char('/')
                    // This is the name that was saved in Tilesets.txt,
                    // relative to Tiles directory, plus .png.
                    + ts->imageSource();
#if 1
            QImageReader reader(source);
            if (reader.size().isValid()) {
                ts->loadFromNothing(reader.size(), ts->imageSource()); // update the size now
                QFileInfo info(source);
                TilesetManager::instance()->loadTileset(ts, info.canonicalFilePath());
            }
#else
            QFileInfo info(source);
            if (info.exists()) {
                source = info.canonicalFilePath();
                if (loadTilesetImage(ts, source)) {
                    ts->setMissing(false); // Yay!
                    TilesetManager::instance()->tilesetSourceChanged(ts, oldSource, true);
                }
            }
#endif
        }
    }
}

void TileMetaInfoMgr::tilesetChanged(Tileset *ts)
{
    if (tilesets().contains(ts)) {
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

bool TileMetaInfoMgr::isEnumWest(int enumValue) const
{
    Q_ASSERT(mEnums.values().contains(enumValue));
    return mEnums.key(enumValue).endsWith(QLatin1Char('W'));
}

bool TileMetaInfoMgr::isEnumNorth(int enumValue) const
{
    Q_ASSERT(mEnums.values().contains(enumValue));
    return mEnums.key(enumValue).endsWith(QLatin1Char('N'));
}

bool TileMetaInfoMgr::isEnumWest(const QString &enumName) const
{
    return enumName.endsWith(QLatin1Char('W'));
}

bool TileMetaInfoMgr::isEnumNorth(const QString &enumName) const
{
    return enumName.endsWith(QLatin1Char('N'));
}

bool TileMetaInfoMgr::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}

/////

QString TilesetMetaInfo::key(Tile *tile)
{
    int column = tile->id() % tile->tileset()->columnCount();
    int row = tile->id() / tile->tileset()->columnCount();
    return QString(QLatin1String("%1,%2")).arg(column).arg(row);
}
