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

#include "preferences.h"
#include "mapmanager.h"

#include "map.h"
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

///// ///// ///// ///// /////

CompositeLayerGroup::CompositeLayerGroup(MapComposite *owner, int level)
    : ZTileLayerGroup(owner->map(), level)
    , mOwner(owner)
    , mAnyVisibleLayers(false)
    , mNeedsSynch(true)
{

}

void CompositeLayerGroup::addTileLayer(TileLayer *layer, int index)
{
    ZTileLayerGroup::addTileLayer(layer, index);

    // Remember the names of layers (without the N_ prefix)
    const QString name = MapComposite::layerNameWithoutPrefix(layer);
    mLayersByName[name].append(layer);

    index = mLayers.indexOf(layer);
    mVisibleLayers.insert(index, layer->isVisible());

    // To optimize drawing of submaps, remember which layers are totally empty.
    // But don't do this for the top-level map (the one being edited).
    // TileLayer::isEmpty() is SLOW, it's why I'm caching it.
    bool empty = mOwner->mapInfo()->isBeingEdited()
            ? false
            : layer->isEmpty() || layer->name().contains(QLatin1String("NoRender"));
    mEmptyLayers.insert(index, empty);
}

void CompositeLayerGroup::removeTileLayer(TileLayer *layer)
{
    int index = mLayers.indexOf(layer);
    mVisibleLayers.remove(index);
    mEmptyLayers.remove(index);

    ZTileLayerGroup::removeTileLayer(layer);

    const QString name = MapComposite::layerNameWithoutPrefix(layer);
    index = mLayersByName[name].indexOf(layer);
    mLayersByName[name].remove(index);
}

void CompositeLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedSubMapLayers.clear();
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
}

bool CompositeLayerGroup::orderedCellsAt(const QPoint &pos, QVector<const Cell *> &cells) const
{
    bool cleared = false;
    int index = -1;
    foreach (TileLayer *tl, mLayers) {
        ++index;
#if SPARSE_TILELAYER
        // Checking isEmpty() and mEmptyLayers to catch hidden NoRender layers in submaps
        if (!mVisibleLayers[index] || mEmptyLayers[index] || tl->isEmpty())
#else
        if (!mVisibleLayers[index] || mEmptyLayers[index])
#endif
            continue;
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel;
        if (tl->contains(subPos)) {
            const Cell *cell = &tl->cellAt(subPos);
            if (!cell->isEmpty()) {
                if (!cleared) {
                    cells.clear();
                    cleared = true;
                }
                cells.append(cell);
            }
        }
    }

    // Overwrite map cells with sub-map cells at this location
    foreach (const SubMapLayers& subMapLayer, mPreparedSubMapLayers)
        subMapLayer.mLayerGroup->orderedCellsAt(pos - subMapLayer.mSubMap->origin(), cells);

    return !cells.isEmpty();
}

void CompositeLayerGroup::synch()
{
    QRect r;
    // See TileLayer::drawMargins()
    QMargins m(0, mOwner->map()->tileHeight(), mOwner->map()->tileWidth(), 0);

    mAnyVisibleLayers = false;

    int index = 0;
    foreach (TileLayer *tl, mLayers) {
#if SPARSE_TILELAYER
        // Checking isEmpty() and mEmptyLayers to catch hidden NoRender layers in submaps
        if (mVisibleLayers[index] && !mEmptyLayers[index] && !tl->isEmpty()) {
#else
        if (mVisibleLayers[index] && !mEmptyLayers[index]) {
#endif
            unionTileRects(r, tl->bounds().translated(mOwner->orientAdjustTiles() * mLevel), r);
            maxMargins(m, tl->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
        ++index;
    }

    mTileBounds = r;

    r = QRect();
    mVisibleSubMapLayers.clear();

    foreach (MapComposite *subMap, mOwner->subMaps()) {
        if (!subMap->isGroupVisible() || !subMap->isVisible())
            continue;
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup) {

            // Set the visibility of sub-map layers to match this layer-group's layers.
            // Layers in the sub-map without a matching name in the base map are always shown.
            foreach (TileLayer *layer, layerGroup->mLayers)
                layerGroup->setLayerVisibility(layer, true);
            int index = 0;
            foreach (TileLayer *layer, mLayers)
                layerGroup->setLayerVisibility(layer->name(), mVisibleLayers[index++]);

#if 1 // Re-enable this if submaps ever change
            layerGroup->synch();
#endif
            if (layerGroup->mAnyVisibleLayers) {
                mVisibleSubMapLayers.append(SubMapLayers(subMap, layerGroup));
                unionTileRects(r, layerGroup->bounds().translated(subMap->origin()), r);
                maxMargins(m, layerGroup->drawMargins(), m);
                mAnyVisibleLayers = true;
            }
        }
    }

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

void CompositeLayerGroup::setLayerVisibility(const QString &layerName, bool visible)
{
    const QString name = MapComposite::layerNameWithoutPrefix(layerName);
    if (!mLayersByName.contains(name))
        return;
    foreach (Layer *layer, mLayersByName[name]) {
        mVisibleLayers[mLayers.indexOf(layer->asTileLayer())] = visible;
        mNeedsSynch = true;
    }
}

void CompositeLayerGroup::setLayerVisibility(TileLayer *tl, bool visible)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    if (visible != mVisibleLayers[index]) {
        mVisibleLayers[index] = visible;
        mNeedsSynch = true;
    }
}

bool CompositeLayerGroup::isLayerVisible(TileLayer *tl)
{
    int index = mLayers.indexOf(tl);
    Q_ASSERT(index != -1);
    return mVisibleLayers[index];
}

#if 0
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
#endif

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
    : mMapInfo(mapInfo)
    , mMap(mapInfo->map())
    , mParent(parent)
    , mPos(positionInParent)
    , mLevelOffset(levelOffset)
    , mOrientRender(orientRender)
    , mMinLevel(0)
    , mMaxLevel(0)
    , mVisible(true)
    , mGroupVisible(true)
    , mHiddenDuringDrag(false)
{
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

#if 0
    // FIXME: no changing of mMap should happen after it is loaded!
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        mMap->addTileLayerGroup(layerGroup);
#endif

    if (!mapInfo->isBeingEdited()) {
        foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
            foreach (MapObject *object, objectGroup->objects()) {
                if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                    // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                    // then any sub-maps of its own will lose their level offsets.
                    // FIXME: look in the same directory as the parent map, then the maptools directory.
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(),
                                                                          QFileInfo(mMapInfo->path()).absolutePath());
                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
                        subMapInfo = MapManager::instance()->getPlaceholderMap(object->type(),
                                                                               32, 32); // FIXME: calculate map size
                    }
                    if (subMapInfo) {
                        int levelOffset;
                        (void) levelForLayer(objectGroup, &levelOffset);
#if 1
                        MapComposite *_subMap = new MapComposite(subMapInfo, mOrientRender,
                                                                 this, object->position().toPoint()
                                                                 + mOrientAdjustPos * levelOffset,
                                                                 levelOffset);
                        mSubMaps.append(_subMap);
#else
                        addMap(subMap, object->position().toPoint(), levelOffset);
#endif
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

    if (mMinLevel == 10000)
        mMinLevel = 0;

    if (!mParent && !mMapInfo->isBeingEdited())
        mMaxLevel = qMax(mMaxLevel, 10);

    for (int level = mMinLevel; level <= mMaxLevel; ++level) {
        if (!mLayerGroups.contains(level))
            mLayerGroups[level] = new CompositeLayerGroup(this, level);
        mSortedLayerGroups.append(mLayerGroups[level]);
    }
}

MapComposite::~MapComposite()
{
    qDeleteAll(mSubMaps);
    qDeleteAll(mLayerGroups);
}

bool MapComposite::levelForLayer(Layer *layer, int *levelPtr)
{
    if (levelPtr) (*levelPtr) = 0;

    // See if the layer name matches "0_foo" or "1_bar" etc.
    const QString& name = layer->name();
    QStringList sl = name.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        uint level = sl[0].toUInt(&conversionOK);
        if (levelPtr) (*levelPtr) = level;
        return conversionOK;
    }
    return false;
}

MapComposite *MapComposite::addMap(MapInfo *mapInfo, const QPoint &pos, int levelOffset)
{
    MapComposite *subMap = new MapComposite(mapInfo, mOrientRender, this, pos, levelOffset);
    mSubMaps.append(subMap);

    ensureMaxLevels(levelOffset + subMap->maxLevel());

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->setNeedsSynch(true);

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
        layerGroup->synch();
}

CompositeLayerGroup *MapComposite::tileLayersForLevel(int level) const
{
    if (mLayerGroups.contains(level))
        return mLayerGroups[level];
    return 0;
}

CompositeLayerGroup *MapComposite::layerGroupForLayer(TileLayer *tl) const
{
    return tileLayersForLevel(tl->level());
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
        if (mLevelOffset + layerGroup->level() > renderer->maxLevel())
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
        int minLevel = mLevelOffset;
        if (minLevel > renderer->maxLevel())
            minLevel = renderer->maxLevel();
        unionSceneRects(bounds,
                        renderer->boundingRect(mapTileBounds, minLevel),
                        bounds);
        // When setting the bounds of the scene, make sure the highest level is included
        // in the sceneRect() so the grid won't be cut off.
        int maxLevel = mLevelOffset + mMaxLevel;
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

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->restoreVisibility();

    foreach (MapComposite *subMap, mSubMaps)
        subMap->restoreVisibility();
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
