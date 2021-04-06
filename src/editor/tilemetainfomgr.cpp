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
#include "tilesetstxtfile.h"

#include "tile.h"
#include "tileset.h"

#include <QDir>
#include <QImage>
#include <QImageReader>

using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "Tilesets.txt";

TileMetaInfoMgr* TileMetaInfoMgr::mInstance = nullptr;

TileMetaInfoMgr* TileMetaInfoMgr::instance()
{
    if (!mInstance)
        mInstance = new TileMetaInfoMgr;
    return mInstance;
}

void TileMetaInfoMgr::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

void TileMetaInfoMgr::changeTilesDirectory(const QString &path)
{
    Preferences::instance()->setTilesDirectory(path); // must be done before loading tilesets
    foreach (Tileset *ts, tilesets()) {
        if (ts->isMissing())
            continue; // keep the relative path
        QString imageSource, imageSource2x;
        TilesetManager::instance()->getTilesetFileName(ts->name(), imageSource, imageSource2x);
        QImageReader reader2x(imageSource2x);
        if (reader2x.size().isValid()) {
            // can't use canonicalFilePath since the 1x tileset may not exist
            TilesetManager::instance()->changeTilesetSource(ts, imageSource, false);
            TilesetManager::instance()->loadTileset(ts, ts->imageSource());
            continue;
        }
        QImageReader reader(imageSource);
        if (reader.size().isValid()) {
            QFileInfo finfo(imageSource);
            TilesetManager::instance()->changeTilesetSource(ts, finfo.canonicalFilePath(), false);
            TilesetManager::instance()->loadTileset(ts, ts->imageSource());
        } else {
            // There was a valid image in the old directory, but not in the new one.
            Tile *missingTile = TilesetManager::instance()->missingTile();
            for (int i = 0; i < ts->tileCount(); i++)
                ts->tileAt(i)->setImage(missingTile);
            TilesetManager::instance()->changeTilesetSource(ts, imageSource, true);
        }
    }
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

QString TileMetaInfoMgr::tiles2xDirectory() const
{
    return Preferences::instance()->tiles2xDirectory();
}

QStringList TileMetaInfoMgr::tilesetNames() const
{
    QStringList ret;
    foreach (Tileset *ts, tilesets()) {
        ret += ts->name();
    }
    return ret;
}

QString TileMetaInfoMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString TileMetaInfoMgr::txtPath()
{
    return Preferences::instance()->configPath(txtName());
}

#if 1
bool TileMetaInfoMgr::readTxt()
{
#ifdef WORLDEDxxx
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = this->tilesDirectory();
    QDir dir(tilesDirectory);
    if (tilesDirectory.isEmpty() || !dir.exists()) {
        mError = tr("The Tiles directory specified in the preferences doesn't exist!\n%1")
                .arg(tilesDirectory);
        return false;
    }
#endif

    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;

    TilesetsTxtFile reader;
    if (!reader.read(txtPath())) {
        mError = reader.errorString();
        return false;
    }

    if (reader.mVersion != TilesetsTxtFile::VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(TilesetsTxtFile::VERSION_LATEST).arg(reader.mVersion);
        return false;
    }

    mRevision = reader.mRevision;
    mSourceRevision = reader.mSourceRevision;

    for (const TilesetsTxtFile::MetaEnum& metaEnum : qAsConst(reader.mEnums)) {
        mEnumNames += metaEnum.mName;
        mEnums.insert(metaEnum.mName, metaEnum.mValue);
    }

    for (const TilesetsTxtFile::Tileset* fileTileset : qAsConst(reader.mTilesets)) {
        Tileset *tileset = new Tileset(fileTileset->mName, 64, 128);

        // Don't load the tilesets yet because the user might not have
        // chosen the Tiles directory. The tilesets will be loaded when
        // other code asks for them or when the Tiles directory is changed.
        int width = fileTileset->mColumns * 64;
        int height = fileTileset->mRows * 128;
        QString tilesetFileName = fileTileset->mFile + QLatin1String(".png");
        tileset->loadFromNothing(QSize(width, height), tilesetFileName);
        Tile *missingTile = TilesetManager::instance()->missingTile();
        for (int i = 0; i < tileset->tileCount(); i++) {
            tileset->tileAt(i)->setImage(missingTile);
        }
        tileset->setMissing(true);
        addTileset(tileset);

        TilesetMetaInfo *info = new TilesetMetaInfo;
        for (const TilesetsTxtFile::Tile& fileTile : fileTileset->mTiles) {
            QString coordString = QStringLiteral("%1,%2").arg(fileTile.mX).arg(fileTile.mY);
            info->mInfo[coordString].mMetaGameEnum = fileTile.mMetaEnum;
        }
        mTilesetInfo[fileTileset->mName] = info;
    }

    const QList<int> enumInts = mEnums.values();
    for (const QString& enumName : qAsConst(mEnumNames)) {
        if (isEnumWest(enumName) || isEnumNorth(enumName)) {
            if (enumInts.contains(mEnums[enumName] + 1)) {
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
    QList<TilesetsTxtFile::Tileset*> fileTilesets;
    QList<TilesetsTxtFile::MetaEnum> fileMetaEnums;

    for (const QString& name : qAsConst(mEnumNames)) {
        fileMetaEnums += TilesetsTxtFile::MetaEnum(name, mEnums[name]);
    }

    QDir tilesDir(tilesDirectory());
    const QList<Tileset*> tilesets1 = tilesets();
    for (Tiled::Tileset *tileset : tilesets1) {
        QString relativePath = tilesDir.relativeFilePath(tileset->imageSource());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        TilesetsTxtFile::Tileset* fileTileset = new TilesetsTxtFile::Tileset();
        fileTileset->mName = tileset->name();
        fileTileset->mFile = relativePath;

        int columns = tileset->columnCount();
        int rows = tileset->tileCount() / columns;
        if (tileset->isLoaded()) {
            columns = tileset->columnCountForWidth(tileset->imageWidth());
            rows = tileset->imageHeight() / (tileset->imageSource2x().isEmpty() ? 128 : (128 * 2));
        }
        fileTileset->mColumns = columns;
        fileTileset->mRows = rows;

        if (mTilesetInfo.contains(tileset->name())) {
            const QMap<QString,TileMetaInfo> &info = mTilesetInfo[tileset->name()]->mInfo;
            const QStringList tilesetNames = info.keys();
            for (const QString& key : tilesetNames) {
                Q_ASSERT(info[key].mMetaGameEnum.isEmpty() == false);
                if (info[key].mMetaGameEnum.isEmpty())
                    continue;
                TilesetsTxtFile::Tile fileTile;
                parse2Ints(key, &fileTile.mX, &fileTile.mY);
                fileTile.mMetaEnum = info[key].mMetaGameEnum;
                fileTileset->mTiles += fileTile;
            }
        }

        fileTilesets += fileTileset;
    }

    TilesetsTxtFile writer;
    if (!writer.write(txtPath(), ++mRevision, mSourceRevision, fileTilesets, fileMetaEnums)) {
        mError = writer.errorString();
        return false;
    }

    return true;
}
#else
bool TileMetaInfoMgr::readTxt()
{
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
                tileset->loadFromNothing(QSize(width, height), tilesetFileName);
                Tile *missingTile = TilesetManager::instance()->missingTile();
                for (int i = 0; i < tileset->tileCount(); i++)
                    tileset->tileAt(i)->setImage(missingTile);
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
        if (tileset->isLoaded()) {
            columns = tileset->columnCountForWidth(tileset->imageWidth());
            rows = tileset->imageHeight() / (tileset->imageSource2x().isEmpty() ? 128 : (128 * 2));
        }
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
#endif

bool TileMetaInfoMgr::upgradeTxt()
{
    return true;
}

bool TileMetaInfoMgr::mergeTxt()
{
#ifdef WORLDED
    // There isn't a source Tilesets.txt in WorldEd.
    return true;
#endif
    QString userPath = txtPath();

    QString sourcePath = Preferences::instance()->appConfigPath(txtName());

    TilesetsTxtFile sourceFileX;
    if (!sourceFileX.read(sourcePath)) {
        mError = sourceFileX.errorString();
        return false;
    }

    TilesetsTxtFile userFileX;
    if (!userFileX.read(userPath)) {
        mError = userFileX.errorString();
        return false;
    }

    int userSourceRevision = userFileX.mSourceRevision;
    int sourceRevision = sourceFileX.mRevision;
    if (sourceRevision == userSourceRevision) {
        return true;
    }

    // MERGE HERE

    // Overwrite all user-defined meta-enums.
    userFileX.mEnums = sourceFileX.mEnums;

    QSet<QString> enumNameSet;
    for (auto& metaEnum : userFileX.mEnums) {
        enumNameSet += metaEnum.mName;
    }

    for (auto& tilesetUser : userFileX.mTilesets) {
        QList<TilesetsTxtFile::Tile> userTiles = tilesetUser->mTiles;
        int tileIndex = 0;
        for (auto& tileUser : userTiles) {
            if (!enumNameSet.contains(tileUser.mMetaEnum)) {
                tilesetUser->mTiles.removeAt(tileIndex);
            } else {
                ++tileIndex;
            }
        }
    }

    QMap<QString,TilesetsTxtFile::Tileset*> userTilesetMap;
    for (auto& tilesetUser : userFileX.mTilesets) {
        userTilesetMap[tilesetUser->mName] = tilesetUser;
    }

    for (auto& tilesetSource : sourceFileX.mTilesets) {
        if (userTilesetMap.contains(tilesetSource->mName)) {
            TilesetsTxtFile::Tileset* userTileset = userTilesetMap[tilesetSource->mName];
            // Add missing tiles to the user file.
            for (auto& tileSource : tilesetSource->mTiles) {
                userTileset->setTile(tileSource);
            }
        }
    }

    if (!userFileX.write(userPath, sourceRevision + 1, sourceRevision, userFileX.mTilesets, userFileX.mEnums)) {
        mError = userFileX.errorString();
        return false;
    }

    return true;
}

bool TileMetaInfoMgr::addNewTilesets()
{
    QDir dir(tiles2xDirectory());
    if (!dir.exists())
        return true;

    dir.setFilter(QDir::Files);
    dir.setSorting(QDir::Name);
    QStringList nameFilters;
    foreach (QByteArray format, QImageReader::supportedImageFormats())
        nameFilters += QLatin1String("*.") + QString::fromLatin1(format);

    QFileInfoList fileInfoList = dir.entryInfoList(nameFilters);
    foreach (QFileInfo fileInfo, fileInfoList) {
        QString tilesetName = fileInfo.completeBaseName();
        if (mTilesetByName.contains(tilesetName))
            continue;
        QImageReader ir(fileInfo.absoluteFilePath());
        if (!ir.size().isValid())
            continue;
        int columns = ir.size().width() / (64 * 2);
        int rows = ir.size().height() / (64 * 2);
        Tileset *tileset = new Tileset(tilesetName, 64, 128);
        tileset->loadFromNothing(QSize(columns * 64, rows * 128), fileInfo.fileName());
        Tile *missingTile = TilesetManager::instance()->missingTile();
        for (int i = 0; i < tileset->tileCount(); i++)
            tileset->tileAt(i)->setImage(missingTile);
        tileset->setMissing(true);
        addTileset(tileset);
        TilesetMetaInfo *info = new TilesetMetaInfo;
        mTilesetInfo[tilesetName] = info;
    }

    return true;
}

Tileset *TileMetaInfoMgr::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
    if (!loadTilesetImage(ts, source)) {
        delete ts;
        return nullptr;
    }
    return ts;
}

bool TileMetaInfoMgr::loadTilesetImage(Tileset *ts, const QString &source)
{
    QString imageSource, imageSource2x;
    TilesetManager::instance()->getTilesetFileName(ts->name(), imageSource, imageSource2x);

    QImageReader ir2x(imageSource2x);
    if (ir2x.size().isValid()) {
        ts->loadFromNothing(ir2x.size() / 2, source);
        // can't use canonicalFilePath since the 1x tileset may not exist
        TilesetManager::instance()->loadTileset(ts, source);
        return true;
    }
    QImageReader reader(imageSource);
    if (reader.size().isValid()) {
        ts->loadFromNothing(reader.size(), imageSource);
        QFileInfo info(imageSource);
        TilesetManager::instance()->loadTileset(ts, info.canonicalFilePath());
        return true;
    }
    mError = tr("Error loading tileset image:\n'%1'").arg(source);
    return false;
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

void TileMetaInfoMgr::loadTilesets(const QList<Tileset *> &tilesets, bool processEvents)
{
    QList<Tileset *> _tilesets = tilesets;
    if (_tilesets.isEmpty())
        _tilesets = this->tilesets();

    foreach (Tileset *ts, _tilesets) {
        if (ts->isMissing()) {
            QString imageSource,imageSource2x;
            TilesetManager::instance()->getTilesetFileName(ts->name(), imageSource, imageSource2x);
            QImageReader ir2x(imageSource2x);
            if (ir2x.size().isValid()) {
                ts->loadFromNothing(ir2x.size() / 2, imageSource);
                // can't use canonicalFilePath since the 1x tileset may not exist
                TilesetManager::instance()->loadTileset(ts, imageSource);
                if (processEvents)
                    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
                continue;
            }
            QImageReader reader(imageSource);
            if (reader.size().isValid()) {
                ts->loadFromNothing(reader.size(), imageSource); // update the size now
                QFileInfo info(imageSource);
                TilesetManager::instance()->loadTileset(ts, info.canonicalFilePath());
                if (processEvents)
                    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
            }
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
    QStringList coords = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
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
