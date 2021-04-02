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

#include "mapcomposite.h"

#include "bmpblender.h"
#include "mapmanager.h"
#include "tilerotation.h"
#include "tilesetmanager.h"

#include "maplevel.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#ifdef WORLDED
#define MAX_WORLD_LEVELS 8
#endif

using namespace Tiled;

static void maxMargins(const QMargins &a,
                       const QMargins &b,
                       QMargins &out)
{
    out.setLeft(qMax(a.left(), b.left()));
    out.setTop(qMax(a.top(), b.top()));
    out.setRight(qMax(a.right(), b.right()));
    out.setBottom(qMax(a.bottom(), b.bottom()));
}

static void unionTileRects(const QRect &a,
                           const QRect &b,
                           QRect &out)
{
    if (a.isEmpty())
        out = b;
    else if (b.isEmpty())
        out = a;
    else
        out = a | b;
}

static void unionSceneRects(const QRectF &a,
                           const QRectF &b,
                           QRectF &out)
{
    if (a.isEmpty())
        out = b;
    else if (b.isEmpty())
        out = a;
    else
        out = a | b;
}

QString MapComposite::layerNameWithoutPrefix(const QString &name)
{
    int pos = name.indexOf(QLatin1Char('_')) + 1; // Could be "-1 + 1 == 0"
    return name.mid(pos);
}

QString MapComposite::layerNameWithoutPrefix(Layer *layer)
{
    return layerNameWithoutPrefix(layer->name());
}

///// ///// ///// ///// /////

CompositeLayerGroup::SubMapLayers::SubMapLayers(MapComposite *subMap,
                                                CompositeLayerGroup *layerGroup)
    : mSubMap(subMap)
    , mLayerGroup(layerGroup)
    , mBounds(layerGroup->bounds().translated(subMap->origin()))
{
}

///// ///// ///// ///// /////

CompositeLayerGroup::CompositeLayerGroup(MapComposite *owner, int level)
    : ZTileLayerGroup(owner->map(), level)
    , mOwner(owner)
    , mAnyVisibleLayers(false)
    , mNeedsSynch(true)
    , mNoBlendCell(Tiled::Internal::TilesetManager::instance()->noBlendTile())

{

}

void CompositeLayerGroup::addTileLayer(TileLayer *layer, int index)
{
#ifndef WORLDED
    // Hack -- only a map being edited can set a TileLayer's group.
    ZTileLayerGroup *oldGroup = layer->group();
#endif
    ZTileLayerGroup::addTileLayer(layer, index);
#ifndef WORLDED
    if (!mOwner->mapInfo()->isBeingEdited())
        layer->setGroup(oldGroup);
#endif

    // Remember the names of layers (without the N_ prefix)
    const QString name = layer->name(); // MapComposite::layerNameWithoutPrefix(layer);
    mLayersByName[name].append(layer);

    index = mLayers.indexOf(layer);
    mVisibleLayers.insert(index, layer->isVisible());
    mLayerOpacity.insert(index, mOwner->mapInfo()->isBeingEdited()
                         ? layer->opacity() : 1.0f);

    // To optimize drawing of submaps, remember which layers are totally empty.
    // But don't do this for the top-level map (the one being edited).
    // TileLayer::isEmpty() is SLOW, it's why I'm caching it.
    bool empty = mOwner->mapInfo()->isBeingEdited()
            ? false
            : layer->isEmpty() || layer->name().contains(QLatin1String("NoRender"));
    mEmptyLayers.insert(index, empty);

    mBmpBlendLayers.insert(index, nullptr);
    mNoBlends.insert(index, nullptr);
#ifdef BUILDINGED
    mBlendOverLayers.insert(index, nullptr);
    mToolLayers.insert(index, ToolLayer());
    mToolNoBlends.insert(index, ToolNoBlend());
    mForceNonEmpty.insert(index, false);
#endif // BUILDINGED
}

void CompositeLayerGroup::removeTileLayer(TileLayer *layer)
{
    int index = mLayers.indexOf(layer);
    mVisibleLayers.remove(index);
    mLayerOpacity.remove(index);
    mEmptyLayers.remove(index);
    mBmpBlendLayers.remove(index);
    mNoBlends.remove(index);
#ifdef BUILDINGED
    mBlendOverLayers.remove(index);
    mToolLayers.remove(index);
    mToolNoBlends.remove(index);
    mForceNonEmpty.remove(index);
#endif // BUILDINGED
#ifndef WORLDED
    // Hack -- only a map being edited can set a TileLayer's group.
    ZTileLayerGroup *oldGroup = layer->group();
#endif
    ZTileLayerGroup::removeTileLayer(layer);
#ifndef WORLDED
    if (!mOwner->mapInfo()->isBeingEdited())
        layer->setGroup(oldGroup);
#endif

    const QString name = layer->name(); // MapComposite::layerNameWithoutPrefix(layer);
    index = mLayersByName[name].indexOf(layer);
    mLayersByName[name].remove(index);
}

void CompositeLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedSubMapLayers.resize(0);
    if (mAnyVisibleLayers == false)
        return;
    for (const SubMapLayers &subMapLayer : mVisibleSubMapLayers) {
        CompositeLayerGroup *layerGroup = subMapLayer.mLayerGroup;
        if (subMapLayer.mSubMap->isHiddenDuringDrag())
            continue;
        QRectF bounds = layerGroup->boundingRect(renderer);
        if ((bounds & rect).isValid()) {
            mPreparedSubMapLayers.append(subMapLayer);
            layerGroup->prepareDrawing(renderer, rect);
        }
    }
    if (level() == 0 && mOwner->bmpBlender())
        mOwner->bmpBlender()->flush(renderer, rect, mOwner->originRecursive());
}

static QLatin1String sFloor("Floor"); // FIXME: thread safe?
static QLatin1String sAboveLot("_AboveLot");

bool CompositeLayerGroup::orderedCellsAt(const Tiled::MapRenderer *renderer,
                                         const QPoint &pos,
                                         QVector<const Cell *> &cells,
                                         QVector<qreal> &opacities) const
{
    MapComposite *root = mOwner->rootOrAdjacent();
    if (root == mOwner)
        root->mKeepFloorLayerCount = 0;

    QRegion suppressRgn;
    if (mOwner->levelRecursive() + level() == mOwner->root()->suppressLevel())
        suppressRgn = mOwner->root()->suppressRegion();
    const QPoint rootPos = pos + mOwner->originRecursive();

    QVector<const Cell*> aboveLotCells;
    QVector<qreal> aboveLotOpacities;

    bool cleared = false;
    const Cell emptyCell;
    for (int index = 0; index < mLayers.size(); index++) {
        if (isLayerEmpty(index))
            continue;
        const TileLayer *tl = mLayers[index];
        const QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel - tl->position();
        if (!tl->contains(subPos))
            continue;
        const TileLayer *tlBmpBlend = mBmpBlendLayers[index];
        const MapNoBlend *noBlend = mNoBlends[index];
#ifdef BUILDINGED
        const TileLayer *tlBlendOver = mBlendOverLayers[index];
        const TileLayer *tlTool = mToolLayers[index].mLayer;
        const ToolNoBlend &nbTool = mToolNoBlends[index];
        QPoint nbPos;
        if (nbTool.mRegion.contains(subPos)) {
            noBlend = &nbTool.mNoBlend;
            nbPos = nbTool.mPos;
        }
#endif // BUILDINGED
        const Cell *cell = &tl->cellAt(subPos);
        if (!mOwner->parent() && !mOwner->showMapTiles())
            cell = &emptyCell;
        if (mOwner->parent() != nullptr && mOwner->parent()->showLotFloorsOnly()) {
            bool isFloor = !mLevel && !index && (tl->name() == sFloor);
            if (!isFloor && !tl->name().contains(sAboveLot)) {
                cell = &emptyCell;
            }
        }
        if (tlBmpBlend && tlBmpBlend->contains(subPos) && !tlBmpBlend->cellAt(subPos).isEmpty())
            if (mOwner->parent() || mOwner->showBMPTiles()) {
                if (!noBlend || !noBlend->get(subPos - nbPos))
                    cell = &tlBmpBlend->cellAt(subPos);
            }
#ifdef BUILDINGED
        // Use an empty tool tile if given during erasing.
        if (tlTool && mToolLayers[index].mRegion.contains(subPos) &&
                tlTool->contains(subPos - mToolLayers[index].mPos))
            cell = &tlTool->cellAt(subPos - mToolLayers[index].mPos);
        else if (cell->isEmpty() && tlBlendOver && tlBlendOver->contains(subPos))
            cell = &tlBlendOver->cellAt(subPos);
#endif // BUILDINGED
        if (index && suppressRgn.contains(rootPos))
            cell = &emptyCell;
        if (!cell->isEmpty() && (root == mOwner) && tl->name().contains(sAboveLot)) {
            aboveLotCells += cell;
            aboveLotOpacities += mLayerOpacity[index];
            cell = &emptyCell;
        }
        if (!cell->isEmpty()) {
            if (!cleared) {
                bool isFloor = !mLevel && !index && (tl->name() == sFloor);
                if (isFloor) root->mKeepFloorLayerCount = 0;
                cells.resize(root->mKeepFloorLayerCount);
                opacities.resize(root->mKeepFloorLayerCount);
                cleared = true;
            }
            cells.append(cell);
#if 1
            opacities.append(mLayerOpacity[index]);
#else
            if (mHighlightLayer.isEmpty() || tl->name() == mHighlightLayer)
                opacities.append(mLayerOpacity[index]);
            else
                opacities.append(0.25);
#endif
            if (mMaxFloorLayer >= index)
                mOwner->mKeepFloorLayerCount = cells.size();
        }

        // Draw the no-blend tile.
        if (noBlend && tl->name() == mOwner->mNoBlendLayer && noBlend->get(subPos - nbPos)) {
            if (!cleared) {
                bool isFloor = !mLevel && !index && (tl->name() == sFloor);
                if (isFloor) root->mKeepFloorLayerCount = 0;
                cells.resize(root->mKeepFloorLayerCount);
                opacities.resize(root->mKeepFloorLayerCount);
                cleared = true;
            }
            cells.append(&mNoBlendCell);
            opacities.append(0.25);
            if (mMaxFloorLayer >= index)
                mOwner->mKeepFloorLayerCount = cells.size();
        }
    }

    // Overwrite map cells with sub-map cells at this location.
    // Chop off sub-map cells that aren't in the root- or adjacent-map's bounds.
    QRect rootBounds(root->originRecursive(), root->mapInfo()->size());
    bool inRoot = (rootBounds.size() != QSize(300, 300)) || rootBounds.contains(rootPos);
    for (const SubMapLayers& subMapLayer : mPreparedSubMapLayers) {
        if (!inRoot && !subMapLayer.mSubMap->isAdjacentMap())
            continue;
        if (!subMapLayer.mBounds.contains(pos))
            continue;
        subMapLayer.mLayerGroup->orderedCellsAt(renderer, pos - subMapLayer.mSubMap->origin(),
                                                cells, opacities);
    }

    cells += aboveLotCells;
    opacities += aboveLotOpacities;

    return !cells.isEmpty();
}

bool CompositeLayerGroup::orderedTilesAt(const MapRenderer *renderer, const QPoint &point,
                                         QVector<ZTileRenderInfo> &tileInfos) const
{
    QVector<const Cell *> cells(40);
    QVector<qreal> opacities(40);
    cells.resize(0);
    opacities.resize(0);
    orderedCellsAt(renderer, point, cells, opacities);

    tileInfos.clear();

    for (int i = 0; i < cells.size(); i++) {
        const Cell* cell = cells[i];
        if (cell->isEmpty())
            continue;
        int j = tileInfos.size();
        TileRotation::instance()->rotateTile(*cells[i], renderer->rotation(), tileInfos);
        for (; j < tileInfos.size(); j++) {
            tileInfos[j].mOpacity = opacities[i];
        }
    }

    sortForRendering(renderer, tileInfos);

    return !tileInfos.isEmpty();
}

void CompositeLayerGroup::prepareDrawing2()
{
    mPreparedSubMapLayers.resize(0);
    for (MapComposite *subMap : mOwner->subMaps()) {
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup) {
            mPreparedSubMapLayers.append(SubMapLayers(subMap, layerGroup));
            layerGroup->prepareDrawing2();
        }
    }
    if (level() == 0 && mOwner->bmpBlender())
        mOwner->bmpBlender()->flush(bounds());
}

// This is for the benefit of LotFilesManager.  It ignores the visibility of
// layers (so NoRender layers are included) and visibility of sub-maps.
bool CompositeLayerGroup::orderedCellsAt2(const QPoint &pos, QVector<const Cell *> &cells) const
{
    MapComposite *root = mOwner->root();
    if (root == mOwner)
        root->mKeepFloorLayerCount = 0;

    QVector<const Cell*> aboveLotCells;

    bool cleared = false;
    int index = -1;
    for (TileLayer *tl : mLayers) {
        ++index;
        TileLayer *tlBmpBlend = mBmpBlendLayers[index];
        MapNoBlend *noBlend = mNoBlends[index];
#ifdef BUILDINGED
        const TileLayer *tlBlendOver = mBlendOverLayers[index];
#endif // BUILDINGED
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel;
        if (tl->contains(subPos)) {
            const Cell *cell = &tl->cellAt(subPos);
            if (tlBmpBlend && tlBmpBlend->contains(subPos) && !tlBmpBlend->cellAt(subPos).isEmpty()) {
                if (!noBlend || !noBlend->get(subPos)) {
                    cell = &tlBmpBlend->cellAt(subPos);
                }
            }
#ifdef BUILDINGED
            if (cell->isEmpty() && tlBlendOver && tlBlendOver->contains(subPos)) {
                cell = &tlBlendOver->cellAt(subPos);
            }
#endif // BUILDINGED
            if (!cell->isEmpty() && (root == mOwner) && tl->name().contains(sAboveLot)) {
                aboveLotCells += cell;
                continue;
            }
            if (!cell->isEmpty()) {
                if (!cleared) {
                    bool isFloor = !mLevel && !index && (tl->name() == sFloor);
                    if (isFloor) root->mKeepFloorLayerCount = 0;
                    cells.resize(root->mKeepFloorLayerCount);
                    cleared = true;
                }
                cells.append(cell);
                if (mMaxFloorLayer >= index)
                    mOwner->mKeepFloorLayerCount = cells.size();
            }
        }
    }

    // Overwrite map cells with sub-map cells at this location
    for (const SubMapLayers& subMapLayer : mPreparedSubMapLayers) {
        if (!subMapLayer.mBounds.contains(pos))
            continue;
        subMapLayer.mLayerGroup->orderedCellsAt2(pos - subMapLayer.mSubMap->origin(), cells);
    }

    cells += aboveLotCells;

    return !cells.isEmpty();
}

#ifdef WORLDED
void CompositeLayerGroup::prepareDrawingNoBmpBlender(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedSubMapLayers.resize(0);
    for (MapComposite *subMap : mOwner->subMaps()) {
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup == nullptr) {
            continue;
        }
        QRectF bounds = layerGroup->boundingRect(renderer);
        if ((bounds & rect).isValid()) {
            mPreparedSubMapLayers.append(SubMapLayers(subMap, layerGroup));
            layerGroup->prepareDrawingNoBmpBlender(renderer, rect);
        }
    }
}
#endif

bool CompositeLayerGroup::isLayerEmpty(int index) const
{
    if (!mVisibleLayers[index])
        return true;
    if (mBmpBlendLayers[index] && !mBmpBlendLayers[index]->isEmpty())
        return false;
#if 1
    // There could be no user-drawn tiles, only blender tiles.
    if (mLevel == 0) return false; // FIXME: check for non-empty BMP images
#endif
#ifdef BUILDINGED
    if (mForceNonEmpty[index])
        return false;
    if (mBlendOverLayers[index] && !mBlendOverLayers[index]->isEmpty())
        return false;
    if (mToolLayers[index].mLayer && !mToolLayers[index].mRegion.isEmpty())
        return false;
    if (!mToolNoBlends[index].mRegion.isEmpty())
        return false;
#endif // BUILDINGED
#if SPARSE_TILELAYER
    // Checking isEmpty() and mEmptyLayers to catch hidden NoRender layers in submaps.
    return mEmptyLayers[index] || mLayers[index]->isEmpty();
#else
    // TileLayer::isEmpty() is very slow.
    // Checking mEmptyLayers only catches hidden NoRender layers.
    // The actual tile layer might be empty.
    return mEmptyLayers[index]);
#endif
}

void CompositeLayerGroup::synch()
{
    mMaxFloorLayer = -1;
    if (!isVisible()) {
        mAnyVisibleLayers = false;
        mTileBounds = QRect();
        mSubMapTileBounds = QRect();
        mDrawMargins = QMargins(0, mOwner->map()->tileHeight(), mOwner->map()->tileWidth(), 0);
        mVisibleSubMapLayers.clear();
#ifdef BUILDINGED
        mBlendOverLayers.fill(nullptr);
#endif
        mNeedsSynch = false;
        return;
    }

    QRect r;
    // See TileLayer::drawMargins()
    QMargins m(0, mOwner->map()->tileHeight(), mOwner->map()->tileWidth(), 0);

    mAnyVisibleLayers = false;

    for (int i = 0; i < mLayers.size(); i++) {
        if (!mVisibleLayers[i])
            continue;
        if (mBmpBlendLayers[i] && !mBmpBlendLayers[i]->isEmpty()) {
            unionTileRects(r, mBmpBlendLayers[i]->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
            maxMargins(m, mBmpBlendLayers[i]->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
#ifdef BUILDINGED
        if (mToolLayers[i].mLayer && !mToolLayers[i].mRegion.isEmpty()) {
            unionTileRects(r, mLayers[i]->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
            maxMargins(m, mLayers[i]->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
#endif
    }

#ifdef BUILDINGED
    // Do this before the isLayerEmpty() call below.
    mBlendOverLayers.fill(nullptr);
    if (MapComposite *blendOverMap = mOwner->blendOverMap()) {
        if (CompositeLayerGroup *layerGroup = blendOverMap->tileLayersForLevel(mLevel)) {
            for (int i = 0; i < mLayers.size(); i++) {
                if (!mVisibleLayers[i])
                    continue;
                for (int j = 0; j < layerGroup->mLayers.size(); j++) {
                    TileLayer *blendLayer = layerGroup->mLayers[j];
                    if (blendLayer->name() == mLayers[i]->name()) {
                        mBlendOverLayers[i] = blendLayer;
                        if (!blendLayer->isEmpty()) {
                            unionTileRects(r, blendLayer->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
                            maxMargins(m, blendLayer->drawMargins(), m);
                            mAnyVisibleLayers = true;
                        }
                        break;
                    }
                }
            }
        }
    }
#endif

    // Set the visibility and opacity of this group's layers to match the root
    // map's layer-group's layers. Layers without a matching name in the root map
    // are always shown at full opacity.
    // Note that changing the opacity of a layer is *not* a reason for calling
    // synch() again.
    if (mOwner->parent()) {
        for (int index = 0; index < mLayers.size(); index++) {
            mVisibleLayers[index] = true;
            mLayerOpacity[index] = 1.0;
        }
        CompositeLayerGroup *rootGroup = mOwner->root()->layerGroupForLevel(mOwner->levelRecursive() + mLevel);
        if (rootGroup) {
            // FIXME: this doesn't properly handle multiple layers with the same name.
            for (int rootIndex = 0; rootIndex < rootGroup->mLayers.size(); rootIndex++) {
                QString layerName = rootGroup->mLayers[rootIndex]->name();
                const QString name = layerName; // MapComposite::layerNameWithoutPrefix(layerName);
                if (!mLayersByName.contains(name))
                    continue;
                for (Layer *layer : mLayersByName[name]) {
                    int index = mLayers.indexOf(layer->asTileLayer());
                    Q_ASSERT(index != -1);
                    mVisibleLayers[index] = rootGroup->mVisibleLayers[rootIndex];
                    mLayerOpacity[index] = rootGroup->mLayerOpacity[rootIndex];
                }
            }
        }
    }

    int index = 0;
    for (TileLayer *tl : mLayers) {
        if (!isLayerEmpty(index)) {
            unionTileRects(r, tl->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
            maxMargins(m, tl->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
        if (!mLevel && (!mOwner->parent() || mOwner->isAdjacentMap()) &&
                (index == mMaxFloorLayer + 1) &&
                tl->name().startsWith(sFloor)) {
            mMaxFloorLayer = index;
        }
        ++index;
    }

    mTileBounds = r;

    r = QRect();
    mVisibleSubMapLayers.resize(0);

    for (MapComposite *subMap : mOwner->subMaps()) {
        if (!subMap->isGroupVisible() || !subMap->isVisible())
            continue;
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup) {
            layerGroup->synch();
            if (layerGroup->mAnyVisibleLayers) {
                mVisibleSubMapLayers.append(SubMapLayers(subMap, layerGroup));
                unionTileRects(r, layerGroup->bounds().translated(subMap->origin()), r);
                maxMargins(m, layerGroup->drawMargins(), m);
                mAnyVisibleLayers = true;
            }
        }
    }

#if defined(BUILDINGED) && !defined(WORLDED)
    if (mAnyVisibleLayers)
        maxMargins(m, QMargins(0, 128, 64, 0), m);
#endif

    mSubMapTileBounds = r;
    mDrawMargins = m;

    mNeedsSynch = false;
}

void CompositeLayerGroup::saveVisibility()
{
    mSavedVisibleLayers = mVisibleLayers;
}

void CompositeLayerGroup::restoreVisibility()
{
    mVisibleLayers = mSavedVisibleLayers;
}

void CompositeLayerGroup::saveOpacity()
{
    mSavedOpacity = mLayerOpacity;
}

void CompositeLayerGroup::restoreOpacity()
{
    mLayerOpacity = mSavedOpacity;
}

bool CompositeLayerGroup::setBmpBlendLayers(const QList<TileLayer *> &layers)
{
    QVector<TileLayer*> old = mBmpBlendLayers;

    mBmpBlendLayers.fill(nullptr);
    foreach (TileLayer *tl, layers) {
        for (int i = 0; i < mLayers.size(); i++) {
            if (mLayers[i]->name() == tl->name()) {
                mBmpBlendLayers[i] = tl;
                if (mOwner->bmpBlender()->blendLayers().contains(tl->name()))
                    mNoBlends[i] = mMap->noBlend(tl->name());
            }
        }
    }

    return old != mBmpBlendLayers;
}

#ifdef BUILDINGED
bool CompositeLayerGroup::setLayerNonEmpty(const QString &layerName, bool force)
{
    const QString name = layerName; // MapComposite::layerNameWithoutPrefix(layerName);
    if (!mLayersByName.contains(name))
        return false;
    foreach (Layer *layer, mLayersByName[name])
        setLayerNonEmpty(layer->asTileLayer(), force);
    return mNeedsSynch;
}

bool CompositeLayerGroup::setLayerNonEmpty(TileLayer *tl, bool force)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    if (force != mForceNonEmpty[index]) {
        mForceNonEmpty[index] = force;
        mNeedsSynch = true;
    }
    return mNeedsSynch;
}

static ZTileRenderOrder viewRenderOrder(ZTileRenderOrder unRotatedOrder, MapRotation viewRotation)
{
    if (true)
        return unRotatedOrder;

    int vi = int(viewRotation);
    int ui = -1;

    switch (unRotatedOrder) {
    case ZTileRenderOrder::NorthWall:
        ui = int(MapRotation::NotRotated);
        break;
    case ZTileRenderOrder::EastWall:
        ui = int(MapRotation::Clockwise90);
        break;
    case ZTileRenderOrder::SouthWall:
        ui = int(MapRotation::Clockwise180);
        break;
    case ZTileRenderOrder::WestWall:
        ui = int(MapRotation::Clockwise270);
        break;
    default:
        return unRotatedOrder; // West
    }

    int i = (vi + ui) % MAP_ROTATION_COUNT;
    switch (i) {
    case 0:
        return ZTileRenderOrder::NorthWall;
    case 1:
        return ZTileRenderOrder::EastWall;
    case 2:
        return ZTileRenderOrder::SouthWall;
    case 3:
        return ZTileRenderOrder::WestWall;
    default:
        Q_ASSERT(false);
        return unRotatedOrder;
    }
}

void CompositeLayerGroup::sortForRendering(const MapRenderer *renderer, QVector<ZTileRenderInfo> &tileInfo) const
{
//    if (renderer->rotation() == MapRotation::NotRotated) {
//        return;
//    }

    // We know which edge each tile is on in the *un-rotated* view.


    int size = tileInfo.size();
    QVector<ZTileRenderInfo> sorted(size);
    sorted.resize(0);

    // Floors Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::Floor:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // West Wall Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::WestWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // North Wall Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::NorthWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // Non-walls
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::West:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // East Wall Highest -> lowest
    for (int i = size - 1; i >= 0; i--) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::EastWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // South Wall Highest -> lowest
    for (int i = size - 1; i >= 0; i--) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (viewRenderOrder(tri.mOrder, renderer->rotation())) {
        case ZTileRenderOrder::SouthWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    tileInfo = sorted;
}
#endif // BUILDINGED

QRect CompositeLayerGroup::bounds() const
{
    QRect bounds;
    unionTileRects(mTileBounds, mSubMapTileBounds, bounds);
    return bounds;
}

QMargins CompositeLayerGroup::drawMargins() const
{
    return mDrawMargins;
}

bool CompositeLayerGroup::setLayerVisibility(const QString &layerName, bool visible)
{
    const QString name = MapComposite::layerNameWithoutPrefix(layerName);
    if (!mLayersByName.contains(name))
        return false;
    foreach (Layer *layer, mLayersByName[name])
        setLayerVisibility(layer->asTileLayer(), visible);
    return mNeedsSynch;
}

bool CompositeLayerGroup::setLayerVisibility(TileLayer *tl, bool visible)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    if (visible != mVisibleLayers[index]) {
        mVisibleLayers[index] = visible;
        mNeedsSynch = true;
    }
    return mNeedsSynch;
}

bool CompositeLayerGroup::isLayerVisible(TileLayer *tl)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    return mVisibleLayers[index];
}

void CompositeLayerGroup::layerRenamed(TileLayer *layer)
{
    QMapIterator<QString,QVector<Layer*> > it(mLayersByName);
    while (it.hasNext()) {
        it.next();
        int index = it.value().indexOf(layer);
        if (index >= 0) {
            mLayersByName[it.key()].remove(index);
//            it.value().remove(index);
            break;
        }
    }

    const QString name = layer->name(); // MapComposite::layerNameWithoutPrefix(layer);
    mLayersByName[name].append(layer);
}

bool CompositeLayerGroup::setLayerOpacity(const QString &layerName, qreal opacity)
{
    const QString name = MapComposite::layerNameWithoutPrefix(layerName);
    if (!mLayersByName.contains(name))
        return false;
    bool changed = false;
    foreach (Layer *layer, mLayersByName[name]) {
        if (setLayerOpacity(layer->asTileLayer(), opacity))
            changed = true;
    }
    return changed;
}

bool CompositeLayerGroup::setLayerOpacity(TileLayer *tl, qreal opacity)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    if (mLayerOpacity[index] != opacity) {
        mLayerOpacity[index] = opacity;
        return true;
    }
    return false;
}

void CompositeLayerGroup::synchSubMapLayerOpacity(const QString &layerName, qreal opacity)
{
    foreach (MapComposite *subMap, mOwner->subMaps()) {
        CompositeLayerGroup *layerGroup =
                subMap->tileLayersForLevel(mLevel - subMap->levelOffset());
        if (layerGroup) {
            layerGroup->setLayerOpacity(layerName, opacity);
            layerGroup->synchSubMapLayerOpacity(layerName, opacity);
        }
    }
}

bool CompositeLayerGroup::regionAltered(Tiled::TileLayer *tl)
{
    QMargins m;
    maxMargins(mDrawMargins, tl->drawMargins(), m);
    if (m != mDrawMargins) {
        setNeedsSynch(true);
        return true;
    }
    int index = mLayers.indexOf(tl);
    if (mTileBounds.isEmpty() && mBmpBlendLayers[index] && !mBmpBlendLayers[index]->isEmpty()) {
        setNeedsSynch(true);
        return true;
    }
#ifdef BUILDINGED
    if (mTileBounds.isEmpty() && mBlendOverLayers[index] && !mBlendOverLayers[index]->isEmpty()) {
        setNeedsSynch(true);
        return true;
    }
#endif
#if SPARSE_TILELAYER
    if (mTileBounds.isEmpty() && !tl->isEmpty()) {
        int index = mLayers.indexOf(tl);
        mEmptyLayers[index] = false;
        setNeedsSynch(true);
        return true;
    }
#endif
    return false;
}

QRectF CompositeLayerGroup::boundingRect(const MapRenderer *renderer) const
{
    if (mNeedsSynch)
        const_cast<CompositeLayerGroup*>(this)->synch();

    QRectF boundingRect = renderer->boundingRect(mTileBounds.translated(mOwner->originRecursive()),
                                                 mLevel + mOwner->levelRecursive());

    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.

    boundingRect.adjust(-mDrawMargins.left(),
                -qMax(0, mDrawMargins.top() - owner()->map()->tileHeight()),
                qMax(0, mDrawMargins.right() - owner()->map()->tileWidth()),
                mDrawMargins.bottom());

    for (const SubMapLayers &subMapLayer : mVisibleSubMapLayers) {
        QRectF bounds = subMapLayer.mLayerGroup->boundingRect(renderer);
        unionSceneRects(boundingRect, bounds, boundingRect);
    }

    return boundingRect;
}

///// ///// ///// ///// /////

// FIXME: If the MapDocument is saved to a new name, this MapInfo should be replaced with a new one
MapComposite::MapComposite(MapInfo *mapInfo, Map::Orientation orientRender,
                           MapComposite *parent, const QPoint &positionInParent,
                           int levelOffset)
    : QObject()
    , mMapInfo(mapInfo)
    , mMap(mapInfo->map())
#ifdef BUILDINGED
    , mBlendOverMap(nullptr)
#endif
    , mParent(parent)
    , mPos(positionInParent)
    , mLevelOffset(levelOffset)
    , mOrientRender(orientRender)
    , mMinLevel(0)
    , mMaxLevel(0)
    , mVisible(true)
    , mGroupVisible(true)
    , mHiddenDuringDrag(false)
    , mShowBMPTiles(true)
    , mShowMapTiles(true)
    , mIsAdjacentMap(false)
    , mBmpBlender(new Tiled::Internal::BmpBlender(mMap, this))
    , mSuppressLevel(0)
{
#ifdef WORLDED
    MapManager::instance()->addReferenceToMap(mMapInfo);
#endif
    if (mOrientRender == Map::Unknown)
        mOrientRender = mMap->orientation();
    if (mMap->orientation() != mOrientRender) {
        Map::Orientation orientSelf = mMap->orientation();
        if (orientSelf == Map::Isometric && mOrientRender == Map::LevelIsometric)
            mOrientAdjustPos = mOrientAdjustTiles = QPoint(3, 3);
        if (orientSelf == Map::LevelIsometric && mOrientRender == Map::Isometric)
            mOrientAdjustPos = mOrientAdjustTiles = QPoint(-3, -3);
    }

    for (int level = 0; level < mMap->levelCount(); level++) {
        int index = 0;
        MapLevel *mapLevel = mMap->levelAt(level);
        for (Layer *layer : mapLevel->layers(Layer::Type::TileLayerType)) {
            TileLayer *tl = layer->asTileLayer();
            if (!mLayerGroups.contains(level))
                mLayerGroups[level] = new CompositeLayerGroup(this, level);
            mLayerGroups[level]->addTileLayer(tl, index);
            if (!mapInfo->isBeingEdited())
                mLayerGroups[level]->setLayerVisibility(tl, !layer->name().contains(QLatin1String("NoRender")));
            ++index;
        }
    }

    // Load lots, but only if this is not the map being edited (that is handled
    // by the LotManager).
    if (!mapInfo->isBeingEdited()) {
        for (ObjectGroup *objectGroup : mMap->objectGroups()) {
            int levelOffset = objectGroup->level();
//            (void) levelForLayer(objectGroup, &levelOffset);
            for (MapObject *object : objectGroup->objects()) {
                if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
#if 1
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(
                                object->type(), QFileInfo(mMapInfo->path()).absolutePath(),
                                true, MapManager::PriorityLow);

                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
#if 1 // FIXME: attempt to load this if mapsDirectory changes
                        subMapInfo = MapManager::instance()->getPlaceholderMap(
                                    object->type(), 32, 32); // FIXME: calculate map size
#endif
                    }
                    if (subMapInfo) {
                        if (subMapInfo->isLoading()) {
                            connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
                                    SLOT(mapLoaded(MapInfo*)), Qt::UniqueConnection);
                            connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
                                    SLOT(mapFailedToLoad(MapInfo*)), Qt::UniqueConnection);
                            mSubMapsLoading += SubMapLoading(subMapInfo,
                                                             object->position().toPoint()
                                                             + mOrientAdjustPos * levelOffset,
                                                             levelOffset);
                        } else
                            addMap(subMapInfo, object->position().toPoint()
                                   + mOrientAdjustPos * levelOffset,
                                   levelOffset, true);
                    }
#else
                    // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                    // then any sub-maps of its own will lose their level offsets.
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(),
                                                                          QFileInfo(mMapInfo->path()).absolutePath());
                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
#ifdef WORLDED // FIXME: attempt to load this if mapsDirectory changes
                        subMapInfo = MapManager::instance()->getPlaceholderMap(object->type(),
                                                                               32, 32); // FIXME: calculate map size
#endif
                    }
                    if (subMapInfo) {
                        int levelOffset;
                        (void) levelForLayer(objectGroup, &levelOffset);
                        addMap(subMapInfo, object->position().toPoint()
                               + mOrientAdjustPos * levelOffset,
                               levelOffset, true);
                    }
#endif
                }
            }
        }
    }

    mMinLevel = 10000;
    mMaxLevel = 0;

    for (CompositeLayerGroup *layerGroup : mLayerGroups) {
        if (!mMapInfo->isBeingEdited())
            layerGroup->synch();
        if (layerGroup->level() < mMinLevel)
            mMinLevel = layerGroup->level();
        if (layerGroup->level() > mMaxLevel)
            mMaxLevel = layerGroup->level();
    }
    for (MapComposite *subMap : mSubMaps)
        if (subMap->mLevelOffset + subMap->mMaxLevel > mMaxLevel)
            mMaxLevel = subMap->mLevelOffset + subMap->mMaxLevel;

    if (mMinLevel == 10000)
        mMinLevel = 0;
#ifdef WORLDED
    if (!mParent && !mMapInfo->isBeingEdited())
        mMaxLevel = qMax(mMaxLevel, MAX_WORLD_LEVELS - 1);
#endif // WORLDED

    mSortedLayerGroups.clear();
    for (int level = mMinLevel; level <= mMaxLevel; ++level) {
        if (!mLayerGroups.contains(level))
            mLayerGroups[level] = new CompositeLayerGroup(this, level);
        mSortedLayerGroups.append(mLayerGroups[level]);
    }

    connect(mBmpBlender, SIGNAL(layersRecreated()), SLOT(bmpBlenderLayersRecreated()));
    mBmpBlender->markDirty(0, 0, mMap->width() - 1, mMap->height() - 1);
    mLayerGroups[0]->setBmpBlendLayers(mBmpBlender->tileLayers());
}

MapComposite::~MapComposite()
{
    qDeleteAll(mSubMaps);
    qDeleteAll(mLayerGroups);
#ifdef WORLDED
    if (mMapInfo)
        MapManager::instance()->removeReferenceToMap(mMapInfo);
#endif
}

bool MapComposite::levelForLayer(const QString &layerName, int *levelPtr)
{
    if (levelPtr) (*levelPtr) = 0;

    // See if the layer name matches "0_foo" or "1_bar" etc.
    QStringList sl = layerName.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        uint level = sl[0].toUInt(&conversionOK);
        if (levelPtr) (*levelPtr) = level;
        return conversionOK;
    }
    return false;
}

bool MapComposite::levelForLayer(Layer *layer, int *levelPtr)
{
    return levelForLayer(layer->name(), levelPtr);
}

MapComposite *MapComposite::addMap(MapInfo *mapInfo, const QPoint &pos,
                                   int levelOffset, bool creating)
{
    MapComposite *subMap = new MapComposite(mapInfo, mOrientRender, this, pos, levelOffset);
    mSubMaps.append(subMap);

    if (creating)
        return subMap;

    ensureMaxLevels(levelOffset + subMap->maxLevel());

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        layerGroup->setNeedsSynch(true);
    }

    MapComposite *mc = this;
    while (mc->mParent) {
        mc->mParent->ensureMaxLevels(mc->levelOffset() + mc->maxLevel());
        // FIXME: setNeedsSynch() on mc->mParent's layergroups
        mc = mc->mParent;
    }

    return subMap;
}

void MapComposite::removeMap(MapComposite *subMap)
{
    Q_ASSERT(mSubMaps.contains(subMap));
    mSubMaps.remove(mSubMaps.indexOf(subMap));
    delete subMap;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->setNeedsSynch(true);
}

void MapComposite::moveSubMap(MapComposite *subMap, const QPoint &pos)
{
    Q_ASSERT(mSubMaps.contains(subMap));
    subMap->setOrigin(pos);

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->setNeedsSynch(true);
}

#ifdef WORLDED
void MapComposite::sortSubMaps(const QVector<MapComposite *> &order)
{
    std::sort(mSubMaps.begin(), mSubMaps.end(), [order,this](MapComposite *a, MapComposite *b) {
        int indexA = order.indexOf(a);
        int indexB = order.indexOf(b);
        if (indexA == -1)
            indexA = mSubMaps.indexOf(a);
        if (indexB == -1)
            indexB = mSubMaps.indexOf(b);
        return indexA < indexB;
    });

    for (CompositeLayerGroup *layerGroup : mLayerGroups) {
        layerGroup->setNeedsSynch(true);
    }
}
#endif

void MapComposite::layerAdded(int z, int index)
{
    layerRenamed(z, index);
}

void MapComposite::layerAboutToBeRemoved(int z, int index)
{
    Layer *layer = mMap->layerAt(z, index);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group()) {
            CompositeLayerGroup *oldGroup = (CompositeLayerGroup*)tl->group();
            emit layerAboutToBeRemovedFromGroup(z, index);
            removeLayerFromGroup(z, index);
            emit layerRemovedFromGroup(z, index, oldGroup);
        }
    }
}

void MapComposite::layerRenamed(int z, int index)
{
    Layer *layer = mMap->layerAt(z, index);

    int oldLevel = layer->level();
    int newLevel = layer->level();
    bool hadGroup = false;
    bool hasGroup = true; // levelForLayer(layer, &newLevel);
    CompositeLayerGroup *oldGroup = nullptr;

    if (TileLayer *tl = layer->asTileLayer()) {
        oldGroup = (CompositeLayerGroup*)tl->group();
        hadGroup = oldGroup != nullptr;
        if (oldGroup)
            oldGroup->layerRenamed(tl);
    }

    if ((oldLevel != newLevel) || (hadGroup != hasGroup)) {
        if (hadGroup) {
            emit layerAboutToBeRemovedFromGroup(z, index);
            removeLayerFromGroup(oldLevel, index);
            emit layerRemovedFromGroup(z, index, oldGroup);
        }
        if (oldLevel != newLevel) {
            layer->setLevel(newLevel);
            emit layerLevelChanged(newLevel, index, oldLevel);
        }
        if (hasGroup && layer->isTileLayer()) {
            addLayerToGroup(newLevel, index);
            emit layerAddedToGroup(newLevel, index);
        }
    }
}

void MapComposite::addLayerToGroup(int z, int index)
{
    Layer *layer = mMap->layerAt(z, index);
    Q_ASSERT(layer->isTileLayer());
//    Q_ASSERT(levelForLayer(layer));
    if (TileLayer *tl = layer->asTileLayer()) {
        int level = tl->level();
        if (!mLayerGroups.contains(level)) {
            mLayerGroups[level] = new CompositeLayerGroup(this, level);

            if (level < mMinLevel)
                mMinLevel = level;
            if (level > mMaxLevel)
                mMaxLevel = level;

            mSortedLayerGroups.clear();
            for (int n = mMinLevel; n <= mMaxLevel; ++n) {
                if (mLayerGroups.contains(n))
                    mSortedLayerGroups.append(mLayerGroups[n]);
            }

            emit layerGroupAdded(level);
        }
        mLayerGroups[level]->addTileLayer(tl, index);
//        tl->setGroup(mLayerGroups[level]);
    }
}

void MapComposite::removeLayerFromGroup(int z, int index)
{
    Layer *layer = mMap->layerAt(z, index);
    Q_ASSERT(layer->isTileLayer());
    if (TileLayer *tl = layer->asTileLayer()) {
#if 1
        // Unused hack for MiniMapScene
        if (!mMapInfo->isBeingEdited()) {
            if (CompositeLayerGroup *layerGroup = tileLayersForLevel(tl->level()))
                layerGroup->removeTileLayer(tl);
            return;
        }
#endif
        Q_ASSERT(tl->group());
        if (CompositeLayerGroup *layerGroup = (CompositeLayerGroup*)tl->group()) {
            layerGroup->removeTileLayer(tl);
//            tl->setGroup(0);
        }
    }
}

CompositeLayerGroup *MapComposite::tileLayersForLevel(int level) const
{
    if (mLayerGroups.contains(level))
        return mLayerGroups[level];
    return nullptr;
}

CompositeLayerGroup *MapComposite::layerGroupForLevel(int level) const
{
    if (mLayerGroups.contains(level))
        return mLayerGroups[level];
    return nullptr;
}

CompositeLayerGroup *MapComposite::layerGroupForLayer(TileLayer *tl) const
{
    if (tl->group())
        return tileLayersForLevel(tl->level());
    return nullptr;
}

QList<MapComposite *> MapComposite::maps()
{
    QList<MapComposite*> ret;
    ret += this;
    foreach (MapComposite *subMap, mSubMaps)
        ret += subMap->maps();
    return ret;
}

void MapComposite::setOrigin(const QPoint &origin)
{
    mPos = origin;
}

QPoint MapComposite::originRecursive() const
{
    return mPos + (mParent ? mParent->originRecursive() : QPoint());
}

int MapComposite::levelRecursive() const
{
    return mLevelOffset + (mParent ? mParent->levelRecursive() : 0);
}

QRectF MapComposite::boundingRect(MapRenderer *renderer, bool forceMapBounds) const
{
    // The reason I'm checking renderer->maxLevel() here is because when drawing
    // map images, I don't want empty levels at the top.

    QRectF bounds;
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        if (levelRecursive() + layerGroup->level() > renderer->maxLevel())
            continue;
        unionSceneRects(bounds,
                        layerGroup->boundingRect(renderer),
                        bounds);
    }
    if (forceMapBounds) {
        QRect mapTileBounds(mPos, mMap->size());

        // Always include level 0, even if there are no layers or only empty/hidden
        // layers on level 0, otherwise a SubMapItem's bounds won't include the
        // fancy rectangle.
        int minLevel = levelRecursive();
        if (minLevel > renderer->maxLevel())
            minLevel = renderer->maxLevel();
        unionSceneRects(bounds,
                        renderer->boundingRect(mapTileBounds, minLevel),
                        bounds);
        // When setting the bounds of the scene, make sure the highest level is included
        // in the sceneRect() so the grid won't be cut off.
        int maxLevel = levelRecursive() + mMaxLevel;
#ifdef WORLDED
        if (mParent /*!mMapInfo->isBeingEdited()*/) {
#else
        if (!mMapInfo->isBeingEdited()) {
#endif
            maxLevel = levelRecursive();
            foreach (CompositeLayerGroup *layerGroup, mSortedLayerGroups) {
                if (!layerGroup->bounds().isEmpty())
                    maxLevel = levelRecursive() + layerGroup->level();
            }
        }
        if (maxLevel > renderer->maxLevel())
            maxLevel = renderer->maxLevel();
        unionSceneRects(bounds,
                        renderer->boundingRect(mapTileBounds, maxLevel),
                        bounds);
    }
    return bounds;
}

void MapComposite::saveVisibility()
{
    mSavedGroupVisible = mGroupVisible;
    mGroupVisible = true; // hack

    mSavedVisible = mVisible;
    mVisible = true; // hack

    mSavedShowBMPTiles = mShowBMPTiles;
    mSavedShowMapTiles = mShowMapTiles;
    mShowBMPTiles = true;
    mShowMapTiles = true;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->saveVisibility();

    // FIXME: there can easily be multiple instances of the same map,
    // in which case this does unnecessary work.
    foreach (MapComposite *subMap, mSubMaps)
        subMap->saveVisibility();
}

void MapComposite::restoreVisibility()
{
    mGroupVisible = mSavedGroupVisible;
    mVisible = mSavedVisible;
    mShowBMPTiles = mSavedShowBMPTiles;
    mShowMapTiles = mSavedShowMapTiles;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->restoreVisibility();

    foreach (MapComposite *subMap, mSubMaps)
        subMap->restoreVisibility();
}

void MapComposite::saveOpacity()
{
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->saveOpacity();

    foreach (MapComposite *subMap, mSubMaps)
        subMap->saveOpacity();
}

void MapComposite::restoreOpacity()
{
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->restoreOpacity();

    foreach (MapComposite *subMap, mSubMaps)
        subMap->restoreOpacity();
}

void MapComposite::ensureMaxLevels(int maxLevel)
{
    maxLevel = qMax(maxLevel, mMaxLevel);
    if (mMinLevel == 0 && maxLevel < mLayerGroups.size())
        return;

    for (int level = 0; level <= maxLevel; level++) {
        if (!mLayerGroups.contains(level)) {
            mLayerGroups[level] = new CompositeLayerGroup(this, level);

            if (mMinLevel > level)
                mMinLevel = level;
            if (level > mMaxLevel)
                mMaxLevel = level;

            mSortedLayerGroups.clear();
            for (int i = mMinLevel; i <= mMaxLevel; ++i)
                mSortedLayerGroups.append(mLayerGroups[i]);

            emit layerGroupAdded(level);
        }
    }
}

MapComposite::ZOrderList MapComposite::zOrder()
{
    ZOrderList result;

    QVector<int> seenLevels;
    typedef QPair<int,Layer*> LayerPair;
    QMap<CompositeLayerGroup*,QVector<LayerPair> > layersAboveLevel;
    CompositeLayerGroup *previousGroup = nullptr;
    int layerIndex = -1;
    for (Layer *layer : mMap->layers()) {
        ++layerIndex;
        int level = layer->level();
        bool hasGroup = true; // levelForLayer(layer, &level);
        if (layer->isTileLayer()) {
            // The layer may not be in a group yet during renaming.
            if (hasGroup && mLayerGroups.contains(level)) {
                if (!seenLevels.contains(level)) {
                    seenLevels += level;
                    previousGroup = mLayerGroups[level];
                }
                continue;
            }
        }
        // Handle any layers not in a TileLayerGroup.
        // Layers between the first and last in a TileLayerGroup will be displayed above that TileLayerGroup.
        // Layers before the first TileLayerGroup will be displayed below the first TileLayerGroup.
        if (previousGroup)
            layersAboveLevel[previousGroup].append(qMakePair(layerIndex, layer));
        else
            result += ZOrderItem(layer, layerIndex);
    }

    for (CompositeLayerGroup *layerGroup : mSortedLayerGroups) {
        result += ZOrderItem(layerGroup);
        QVector<LayerPair> layers = layersAboveLevel[layerGroup];
        for (LayerPair pair : layers) {
            result += ZOrderItem(pair.second, pair.first);
        }
    }

    return result;
}

// When 2 TileZeds are running, TZA has main map, TZB has lot map, and lot map
// is saved, TZA MapImageManager puts up PROGRESS dialog, causing the scene to
// be redrawn before the MapComposites have been updated.
bool MapComposite::mapAboutToChange(MapInfo *mapInfo)
{
    bool affected = false;
    if (mapInfo == mMapInfo) {
        foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
            while (layerGroup->layerCount())
                layerGroup->removeTileLayer(layerGroup->layers().first());
        }
        affected = true;
    }
    foreach (MapComposite *subMap, mSubMaps) {
        if (subMap->mapAboutToChange(mapInfo)) {
            affected = true;
        }
    }

    if (affected) {
        foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
            layerGroup->setNeedsSynch(true);
        }
    }

    return affected;
}

// Called by MapDocument when MapManager tells it a map changed, either due to
// its file changing on disk or because a building's map was affected by
// changing tilesets.
// Returns true if this map or any sub-map is affected.
bool MapComposite::mapChanged(MapInfo *mapInfo)
{
    if (mapInfo == mMapInfo) {
        recreate();
        return true;
    }

    bool changed = false;
    foreach (MapComposite *subMap, mSubMaps) {
        if (subMap->mapChanged(mapInfo)) {
            ensureMaxLevels(subMap->levelOffset() + subMap->maxLevel());
            if (!changed) {
                foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
                    layerGroup->setNeedsSynch(true);
                changed = true;
            }
        }
    }

    return changed;
}

bool MapComposite::isTilesetUsed(Tileset *tileset, bool recurse)
{
    QList<MapComposite*> maps = recurse ? this->maps() : (QList<MapComposite*>() << this);

    foreach (MapComposite *mc, maps) {
        if (mc->map()->isTilesetUsed(tileset))
            return true;
        foreach (TileLayer *tl, mc->tileLayersForLevel(0)->bmpBlendLayers()) {
            if (tl && tl->usedTilesets().contains(tileset))
                return true;
        }
    }
    return false;
}

QList<Tileset *> MapComposite::usedTilesets()
{
    QSet<Tileset*> usedTilesets;
    foreach (MapComposite *mc, maps()) {
        usedTilesets += mc->map()->usedTilesets();
        foreach (TileLayer *tl, mc->mBmpBlender->tileLayers())
            usedTilesets += tl->usedTilesets();
    }
    return usedTilesets.values();
}

void MapComposite::synch()
{
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        if (layerGroup->needsSynch())
            layerGroup->synch();
    }
}

void MapComposite::setAdjacentMap(int x, int y, MapInfo *mapInfo)
{
    int index = (x + 1) + (y + 1) * 3;
    if (index < 0 || index == 4 || index > 8) {
        Q_ASSERT(false);
        return;
    }
    if (mAdjacentMaps.isEmpty())
        mAdjacentMaps.resize(9);
    if (mAdjacentMaps[index]) {
        removeMap(mAdjacentMaps[index]);
        mAdjacentMaps[index] = 0;
    }
    if (!mapInfo)
        return;
    QPoint pos;
    switch (x) {
    case -1: pos.setX(-mapInfo->width()); break;
    case 1: pos.setX(mMapInfo->width()); break;
    }
    switch (y) {
    case -1: pos.setY(-mapInfo->height()); break;
    case 1: pos.setY(mMapInfo->height()); break;
    }
    mAdjacentMaps[index] = addMap(mapInfo, pos, 0, false);
    mAdjacentMaps[index]->mIsAdjacentMap = true;
}

MapComposite *MapComposite::adjacentMap(int x, int y)
{
    int index = (x + 1) + (y + 1) * 3;
    if (index < 0 || index == 4 || index > 8) {
        Q_ASSERT(false);
        return 0;
    }
    return mAdjacentMaps.size() ? mAdjacentMaps[index] : 0;
}

bool MapComposite::waitingForMapsToLoad() const
{
    if (mSubMapsLoading.size())
        return true;
    foreach (MapComposite *mc, mSubMaps) {
        if (mc->waitingForMapsToLoad())
            return true;
    }
    return false;
}

void MapComposite::recreate()
{
    qDeleteAll(mSubMaps);
    qDeleteAll(mLayerGroups);
    mSubMaps.clear();
    mLayerGroups.clear();
    mSortedLayerGroups.clear();
    mMinLevel = mMaxLevel = 0;
    mMap = mMapInfo->map();
    mOrientAdjustPos = mOrientAdjustTiles = QPoint();
    mAdjacentMaps.clear();

    ///// FIXME: everything below here is copied from our constructor

    if (mMap->orientation() != mOrientRender) {
        Map::Orientation orientSelf = mMap->orientation();
        if (orientSelf == Map::Isometric && mOrientRender == Map::LevelIsometric)
            mOrientAdjustPos = mOrientAdjustTiles = QPoint(3, 3);
        if (orientSelf == Map::LevelIsometric && mOrientRender == Map::Isometric)
            mOrientAdjustPos = mOrientAdjustTiles = QPoint(-3, -3);
    }

    int index = 0;
    for (Layer *layer : mMap->layers()) {
        if (TileLayer *tl = layer->asTileLayer()) {
            int level = layer->level();
            if (!mLayerGroups.contains(level))
                mLayerGroups[level] = new CompositeLayerGroup(this, level);
            mLayerGroups[level]->addTileLayer(tl, index);
            if (!mMapInfo->isBeingEdited())
                mLayerGroups[level]->setLayerVisibility(tl, !layer->name().contains(QLatin1String("NoRender")));
        }
        ++index;
    }

    // Load lots, but only if this is not the map being edited (that is handled
    // by the LotManager).
    if (!mMapInfo->isBeingEdited()) {
        foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
            int levelOffset = objectGroup->level();
//            (void) levelForLayer(objectGroup, &levelOffset);
            foreach (MapObject *object, objectGroup->objects()) {
                if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                    // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                    // then any sub-maps of its own will lose their level offsets.
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(),
                                                                          QFileInfo(mMapInfo->path()).absolutePath(),
                                                                          true, MapManager::PriorityLow);
                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
#ifdef WORLDED // FIXME: attempt to load this if mapsDirectory changes
                        subMapInfo = MapManager::instance()->getPlaceholderMap(object->type(),
                                                                               32, 32); // FIXME: calculate map size
#endif
                    }
                    if (subMapInfo) {
                        if (subMapInfo->isLoading()) {
                            connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
                                    SLOT(mapLoaded(MapInfo*)), Qt::UniqueConnection);
                            connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
                                    SLOT(mapFailedToLoad(MapInfo*)), Qt::UniqueConnection);
                            mSubMapsLoading += SubMapLoading(subMapInfo,
                                                             object->position().toPoint()
                                                             + mOrientAdjustPos * levelOffset,
                                                             levelOffset);
                        } else {
                            addMap(subMapInfo, object->position().toPoint()
                                   + mOrientAdjustPos * levelOffset,
                                   levelOffset, true);
                        }
                    }
                }
            }
        }
    }

    mMinLevel = 10000;
    mMaxLevel = 0;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        if (!mMapInfo->isBeingEdited())
            layerGroup->synch();
        if (layerGroup->level() < mMinLevel)
            mMinLevel = layerGroup->level();
        if (layerGroup->level() > mMaxLevel)
            mMaxLevel = layerGroup->level();
    }
    foreach (MapComposite *subMap, mSubMaps)
        if (subMap->mLevelOffset + subMap->mMaxLevel > mMaxLevel)
            mMaxLevel = subMap->mLevelOffset + subMap->mMaxLevel;

    if (mMinLevel == 10000)
        mMinLevel = 0;
#ifdef WORLDED
    if (!mParent && !mMapInfo->isBeingEdited())
        mMaxLevel = qMax(mMaxLevel, MAX_WORLD_LEVELS - 1);
#endif

    for (int level = mMinLevel; level <= mMaxLevel; ++level) {
        if (!mLayerGroups.contains(level))
            mLayerGroups[level] = new CompositeLayerGroup(this, level);
        mSortedLayerGroups.append(mLayerGroups[level]);
    }

    mBmpBlender->setMap(mMap);

#ifndef WORLDED
    /////

    if (mParent)
        mParent->moveSubMap(this, origin());
#endif
}

MapComposite *MapComposite::root()
{
    MapComposite *root = this;
    while (root->parent())
        root = root->parent();
    return root;
}

MapComposite *MapComposite::rootOrAdjacent()
{
    MapComposite *root = this;
    while (!root->isAdjacentMap() && root->parent())
        root = root->parent();
    return root;
}

QStringList MapComposite::getMapFileNames() const
{
    QStringList result;

    result += mMapInfo->path();
    foreach (MapComposite *subMap, mSubMaps)
        foreach (QString path, subMap->getMapFileNames())
            if (!result.contains(path))
                result += path;

    return result;
}

void MapComposite::bmpBlenderLayersRecreated()
{
    mLayerGroups[0]->setBmpBlendLayers(mBmpBlender->tileLayers());
}

void MapComposite::mapLoaded(MapInfo *mapInfo)
{
    bool synch = false;

    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        if (mSubMapsLoading[i].mapInfo == mapInfo) {
            addMap(mapInfo, mSubMapsLoading[i].pos, mSubMapsLoading[i].level);
            mSubMapsLoading.removeAt(i);
            --i;
            // Keep going, could be duplicate submaps to load
            synch = true;
        }
    }

    // Let the scene know it needs to synch.
    if (synch) {
#if 0
        MapComposite *mc = mParent;
        while (mc) {
            mc->ensureMaxLevels(mLevelOffset + mMaxLevel);
            mc = mc->mParent;
        }
#endif
        emit needsSynch();
    }
}

void MapComposite::mapFailedToLoad(MapInfo *mapInfo)
{
    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        if (mSubMapsLoading[i].mapInfo == mapInfo) {
            mSubMapsLoading.removeAt(i);
            --i;
            // Keep going, could be duplicate submaps to load
        }
    }
}

void MapComposite::setSuppressRegion(const QRegion &rgn, int level)
{
    mSuppressRgn = rgn;
    mSuppressLevel = level;
}
