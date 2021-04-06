/*
 * tilesetstxtfile.cpp
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

#include "tilesetstxtfile.h"

#ifdef WORLDED
#include "simplefile.h"
#else
#include "BuildingEditor/simplefile.h"
#endif

#include <QFileInfo>
#include <QSet>
#include <QScopedPointer>

TilesetsTxtFile::TilesetsTxtFile()
{

}

TilesetsTxtFile::~TilesetsTxtFile()
{
    qDeleteAll(mTilesets);
}

bool TilesetsTxtFile::read(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(path);
        return false;
    }

    QString path2 = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path2)) {
        mError = simple.errorString();
        return false;
    }

    QString txtName = QLatin1String("Tilesets.txt");

    // TODO: handle different versions here.
    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mVersion = simple.version();
    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    QSet<QString> enumNameSet;
    QSet<int> enumValueSet;
    QSet<QString> tilesetNameSet;

    for (const SimpleFileBlock& block : qAsConst(simple.blocks)) {
        if (block.name == QLatin1String("meta-enums")) {
            for (const SimpleFileKeyValue& kv : block.values) {
                if (enumNameSet.contains(kv.name)) {
                    mError = tr("Duplicate enum %1");
                    return false;
                }
                if (kv.name.contains(QLatin1String(" "))) {
                    mError = tr("No spaces allowed in enum name '%1'").arg(kv.name);
                    return false;
                }
                bool ok;
                int value = kv.value.toInt(&ok);
                if (!ok || value < 0 || value > 255 || enumValueSet.contains(value)) {
                    mError = tr("Invalid or duplicate enum value %1 = %2")
                            .arg(kv.name).arg(kv.value);
                    return false;
                }
                mEnums += MetaEnum(kv.name, value);
                enumNameSet += kv.name;
                enumValueSet += value;
            }
        } else if (block.name == QLatin1String("tileset")) {
            QString tilesetFileName = block.value("file");
            if (tilesetFileName.isEmpty()) {
                mError = tr("No-name tilesets aren't allowed.");
                return false;
            }
//            tilesetFileName += QLatin1String(".png");
            QFileInfo finfo(tilesetFileName); // relative to Tiles directory
            QString tilesetName = finfo.completeBaseName();
            if (tilesetNameSet.contains(tilesetName)) {
                mError = tr("Duplicate tileset '%1'.").arg(tilesetName);
                return false;
            }
            QScopedPointer<Tileset> tileset(new Tileset());
            tileset->mName = tilesetName;
            tileset->mFile = tilesetFileName;

            QString size = block.value("size");
            if (!parse2Ints(size, &tileset->mColumns, &tileset->mRows) ||
                    (tileset->mColumns < 1) || (tileset->mRows < 1)) {
                mError = tr("Invalid tileset size '%1' for tileset '%2'")
                        .arg(size).arg(tilesetName);
                return false;
            }

            for (const SimpleFileBlock& tileBlock : block.blocks) {
                if (tileBlock.name == QLatin1String("tile")) {
                    Tile tile;
                    for (const SimpleFileKeyValue& kv : tileBlock.values) {
                        if (kv.name == QLatin1String("xy")) {
                            int column, row;
                            if (!parse2Ints(kv.value, &column, &row) ||
                                    (column < 0) || (row < 0)) {
                                mError = tr("Invalid %1 = %2").arg(kv.name).arg(kv.value);
                                return false;
                            }
                            tile.mX = column;
                            tile.mY = row;
                        } else if (kv.name == QLatin1String("meta-enum")) {
                            QString enumName = kv.value;
                            if (!enumNameSet.contains(enumName)) {
                                mError = tr("Unknown enum '%1'").arg(enumName);
                                return false;
                            }
//                            Q_ASSERT(!coordString.isEmpty());
                            tile.mMetaEnum = enumName;
                        } else {
                            mError = tr("Unknown value name '%1'.").arg(kv.name);
                            return false;
                        }
                    }
                    tileset->mTiles += tile;
                }
            }

            tilesetNameSet += tileset->mName;
            mTilesets += tileset.take();
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }
    return true;
}

bool TilesetsTxtFile::write(const QString &path, int revision, int sourceRevision, const QList<TilesetsTxtFile::Tileset *> &tilesets, const QList<MetaEnum> &metaEnums)
{
    SimpleFile simpleFile;

    SimpleFileBlock enumsBlock;
    enumsBlock.name = QLatin1String("meta-enums");
    for (const MetaEnum& metaEnum : metaEnums) {
        enumsBlock.addValue(metaEnum.mName, QString::number(metaEnum.mValue));
    }
    simpleFile.blocks += enumsBlock;

    for (const Tileset *tileset : tilesets) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");

        tilesetBlock.addValue("file", tileset->mFile);

        int columns = tileset->mColumns;
        int rows = tileset->mRows;
        tilesetBlock.addValue("size", QString(QLatin1String("%1,%2")).arg(columns).arg(rows));

        for (const Tile& tile : tileset->mTiles) {
            SimpleFileBlock tileBlock;
            tileBlock.name = QLatin1String("tile");
            tileBlock.addValue("xy", QString(QLatin1String("%1,%2")).arg(tile.mX).arg(tile.mY));
            tileBlock.addValue("meta-enum", tile.mMetaEnum);
            tilesetBlock.blocks += tileBlock;
        }
        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(revision));
    simpleFile.replaceValue("source_revision", QString::number(sourceRevision));
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool TilesetsTxtFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a;
    *pb = b;
    return true;
}

// // // // //

void TilesetsTxtFile::Tileset::setTile(const TilesetsTxtFile::Tile &source)
{
    int tileIndex = findTile(source.mX, source.mY);
    if (tileIndex == -1) {
        mTiles += source;
    } else {
        mTiles[tileIndex].mMetaEnum = source.mMetaEnum;
    }
}

int TilesetsTxtFile::Tileset::findTile(int column, int row)
{
    int i = 0;
    for (const Tile& tile : qAsConst(mTiles)) {
        if ((tile.mX == column) && (tile.mY == row)) {
            return i;
        }
        i++;
    }
    return -1;
}
