/*
 * tilerotationfile.cpp
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

#include "tilerotationfile.h"

#include "tilerotation.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"
#ifdef WORLDED
#include "simplefile.h"
#else
#include "BuildingEditor/simplefile.h"
#endif

#include <QFileInfo>

using namespace BuildingEditor;
using namespace Tiled;

// enum TileRotateType
const char *Tiled::TILE_ROTATE_NAMES[] = {
    "None",
    "Door",
    "DoorFrame",
    "Wall",
    "WallExtra",
    "Window",
    "WindowFrame",
    nullptr
};

static const char *MAP_ROTATION_NAMES[] = {
    "R0",
    "R90",
    "R180",
    "R270",
    nullptr
};

static const char *MAP_ROTATION_SUFFIX[] = {
    "_R0",
    "_R90",
    "_R180",
    "_R270",
    nullptr
};

TileRotationFile::TileRotationFile()
{

}

TileRotationFile::~TileRotationFile()
{
    qDeleteAll(mTilesets);
}

bool TileRotationFile::read(const QString &path)
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

    QString txtName = QLatin1Literal("TileRotation.txt");

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    for (const SimpleFileBlock& block : simple.blocks) {
        if (block.name == QLatin1String("visuals")) {
            for (const SimpleFileBlock& block1 : block.blocks) {
                if (TileRotatedVisual *visual = readVisual(block1)) {
                    mVisualLookup[visual->mUuid.toString()] = QSharedPointer<TileRotatedVisual>(visual);
                }
            }
        } else if (block.name == QLatin1String("tileset")) {
            TilesetRotated* tileset = readTileset(block);
            if (tileset == nullptr) {
                return false;
            }
            if (mTilesetByName.contains(tileset->name())) {
                continue;
            }
            mTilesets += tileset;
            mTilesetByName[tileset->name()] = tileset;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

TilesetRotated *TileRotationFile::readTileset(const SimpleFileBlock &tilesetBlock)
{
    TilesetRotated *tileset = new TilesetRotated();
    tileset->mName = tilesetBlock.value("name");
    bool ok = false;
    tileset->mColumnCount =  int(tilesetBlock.value("columns").toUInt(&ok));

    for (const SimpleFileBlock& block : tilesetBlock.blocks) {
        if (block.name == QLatin1String("tile")) {
            TileRotated *tile = readTile(block);
            if (tile == nullptr) {
                delete tileset;
                return nullptr;
            }
            tile->mTileset = tileset;
            tile->mID = tile->mXY.x() + tile->mXY.y() * tileset->mColumnCount;
            tileset->mTiles += tile; // TODO: column,row
            if (tile->mID >= tileset->mTileByID.size()) {
                tileset->mTileByID.resize(tile->mID + 1);
            }
            tileset->mTileByID[tile->mID] = tile;
        } else {
            mError = tr("Unknown block name '%1'.")
                    .arg(block.name);
            delete tileset;
            return nullptr;
        }
    }

    return tileset;
}

MapRotation TileRotationFile::parseRotation(const QString &str)
{
    for (int i = 0; MAP_ROTATION_NAMES[i] != nullptr; i++) {
        if (str == QLatin1Literal(MAP_ROTATION_NAMES[i])) {
            return static_cast<MapRotation>(i);
        }
    }
    return MapRotation::NotRotated;
}

TileRotatedVisual *TileRotationFile::readVisual(const SimpleFileBlock &visualBlock)
{
    TileRotatedVisual *visual = new TileRotatedVisual();
    visual->mUuid = visualBlock.value("uuid");

    for (const SimpleFileBlock& block : visualBlock.blocks) {
        if (block.name == QLatin1Literal("R0")) {
            if (!readDirection(block, visual->mData[0])) {
                delete visual;
                return nullptr;
            }
        }
        else if (block.name == QLatin1Literal("R90")) {
            if (!readDirection(block, visual->mData[1])) {
                delete visual;
                return nullptr;
            }
        }
        else if (block.name == QLatin1Literal("R180")) {
            if (!readDirection(block, visual->mData[2])) {
                delete visual;
                return nullptr;
            }
        }
        else if (block.name == QLatin1Literal("R270")) {
            if (!readDirection(block, visual->mData[3])) {
                delete visual;
                return nullptr;
            }
        }
        else {
            mError = tr("Unknown block name '%1'.")
                    .arg(block.name);
            delete visual;
            return nullptr;
        }
    }
    return visual;
}

TileRotated *TileRotationFile::readTile(const SimpleFileBlock &tileBlock)
{
    TileRotated *tile = new TileRotated(); // QScopedPointer
    parse2Ints(tileBlock.value("xy"), &tile->mXY.rx(), &tile->mXY.ry());
    tile->mRotation = parseRotation(tileBlock.value("rotation"));
    tile->mVisual = getVisual(tileBlock.value("visual"));
    return tile;
}

bool TileRotationFile::readDirection(const SimpleFileBlock &block, TileRotatedVisualData &direction)
{
    for (const SimpleFileBlock& block1 : block.blocks) {
        QString tileName = block1.value("name");
        QString tilesetName;
        int tileIndex;
        if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileIndex)) {
            mError = tr("Can't parse tile name '%1'").arg(tileName);
            return false;
        }
        QPoint offset;
        if (block1.hasValue("offset")) {
            if (!parse2Ints(block1.value("offset"), &offset.rx(), &offset.ry())) {
                mError = tr("Can't parse offset '%1'").arg(block1.value("offset"));
                return false;
            }
        }
        TileRotatedVisualEdge edge = TileRotatedVisualEdge::None;
        if (block1.hasValue("edge")) {
            QString edgeStr = block1.value("edge");
            for (int i = 0; TileRotatedVisual::EDGE_NAMES[i] != nullptr; i++) {
                if (edgeStr == QLatin1Literal(TileRotatedVisual::EDGE_NAMES[i])) {
                    edge = static_cast<TileRotatedVisualEdge>(i);
                    break;
                }
            }
        }
        direction.addTile(tileName, offset, edge);
    }
    return true;
}

bool TileRotationFile::write(const QString &path, const QList<TilesetRotated *> &tilesets, const QList<QSharedPointer<TileRotatedVisual>>& visuals)
{
    SimpleFile simpleFile;

    SimpleFileBlock visualsBlock;
    visualsBlock.name = QLatin1Literal("visuals");
    QList<QSharedPointer<TileRotatedVisual>> visualsSorted = visuals;
    std::sort(visualsSorted.begin(), visualsSorted.end(), [](auto &a, auto &b) {
        QString tileNameA;
        QString tileNameB;
        for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
            if (tileNameA.isEmpty() && !a->mData[i].mTileNames.isEmpty())
                tileNameA = a->mData[i].mTileNames[0];
            if (tileNameB.isEmpty() && !b->mData[i].mTileNames.isEmpty())
                tileNameB = b->mData[i].mTileNames[0];
            if (!tileNameA.isEmpty() && !tileNameB.isEmpty())
                break;
        }
        return tileNameA < tileNameB;
    });
    for (auto& visual : visualsSorted) {
        SimpleFileBlock visualBlock;
        writeVisual(visual.data(), visualBlock);
        visualsBlock.blocks += visualBlock;
    }
    simpleFile.blocks += visualsBlock;

    for (TilesetRotated *tileset : tilesets) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("name", tileset->name());
        tilesetBlock.addValue("columns", QString::number(tileset->mColumnCount));
        for (TileRotated *tile : tileset->mTiles) {
            if (tile->mVisual == nullptr) {
                continue;
            }
            SimpleFileBlock tileBlock;
            writeTile(tile, tileBlock);
            tilesetBlock.blocks += tileBlock;
        }
        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<TilesetRotated *> Tiled::TileRotationFile::takeTilesets()
{
    QList<TilesetRotated *> result(mTilesets);
    mTilesets.clear();
    return result;
}

QList<QSharedPointer<TileRotatedVisual> > TileRotationFile::takeVisuals()
{
    QList<QSharedPointer<TileRotatedVisual>> result(mVisualLookup.values());
    mVisualLookup.clear();
    return result;
}

//QMap<QString, QString> TileRotationFile::takeMapping()
//{
//    return mMapping;
//}

bool TileRotationFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
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

QString TileRotationFile::twoInts(int a, int b)
{
    return QString(QLatin1Literal("%1,%2")).arg(a).arg(b);
}

void TileRotationFile::writeVisual(TileRotatedVisual *visual, SimpleFileBlock &visualBlock)
{
    visualBlock.name = QLatin1Literal("visual");
    visualBlock.addValue("uuid", visual->mUuid.toString());

    SimpleFileBlock r0Block;
    r0Block.name = QLatin1Literal("R0");
    writeDirection(visual->mData[0], r0Block);
    visualBlock.blocks += r0Block;

    SimpleFileBlock r90Block;
    r90Block.name = QLatin1Literal("R90");
    writeDirection(visual->mData[1], r90Block);
    visualBlock.blocks += r90Block;

    SimpleFileBlock r180Block;
    r180Block.name = QLatin1Literal("R180");
    writeDirection(visual->mData[2], r180Block);
    visualBlock.blocks += r180Block;

    SimpleFileBlock r270Block;
    r270Block.name = QLatin1Literal("R270");
    writeDirection(visual->mData[3], r270Block);
    visualBlock.blocks += r270Block;
}

void TileRotationFile::writeTile(TileRotated *tile, SimpleFileBlock &tileBlock)
{
    tileBlock.name = QLatin1Literal("tile");
    tileBlock.addValue("xy", twoInts(tile->mXY.x(), tile->mXY.y()));
    tileBlock.addValue("rotation", QLatin1Literal(MAP_ROTATION_NAMES[int(tile->mRotation)]));
    tileBlock.addValue("visual", tile->mVisual->mUuid.toString());
}

void TileRotationFile::writeDirection(TileRotatedVisualData &direction, SimpleFileBlock &directionBlock)
{
    for (int i = 0; i < direction.mTileNames.size(); i++) {
        const QString& tileName = direction.mTileNames[i];
        QPoint tileOffset = direction.mOffsets[i];
        TileRotatedVisualEdge edge = direction.mEdges[i];
        SimpleFileBlock block;
        block.name = QLatin1Literal("tile");
        block.addValue("name", tileName);
        if (!tileOffset.isNull()) {
            block.addValue("offset", twoInts(tileOffset.x(), tileOffset.y()));
        }
        if (edge != TileRotatedVisualEdge::None) {
            block.addValue("edge", QLatin1Literal(TileRotatedVisual::EDGE_NAMES[int(edge)]));
        }
        directionBlock.blocks += block;
    }
}

QSharedPointer<TileRotatedVisual> TileRotationFile::getVisual(const QString &uuidStr)
{
    if (mVisualLookup.contains(uuidStr)) {
        return mVisualLookup[uuidStr];
    }
    // This should never happen.
    QSharedPointer<TileRotatedVisual> visual = QSharedPointer<TileRotatedVisual>::create();
    visual->mUuid = uuidStr;
    Q_ASSERT(visual->mUuid.toString() == uuidStr);
    mVisualLookup[uuidStr] = visual;
    return visual;
}
