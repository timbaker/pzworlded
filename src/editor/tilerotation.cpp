/*
 * tilerotation.cpp
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

#include "tilerotation.h"

#include "tilerotationfile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QDebug>
#include <QPainter>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

// // // // //

namespace Tiled
{

const char *TileRotatedVisual::EDGE_NAMES[] =
{
    "None",
    "Floor",
    "North",
    "East",
    "South",
    "West",
    nullptr
};

class TileRotationPrivate
{
public:
    TileRotationPrivate(TileRotation& outer)
        : mOuter(outer)
    {

    }

    ZTileRenderOrder edgeToOrder(TileRotatedVisualEdge edge)
    {
        switch (edge)
        {
        case TileRotatedVisualEdge::Floor:
            return ZTileRenderOrder::Floor;
        case TileRotatedVisualEdge::North:
            return ZTileRenderOrder::NorthWall;
        case TileRotatedVisualEdge::East:
            return ZTileRenderOrder::EastWall;
        case TileRotatedVisualEdge::South:
            return ZTileRenderOrder::SouthWall;
        case TileRotatedVisualEdge::West:
            return ZTileRenderOrder::WestWall;
        default:
            return ZTileRenderOrder::West;
        }
    }
#if 0
    void fileLoaded(TileRotated *tile)
    {
        // One or more tiles share the same TileRotatedVisual, don't do this more than once per TileRotatedVisual.
        TileRotatedVisual* visual = tile->mVisual.data();
        for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
            for (int j = 0; j < visual->mData[i].mTileNames.size(); j++) {
                const QString& tileName = visual->mData[i].mTileNames[j];
                ZTileRenderInfo renderInfo;
                renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
                renderInfo.mOffset = visual->mData[i].pixelOffset(j);
                renderInfo.mOrder = edgeToOrder(visual->mData[i].mEdges[j]);
                visual->mData[i].mRenderInfo += renderInfo;
            }
        }
    }

    void fileLoaded()
    {
        QSet<TileRotatedVisual*> doneVisuals;
        for (TilesetRotated *tileset : mTilesets) {
            for (TileRotated *tile : tileset->mTiles) {
                if (doneVisuals.contains(tile->mVisual.data())) {
                    continue;
                }
                doneVisuals += tile->mVisual.data();
                fileLoaded(tile);
            }
        }
    }
#endif
    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x());
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1);
        }
    }

    QPoint unrotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(pos.y(), height - pos.x() - 1); // ok
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(width - pos.y() - 1, pos.x());
        }
    }

    bool isNone(Tile *tile)
    {
        return tile == nullptr || tile == BuildingTilesMgr::instance()->noneTiledTile();
    }

    MapRotation rotation[4] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    void tempLazyInit()
    {
//      initFromBuildingTiles();
        TileRotationFile file;
        if (file.read(QLatin1String("D:\\pz\\TileRotation.txt"))) {
            mTilesetRotatedList = file.takeTilesets();
            mVisuals = file.takeVisuals();
            mTilesetByName.clear();
            for (TilesetRotated *tilesetR : qAsConst(mTilesetRotatedList)) {
                mTilesetByName[tilesetR->name()] = tilesetR;
            }
            initRenderInfo(mVisuals);
        }
    }

    void rotateTile(const Cell &cell, MapRotation viewRotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
    {
        if (cell.isEmpty()) {
            return;
        }
        Tile *tile = cell.tile;

        if ((viewRotation == MapRotation::NotRotated) && (cell.rotation == MapRotation::NotRotated)) {
            tileInfos += ZTileRenderInfo(tile);
            return;
        }

        // FIXME: temporary lazy init
        if (mTilesetRotatedList.isEmpty()) {
            tempLazyInit();
        }

        // This is a real tile in an unrotated view.
        if (viewRotation == MapRotation::NotRotated) {
//            tileInfos += ZTileRenderInfo(tile);
//            return;
        }

        QString tilesetName = tile->tileset()->name();
        if (mTilesetByName.contains(tilesetName)) {
            TilesetRotated* tilesetR = mTilesetByName[tilesetName];
            if (TileRotated *tileR = tilesetR->tileAt(tile->id())) {
                if (tileR->mVisual != nullptr) {
                    int viewRotationInt = int(viewRotation);
                    int cellRotationInt = int(cell.rotation);
                    int m2 = (viewRotationInt + cellRotationInt + int(tileR->mRotation)) % MAP_ROTATION_COUNT;
                    tileInfos += tileR->mVisual->mData[m2].mRenderInfo;
                    return;
                }
            }
        }

        if (viewRotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
        }
    }


    QSharedPointer<TileRotatedVisual> allocVisual()
    {
        QSharedPointer<TileRotatedVisual> visual(new TileRotatedVisual());
        visual->mUuid = QUuid::createUuid();
//        mVisualLookup[visual->mUuid] = visual;
        return visual;
    }

    void initRenderInfo(const QList<QSharedPointer<TileRotatedVisual> > &visuals)
    {
        for (auto& visual : visuals) {
            for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
                for (int j = 0; j < visual->mData[i].mTileNames.size(); j++) {
                    const QString& tileName = visual->mData[i].mTileNames[j];
                    ZTileRenderInfo renderInfo;
                    renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
                    renderInfo.mOffset = visual->mData[i].pixelOffset(j);
                    renderInfo.mOrder = edgeToOrder(visual->mData[i].mEdges[j]);
                    visual->mData[i].mRenderInfo += renderInfo;
                }
            }
        }
    }

    bool hasTileRotated(const QString &tilesetName, int tileID)
    {
        if (mTilesetByName.contains(tilesetName)) {
            if (TileRotated *tileR = mTilesetByName[tilesetName]->tileAt(tileID)) {
                return tileR->mVisual != nullptr;
            }
            return false;
        }
        return false;
    }

    void reload()
    {
        qDeleteAll(mTilesetRotatedList);
        mTilesetRotatedList.clear();
        mTilesetByName.clear();
    }

    TileRotation& mOuter;
    QList<TilesetRotated*> mTilesetRotatedList;
    QMap<QString, TilesetRotated*> mTilesetByName;
    QList<QSharedPointer<TileRotatedVisual>> mVisuals;
};

// // // // //

TilesetRotated::~TilesetRotated()
{
    qDeleteAll(mTiles);
    mTiles.clear();
}

TileRotated *TilesetRotated::createTile(int tileID)
{
    if (TileRotated *tileR = tileAt(tileID)) {
        return tileR;
    }
    TileRotated* tileR = new TileRotated();
    tileR->mTileset = this;
    tileR->mID = tileID;
    tileR->mXY = QPoint(tileID % mColumnCount, tileID / mColumnCount);
    mTiles += tileR;
    if (tileID >= mTileByID.size()) {
        mTileByID.resize(tileID + 1);
    }
    mTileByID[tileID] = tileR;
    return tileR;
}

QString TileRotated::name() const
{
    return BuildingTilesMgr::instance()->nameForTile(mTileset->name(), mID);
}

// namespace Tiled
}

// // // // //

TileRotation *TileRotation::mInstance = nullptr;

TileRotation *TileRotation::instance()
{
    if (mInstance == nullptr) {
        mInstance = new TileRotation();
    }
    return mInstance;
}

void TileRotation::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

TileRotation::TileRotation()
    : mPrivate(new TileRotationPrivate(*this))
{

}

void TileRotation::readFile(const QString &filePath)
{
    TileRotationFile file;
    if (!file.read(filePath)) {
        return;
    }
    qDeleteAll(mPrivate->mTilesetRotatedList);
    mPrivate->mTilesetRotatedList = file.takeTilesets();
    mPrivate->mVisuals = file.takeVisuals();
    mPrivate->mTilesetByName.clear();
    for (TilesetRotated *tilesetR : qAsConst(mPrivate->mTilesetRotatedList)) {
        mPrivate->mTilesetByName[tilesetR->name()] = tilesetR;
    }
    mPrivate->initRenderInfo(mPrivate->mVisuals);
}

QSharedPointer<TileRotatedVisual> TileRotation::allocVisual()
{
    return mPrivate->allocVisual();
}

void TileRotation::initRenderInfo(const QList<QSharedPointer<TileRotatedVisual> > &visuals)
{
    mPrivate->initRenderInfo(visuals);
}

bool TileRotation::hasTileRotated(const QString &tilesetName, int tileID)
{
    return mPrivate->hasTileRotated(tilesetName, tileID);
}

void TileRotation::rotateTile(const Cell &cell, MapRotation viewRotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
{
    mPrivate->rotateTile(cell, viewRotation, tileInfos);
}

void TileRotation::reload()
{
    mPrivate->reload();
}

MapRotation TileRotation::unrotateTile(const QString &tileName, MapRotation viewRotation)
{
    Q_UNUSED(tileName)
    switch (viewRotation) {
    case MapRotation::NotRotated:
        return MapRotation::NotRotated;
    case MapRotation::Clockwise90:
        return MapRotation::Clockwise270;
    case MapRotation::Clockwise180:
        return MapRotation::Clockwise180;
    case MapRotation::Clockwise270:
        return MapRotation::Clockwise90;
    }
}
