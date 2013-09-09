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
#include "tilesetmanager.h"

#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

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
#ifdef BUILDINGED
    , mToolTileLayer(0)
#endif // BUILDINGED
#if 1 // ROAD_CRUD
    , mRoadLayer0(0)
    , mRoadLayer1(0)
#endif // ROAD_CRUD
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
    const QString name = MapComposite::layerNameWithoutPrefix(layer);
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

    mBmpBlendLayers.insert(index, 0);
    mNoBlends.insert(index, 0);
#ifdef BUILDINGED
    mBlendLayers.insert(index, 0);
    mForceNonEmpty.insert(index, false);
#endif // BUILDINGED
#if 1 // ROAD_CRUD
    if (layer->name() == QLatin1String("0_Floor"))
        mRoadLayer0 = layer;
    if (layer->name() == QLatin1String("0_FloorOverlay"))
        mRoadLayer1 = layer;
#endif // ROAD_CRUD
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
    mBlendLayers.remove(index);
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

    const QString name = MapComposite::layerNameWithoutPrefix(layer);
    index = mLayersByName[name].indexOf(layer);
    mLayersByName[name].remove(index);

#if 1 // ROAD_CRUD
    if (layer == mRoadLayer0)
        mRoadLayer0 = 0;
    if (layer == mRoadLayer1)
        mRoadLayer1 = 0;
#endif // ROAD_CRUD
}

void CompositeLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedSubMapLayers.resize(0);
    if (mAnyVisibleLayers == false)
        return;
    foreach (const SubMapLayers &subMapLayer, mVisibleSubMapLayers) {
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

bool CompositeLayerGroup::orderedCellsAt(const QPoint &pos,
                                         QVector<const Cell *> &cells,
                                         QVector<qreal> &opacities) const
{
    static QLatin1String sFloor("0_Floor");

    MapComposite *root = mOwner->rootOrAdjacent();
    if (root == mOwner)
        root->mKeepFloorLayerCount = 0;

    QRegion suppressRgn;
    if (mOwner->levelRecursive() + level() == mOwner->root()->suppressLevel())
        suppressRgn = mOwner->root()->suppressRegion();
    const QPoint rootPos = pos + mOwner->originRecursive();

    bool cleared = false;
    for (int index = 0; index < mLayers.size(); index++) {
        if (isLayerEmpty(index))
            continue;
        TileLayer *tl = mLayers[index];
        TileLayer *tlBmpBlend = mBmpBlendLayers[index];
        MapNoBlend *noBlend = mNoBlends[index];
#ifdef BUILDINGED
        TileLayer *tlBlend = mBlendLayers[index];
#endif // BUILDINGED
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel - tl->position();
        if (tl->contains(subPos)) {
#if 1 // ROAD_CRUD
            if (tl == mRoadLayer0 || tl == mRoadLayer1) {
                const Cell *cell = (tl == mRoadLayer0)
                        ? &mOwner->roadLayer0()->cellAt(subPos)
                        : &mOwner->roadLayer1()->cellAt(subPos);
                if (!cell->isEmpty()) {
                    if (!cleared) {
                        bool isFloor = !mLevel && !index && (tl->name() == sFloor);
                        if (isFloor) root->mKeepFloorLayerCount = 0;
                        cells.resize(root->mKeepFloorLayerCount);
                        opacities.resize(root->mKeepFloorLayerCount);
                        cleared = true;
                    }
                    cells.append(cell);
                    opacities.append(mLayerOpacity[index]);
                    if (mMaxFloorLayer >= index)
                        mOwner->mKeepFloorLayerCount = cells.size();
                    continue;
                }
            }
#endif // ROAD_CRUD
            const Cell *cell = &tl->cellAt(subPos);
            const Cell emptyCell;
            if (!mOwner->parent() && !mOwner->showMapTiles())
                cell = &emptyCell;
            if (tlBmpBlend && tlBmpBlend->contains(subPos) && !tlBmpBlend->cellAt(subPos).isEmpty())
                if (mOwner->parent() || mOwner->showBMPTiles()) {
                    if (!noBlend || !noBlend->get(subPos.x(), subPos.y()))
                        cell = &tlBmpBlend->cellAt(subPos);
                }
#ifdef BUILDINGED
            // Use an empty tool tile if given during erasing.
            if ((mToolTileLayer == tl) && !mToolTiles.isEmpty() &&
                    QRect(mToolTilesPos, QSize(mToolTiles.size(), mToolTiles[0].size())).contains(subPos))
                cell = &mToolTiles[subPos.x()-mToolTilesPos.x()][subPos.y()-mToolTilesPos.y()];
            else if (cell->isEmpty() && tlBlend && tlBlend->contains(subPos))
                cell = &tlBlend->cellAt(subPos);
#endif // BUILDINGED
#if 1
            if (index && suppressRgn.contains(rootPos))
                cell = &emptyCell;
#endif
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
            if (noBlend && mOwner && tl->name() == mOwner->mNoBlendLayer && noBlend->get(subPos.x(), subPos.y())) {
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
    }

    // Overwrite map cells with sub-map cells at this location.
    // Chop off sub-map cells that aren't in the root- or adjacent-map's bounds.
    QRect rootBounds(root->originRecursive(), root->mapInfo()->size());
    bool inRoot = (rootBounds.size() == QSize(300, 300)) && rootBounds.contains(rootPos);
    foreach (const SubMapLayers& subMapLayer, mPreparedSubMapLayers) {
        if (!inRoot && !subMapLayer.mSubMap->isAdjacentMap())
            continue;
        if (!subMapLayer.mBounds.contains(pos))
            continue;
        subMapLayer.mLayerGroup->orderedCellsAt(pos - subMapLayer.mSubMap->origin(),
                                                cells, opacities);
    }

    return !cells.isEmpty();
}

#ifdef WORLDED
void CompositeLayerGroup::prepareDrawing2()
{
    mPreparedSubMapLayers.resize(0);
    foreach (MapComposite *subMap, mOwner->subMaps()) {
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
    static QLatin1String sFloor("0_Floor");

    MapComposite *root = mOwner->root();
    if (root == mOwner)
        root->mKeepFloorLayerCount = 0;

    bool cleared = false;
    int index = -1;
    foreach (TileLayer *tl, mLayers) {
        ++index;
        TileLayer *tlBmpBlend = mBmpBlendLayers[index];
        MapNoBlend *noBlend = mNoBlends[index];
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel;
        if (tl->contains(subPos)) {
#if 1 // ROAD_CRUD
            if (tl == mRoadLayer0 || tl == mRoadLayer1) {
                const Cell *cell = (tl == mRoadLayer0)
                        ? &mOwner->roadLayer0()->cellAt(subPos)
                        : &mOwner->roadLayer1()->cellAt(subPos);
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
                    continue;
                }
            }
#endif // ROAD_CRUD
            const Cell *cell = &tl->cellAt(subPos);
            if (tlBmpBlend && tlBmpBlend->contains(subPos) && !tlBmpBlend->cellAt(subPos).isEmpty())
                if (!noBlend || !noBlend->get(subPos.x(), subPos.y()))
                    cell = &tlBmpBlend->cellAt(subPos);
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
    foreach (const SubMapLayers& subMapLayer, mPreparedSubMapLayers) {
        if (!subMapLayer.mBounds.contains(pos))
            continue;
        subMapLayer.mLayerGroup->orderedCellsAt2(pos - subMapLayer.mSubMap->origin(), cells);
    }

    return !cells.isEmpty();
}
#endif // WORLDED

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
    if (mBlendLayers[index] && !mBlendLayers[index]->isEmpty())
        return false;
    if (mToolTileLayer && !mToolTiles.isEmpty())
        return false;
#endif // BUILDINGED
#if 1 // ROAD_CRUD
    if (mLayers[index] == mRoadLayer0 && !mOwner->roadLayer0()->isEmpty())
        return false;
    if (mLayers[index] == mRoadLayer1 && !mOwner->roadLayer1()->isEmpty())
        return false;
#endif // ROAD_CRUD
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
    if (!mVisible) {
        mAnyVisibleLayers = false;
        mTileBounds = QRect();
        mSubMapTileBounds = QRect();
        mDrawMargins = QMargins(0, mOwner->map()->tileHeight(), mOwner->map()->tileWidth(), 0);
        mVisibleSubMapLayers.clear();
#ifdef BUILDINGED
        mBlendLayers.fill(0);
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
    }

#ifdef BUILDINGED
    // Do this before the isLayerEmpty() call below.
    mBlendLayers.fill(0);
    if (MapComposite *blendOverMap = mOwner->blendOverMap()) {
        if (CompositeLayerGroup *layerGroup = blendOverMap->tileLayersForLevel(mLevel)) {
            for (int i = 0; i < mLayers.size(); i++) {
                if (!mVisibleLayers[i])
                    continue;
                for (int j = 0; j < layerGroup->mLayers.size(); j++) {
                    TileLayer *blendLayer = layerGroup->mLayers[j];
                    if (blendLayer->name() == mLayers[i]->name()) {
                        mBlendLayers[i] = blendLayer;
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

    if (mToolTileLayer && !mToolTiles.isEmpty()) {
        unionTileRects(r, mToolTileLayer->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
        maxMargins(m, QMargins(0, 128, 64, 0), m);
        mAnyVisibleLayers = true;
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
                const QString name = MapComposite::layerNameWithoutPrefix(layerName);
                if (!mLayersByName.contains(name))
                    continue;
                foreach (Layer *layer, mLayersByName[name]) {
                    int index = mLayers.indexOf(layer->asTileLayer());
                    Q_ASSERT(index != -1);
                    mVisibleLayers[index] = rootGroup->mVisibleLayers[rootIndex];
                    mLayerOpacity[index] = rootGroup->mLayerOpacity[rootIndex];
                }
            }
        }
    }

    int index = 0;
    foreach (TileLayer *tl, mLayers) {
        if (!isLayerEmpty(index)) {
            unionTileRects(r, tl->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
            maxMargins(m, tl->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
        if (!mLevel && (!mOwner->parent() || mOwner->isAdjacentMap()) &&
                (index == mMaxFloorLayer + 1) &&
                tl->name().startsWith(QLatin1String("0_Floor")))
            mMaxFloorLayer = index;
        ++index;
    }

    mTileBounds = r;

    r = QRect();
    mVisibleSubMapLayers.resize(0);

    foreach (MapComposite *subMap, mOwner->subMaps()) {
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

    mBmpBlendLayers.fill(0);
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
    const QString name = MapComposite::layerNameWithoutPrefix(layerName);
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

    const QString name = MapComposite::layerNameWithoutPrefix(layer);
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
    if (mTileBounds.isEmpty() && mBlendLayers[index] && !mBlendLayers[index]->isEmpty()) {
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

QRectF CompositeLayerGroup::boundingRect(const MapRenderer *renderer)
{
    if (mNeedsSynch)
        synch();

    QRectF boundingRect = renderer->boundingRect(mTileBounds.translated(mOwner->originRecursive()),
                                                 mLevel + mOwner->levelRecursive());

    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.

    boundingRect.adjust(-mDrawMargins.left(),
                -qMax(0, mDrawMargins.top() - owner()->map()->tileHeight()),
                qMax(0, mDrawMargins.right() - owner()->map()->tileWidth()),
                mDrawMargins.bottom());

    foreach (const SubMapLayers &subMapLayer, mVisibleSubMapLayers) {
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
    , mBlendOverMap(0)
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

    int index = 0;
    foreach (Layer *layer, mMap->layers()) {
        int level;
        if (levelForLayer(layer, &level)) {
            // FIXME: no changing of mMap should happen after it is loaded!
            layer->setLevel(level); // for ObjectGroup,ImageLayer as well

            if (TileLayer *tl = layer->asTileLayer()) {
                if (!mLayerGroups.contains(level))
                    mLayerGroups[level] = new CompositeLayerGroup(this, level);
                mLayerGroups[level]->addTileLayer(tl, index);
                if (!mapInfo->isBeingEdited())
                    mLayerGroups[level]->setLayerVisibility(tl, !layer->name().contains(QLatin1String("NoRender")));
            }
        }
        ++index;
    }

#if 1 // ROAD_CRUD
    mRoadLayer1 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
    mRoadLayer0 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
#endif

    // Load lots, but only if this is not the map being edited (that is handled
    // by the LotManager).
    if (!mapInfo->isBeingEdited()) {
        foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
            int levelOffset;
            (void) levelForLayer(objectGroup, &levelOffset);
            foreach (MapObject *object, objectGroup->objects()) {
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
#if 1 // FIXME: attempt to load this if mapsDirectory changes
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
        mMaxLevel = qMax(mMaxLevel, 10);
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
#if 1 // ROAD_CRUD
    delete mRoadLayer1;
    delete mRoadLayer0;
#endif // ROAD_CRUD
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

void MapComposite::layerAdded(int index)
{
    layerRenamed(index);
}

void MapComposite::layerAboutToBeRemoved(int index)
{
    Layer *layer = mMap->layerAt(index);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group()) {
            CompositeLayerGroup *oldGroup = (CompositeLayerGroup*)tl->group();
            emit layerAboutToBeRemovedFromGroup(index);
            removeLayerFromGroup(index);
            emit layerRemovedFromGroup(index, oldGroup);
        }
    }
}

void MapComposite::layerRenamed(int index)
{
    Layer *layer = mMap->layerAt(index);

    int oldLevel = layer->level();
    int newLevel;
    bool hadGroup = false;
    bool hasGroup = levelForLayer(layer, &newLevel);
    CompositeLayerGroup *oldGroup = 0;

    if (TileLayer *tl = layer->asTileLayer()) {
        oldGroup = (CompositeLayerGroup*)tl->group();
        hadGroup = oldGroup != 0;
        if (oldGroup)
            oldGroup->layerRenamed(tl);
    }

    if ((oldLevel != newLevel) || (hadGroup != hasGroup)) {
        if (hadGroup) {
            emit layerAboutToBeRemovedFromGroup(index);
            removeLayerFromGroup(index);
            emit layerRemovedFromGroup(index, oldGroup);
        }
        if (oldLevel != newLevel) {
            layer->setLevel(newLevel);
            emit layerLevelChanged(index, oldLevel);
        }
        if (hasGroup && layer->isTileLayer()) {
            addLayerToGroup(index);
            emit layerAddedToGroup(index);
        }
    }
}

void MapComposite::addLayerToGroup(int index)
{
    Layer *layer = mMap->layerAt(index);
    Q_ASSERT(layer->isTileLayer());
    Q_ASSERT(levelForLayer(layer));
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

void MapComposite::removeLayerFromGroup(int index)
{
    Layer *layer = mMap->layerAt(index);
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
    return 0;
}

CompositeLayerGroup *MapComposite::layerGroupForLevel(int level) const
{
    if (mLayerGroups.contains(level))
        return mLayerGroups[level];
    return 0;
}

CompositeLayerGroup *MapComposite::layerGroupForLayer(TileLayer *tl) const
{
    if (tl->group())
        return tileLayersForLevel(tl->level());
    return 0;
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
        if (mParent /*!mMapInfo->isBeingEdited()*/) {
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
    CompositeLayerGroup *previousGroup = 0;
    int layerIndex = -1;
    foreach (Layer *layer, mMap->layers()) {
        ++layerIndex;
        int level;
        bool hasGroup = levelForLayer(layer, &level);
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

    foreach (CompositeLayerGroup *layerGroup, mSortedLayerGroups) {
        result += ZOrderItem(layerGroup);
        QVector<LayerPair> layers = layersAboveLevel[layerGroup];
        foreach (LayerPair pair, layers)
            result += ZOrderItem(pair.second, pair.first);
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
#if 1 // ROAD_CRUD
    delete mRoadLayer0;
    delete mRoadLayer1;
#endif // ROAD_CRUD
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
    foreach (Layer *layer, mMap->layers()) {
        int level;
        if (levelForLayer(layer, &level)) {
            // FIXME: no changing of mMap should happen after it is loaded!
            layer->setLevel(level); // for ObjectGroup,ImageLayer as well

            if (TileLayer *tl = layer->asTileLayer()) {
                if (!mLayerGroups.contains(level))
                    mLayerGroups[level] = new CompositeLayerGroup(this, level);
                mLayerGroups[level]->addTileLayer(tl, index);
                if (!mMapInfo->isBeingEdited())
                    mLayerGroups[level]->setLayerVisibility(tl, !layer->name().contains(QLatin1String("NoRender")));
            }
        }
        ++index;
    }

#if 1 // ROAD_CRUD
    mRoadLayer1 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
    mRoadLayer0 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
#endif // ROAD_CRUD

    // Load lots, but only if this is not the map being edited (that is handled
    // by the LotManager).
    if (!mMapInfo->isBeingEdited()) {
        foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
            int levelOffset;
            (void) levelForLayer(objectGroup, &levelOffset);
            foreach (MapObject *object, objectGroup->objects()) {
                if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                    // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                    // then any sub-maps of its own will lose their level offsets.
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(),
                                                                          QFileInfo(mMapInfo->path()).absolutePath(),
                                                                          true, MapManager::PriorityLow);
                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
#if 1 // FIXME: attempt to load this if mapsDirectory changes
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
        mMaxLevel = qMax(mMaxLevel, 10);
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
        MapComposite *mc = mParent;
        while (mc) {
            mc->ensureMaxLevels(mLevelOffset + mMaxLevel);
            mc = mc->mParent;
        }
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

#if 1 // ROAD_CRUD
#include "road.h"
#include "tileset.h"
#include <QRegion>

static QList<Road*> roadsWithEndpoint(const QPoint &roadPos,
                                      const QList<Road*> &roads,
                                      Road *exclude)
{
    QList<Road*> result;
    foreach (Road *road, roads) {
        if (road == exclude)
            continue;
        if (road->start() == roadPos || road->end() == roadPos)
            result += road;
    }
    return result;
}

static Tile *parseTileDescription(const QString &tileName,
                                  const QList<Tileset*> &tilesets)
{
    if (tileName.isEmpty())
        return 0;
    int n = tileName.lastIndexOf(QLatin1Char('_'));
    if (n < 0)
        return 0;
    QString tilesetName = tileName.mid(0, n);
    int tileID = tileName.mid(n + 1).toInt();
    foreach (Tileset *ts, tilesets) {
        if (ts->name() == tilesetName) { // FIXME: file-name not tileset-name!!!
            if (tileID < ts->tileCount())
                return ts->tileAt(tileID);
            break;
        }
    }
    return 0;
}

static Tile *tileForRoad(Road *road, const QList<Tileset*> &tilesets)
{
    return parseTileDescription(road->tileName(), tilesets);
}

void MapComposite::generateRoadLayers(const QPoint &roadPos, const QList<Road *> &roads)
{
    QRect cellRect(roadPos, QSize(300, 300));
    QList<Road*> roadsInCell;
    foreach (Road *road, roads) {
        QRect roadBounds = road->bounds();
        if (roadBounds.intersects(cellRect))
            roadsInCell += road;
    }

    mRoadLayer1->erase();
    mRoadLayer0->erase();

    // Fill roads with road tile

    foreach (Road *road, roadsInCell) {
        Tile *tile;
        if (!(tile = tileForRoad(road, mMap->tilesets())))
            continue;
        Cell cell0(tile);
        QRect roadBounds = road->bounds();
        roadBounds.translate(-roadPos); // layer coordinates
        for (int x = roadBounds.left(); x <= roadBounds.right(); x++) {
            for (int y = roadBounds.top(); y <= roadBounds.bottom(); y++) {
                if (mRoadLayer0->contains(x, y))
                    mRoadLayer0->setCell(x, y, cell0);
            }
        }
    }

    // Traffic lines

    foreach (Road *road, roadsInCell) {
        if (!road->trafficLines())
            continue;
        Tile *tileInnerNS = parseTileDescription(road->trafficLines()->inner.ns,
                                                 mMap->tilesets());
        Tile *tileInnerWE = parseTileDescription(road->trafficLines()->inner.we,
                                                 mMap->tilesets());
        Tile *tileInnerNW = parseTileDescription(road->trafficLines()->inner.nw,
                                                 mMap->tilesets());
        Tile *tileInnerSW = parseTileDescription(road->trafficLines()->inner.sw,
                                                 mMap->tilesets());

        Tile *tileOuterNS = parseTileDescription(road->trafficLines()->outer.ns,
                                                 mMap->tilesets());
        Tile *tileOuterWE = parseTileDescription(road->trafficLines()->outer.we,
                                                 mMap->tilesets());
        Tile *tileOuterNE = parseTileDescription(road->trafficLines()->outer.ne,
                                                 mMap->tilesets());
        Tile *tileOuterSE = parseTileDescription(road->trafficLines()->outer.se,
                                                 mMap->tilesets());
        QRect roadBounds = road->bounds();
        roadBounds.translate(-roadPos); // layer coordinates
        QList<Road*> roadsAtStart = roadsWithEndpoint(road->start(), roadsInCell,
                                                      road);
        QList<Road*> roadsAtEnd = roadsWithEndpoint(road->end(), roadsInCell,
                                                    road);
        if (road->isVertical()) {
            int x = road->x1() - roadPos.x();
            int y1 = roadBounds.top(), y2 = roadBounds.bottom();
            if (roadsAtStart.count()) {
                if (road->orient() == Road::NorthSouth)
                    y1 += road->width() / 2;
                else
                    y2 -= road->width() -  road->width() / 2;
            }
            if (roadsAtEnd.count()) {
                if (road->orient() == Road::NorthSouth)
                    y2 -= road->width() -  road->width() / 2;
                else
                    y1 += road->width() / 2;
            }
            for (int y = y1; y <= y2; y++) {
                if (tileInnerNS && mRoadLayer1->contains(x, y)) {
                    Tile *curTile = mRoadLayer1->cellAt(x, y).tile;
                    if (curTile == tileInnerWE)
                        mRoadLayer1->setCell(x, y, Cell(tileInnerNW));
                    else if (curTile == tileOuterWE)
                        mRoadLayer1->setCell(x, y, Cell(tileInnerSW));
                    else if (curTile == tileInnerNW || curTile == tileInnerSW)
                        ;
                    else
                        mRoadLayer1->setCell(x, y, Cell(tileInnerNS));
                }
                if (tileOuterNS && mRoadLayer1->contains(x-1, y) && road->width() > 1) {
                    Tile *curTile = mRoadLayer1->cellAt(x-1, y).tile;
                    if (curTile == tileInnerWE)
                        mRoadLayer1->setCell(x-1, y, Cell(tileOuterNE));
                    else if (curTile == tileOuterWE)
                        mRoadLayer1->setCell(x-1, y, Cell(tileOuterSE));
                    else if (curTile == tileOuterNE || curTile == tileOuterSE)
                        ;
                    else
                        mRoadLayer1->setCell(x-1, y, Cell(tileOuterNS));
                }
            }
        } else {
            int y = road->y1() - roadPos.y();
            int x1 = roadBounds.left(), x2 = roadBounds.right();
            if (roadsAtStart.count()) {
                if (road->orient() == Road::WestEast)
                    x1 += road->width() / 2;
                else
                    x2 -= road->width() -  road->width() / 2;
            }
            if (roadsAtEnd.count()) {
                if (road->orient() == Road::WestEast)
                    x2 -= road->width() -  road->width() / 2;
                else
                    x1 += road->width() / 2;
            }
            for (int x = x1; x <= x2; x++) {
                if (tileInnerWE && mRoadLayer1->contains(x, y)) {
                    Tile *curTile = mRoadLayer1->cellAt(x, y).tile;
                    if (curTile == tileOuterNS)
                        mRoadLayer1->setCell(x, y, Cell(tileOuterNE));
                    else if (curTile == tileInnerNS)
                        mRoadLayer1->setCell(x, y, Cell(tileInnerNW));
                    else if (curTile == tileOuterNE || curTile == tileInnerNW)
                        ;
                    else
                        mRoadLayer1->setCell(x, y, Cell(tileInnerWE));
                }
                if (tileOuterWE && mRoadLayer1->contains(x, y-1) && road->width() > 1) {
                    Tile *curTile = mRoadLayer1->cellAt(x, y-1).tile;
                    if (curTile == tileOuterNS)
                        mRoadLayer1->setCell(x, y-1, Cell(tileOuterSE));
                    else if (curTile == tileInnerNS)
                        mRoadLayer1->setCell(x, y-1, Cell(tileInnerSW));
                    else if (curTile == tileOuterSE || curTile == tileInnerSW)
                        ;
                    else
                        mRoadLayer1->setCell(x, y-1, Cell(tileOuterWE));
                }
            }
        }
    }
}
#endif // ROAD_CRUD
