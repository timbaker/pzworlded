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

#include "mapmanager.h"
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
#if 1 // ROAD_CRUD
    , mRoadLayer0(0)
    , mRoadLayer1(0)
#endif
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
    mLayerOpacity.insert(index, mOwner->mapInfo()->isBeingEdited()
                         ? layer->opacity() : 1.0f);

    // To optimize drawing of submaps, remember which layers are totally empty.
    // But don't do this for the top-level map (the one being edited).
    // TileLayer::isEmpty() is SLOW, it's why I'm caching it.
    bool empty = mOwner->mapInfo()->isBeingEdited()
            ? false
            : layer->isEmpty() || layer->name().contains(QLatin1String("NoRender"));
    mEmptyLayers.insert(index, empty);

#if 1 // ROAD_CRUD
    if (layer->name() == QLatin1String("0_Floor"))
        mRoadLayer0 = layer;
    if (layer->name() == QLatin1String("0_FloorOverlay"))
        mRoadLayer1 = layer;
#endif
}

void CompositeLayerGroup::removeTileLayer(TileLayer *layer)
{
    int index = mLayers.indexOf(layer);
    mVisibleLayers.remove(index);
    mLayerOpacity.remove(index);
    mEmptyLayers.remove(index);

    ZTileLayerGroup::removeTileLayer(layer);

    const QString name = MapComposite::layerNameWithoutPrefix(layer);
    index = mLayersByName[name].indexOf(layer);
    mLayersByName[name].remove(index);

#if 1 // ROAD_CRUD
    if (layer == mRoadLayer0)
        mRoadLayer0 = 0;
    if (layer == mRoadLayer1)
        mRoadLayer1 = 0;
#endif
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
}

bool CompositeLayerGroup::orderedCellsAt(const QPoint &pos,
                                         QVector<const Cell *> &cells,
                                         QVector<qreal> &opacities) const
{
    bool cleared = false;
    int index = -1;
    foreach (TileLayer *tl, mLayers) {
        ++index;
#if SPARSE_TILELAYER
        // Checking isEmpty() and mEmptyLayers to catch hidden NoRender layers in submaps
        bool empty = mEmptyLayers[index] || tl->isEmpty();
#else
        bool empty = mEmptyLayers[index];
#endif
#if 1 // ROAD_CRUD
        if (tl == mRoadLayer0 && !mOwner->roadLayer0()->isEmpty())
            empty = false;
        if (tl == mRoadLayer1 && !mOwner->roadLayer1()->isEmpty())
            empty = false;
#endif
        if (!mVisibleLayers[index] || empty)
            continue;
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel;
        if (tl->contains(subPos)) {
#if 1 // ROAD_CRUD
            if (tl == mRoadLayer0 || tl == mRoadLayer1) {
                const Cell *cell = (tl == mRoadLayer0)
                        ? &mOwner->roadLayer0()->cellAt(subPos)
                        : &mOwner->roadLayer1()->cellAt(subPos);
                if (!cell->isEmpty()) {
                    if (!cleared) {
                        cells.resize(0);
                        opacities.resize(0);
                        cleared = true;
                    }
                    cells.append(cell);
                    opacities.append(mLayerOpacity[index]);
                    continue;
                }
            }
#endif
            const Cell *cell = &tl->cellAt(subPos);
            if (!cell->isEmpty()) {
                if (!cleared) {
                    cells.resize(0);
                    opacities.resize(0);
                    cleared = true;
                }
                cells.append(cell);
                opacities.append(mLayerOpacity[index]);
            }
        }
    }

    // Overwrite map cells with sub-map cells at this location
    foreach (const SubMapLayers& subMapLayer, mPreparedSubMapLayers)
        subMapLayer.mLayerGroup->orderedCellsAt(pos - subMapLayer.mSubMap->origin(),
                                                cells, opacities);

    return !cells.isEmpty();
}

// This is for the benefit of LotFilesManager.  It ignores the visibility of
// layers (so NoRender layers are included) and visibility of sub-maps.
bool CompositeLayerGroup::orderedCellsAt2(const QPoint &pos, QVector<const Cell *> &cells) const
{
    bool cleared = false;
    int index = -1;
    foreach (TileLayer *tl, mLayers) {
        ++index;
        QPoint subPos = pos - mOwner->orientAdjustTiles() * mLevel;
        if (tl->contains(subPos)) {
#if 1 // ROAD_CRUD
            if (tl == mRoadLayer0 || tl == mRoadLayer1) {
                const Cell *cell = (tl == mRoadLayer0)
                        ? &mOwner->roadLayer0()->cellAt(subPos)
                        : &mOwner->roadLayer1()->cellAt(subPos);
                if (!cell->isEmpty()) {
                    if (!cleared) {
                        cells.resize(0);
                        cleared = true;
                    }
                    cells.append(cell);
                    continue;
                }
            }
#endif
            const Cell *cell = &tl->cellAt(subPos);
            if (!cell->isEmpty()) {
                if (!cleared) {
                    cells.resize(0);
                    cleared = true;
                }
                cells.append(cell);
            }
        }
    }

    // Overwrite map cells with sub-map cells at this location
    foreach (MapComposite *subMap, mOwner->subMaps()) {
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup)
            layerGroup->orderedCellsAt2(pos - subMap->origin(), cells);
    }

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
    mVisibleSubMapLayers.resize(0);

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

            layerGroup->synch();
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

void CompositeLayerGroup::saveOpacity()
{
    mSavedOpacity = mLayerOpacity;
}

void CompositeLayerGroup::restoreOpacity()
{
    mLayerOpacity = mSavedOpacity;
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

    mRoadLayer1 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
    mRoadLayer0 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
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

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        layerGroup->setNeedsSynch(true);

        if (CompositeLayerGroup *subMapLayerGroup = subMap->tileLayersForLevel(layerGroup->level() - levelOffset)) {
            foreach (TileLayer *tl, layerGroup->layers())
                subMapLayerGroup->setLayerOpacity(tl->name(), tl->opacity());
        }
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

const QList<MapComposite *> MapComposite::maps()
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
    if (mapInfo == mMapInfo) {
        return true;
    }
    bool affected = false;
    foreach (MapComposite *subMap, mSubMaps) {
        if (subMap->mapAboutToChange(mapInfo)) {
            // See CompositeLayerGroupItem::paint() for why this stops drawing.
            // FIXME: Not safe enough!
            foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
                layerGroup->setNeedsSynch(true);
            affected = true;
        }
    }
    return affected;
}

// Called by MapDocument when MapManager tells it a map changed on disk.
// Returns true if this map or any sub-map is affected.
bool MapComposite::mapFileChanged(MapInfo *mapInfo)
{
    if (mapInfo == mMapInfo) {
        recreate();
        return true;
    }

    bool changed = false;
    foreach (MapComposite *subMap, mSubMaps) {
        if (subMap->mapFileChanged(mapInfo)) {
            if (!changed) {
                foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
                    layerGroup->setNeedsSynch(true);
                changed = true;
            }
        }
    }

    return changed;
}

void MapComposite::recreate()
{
    qDeleteAll(mSubMaps);
    qDeleteAll(mLayerGroups);
    mSubMaps.clear();
    mLayerGroups.clear();
    mSortedLayerGroups.clear();

    delete mRoadLayer0;
    delete mRoadLayer1;

    mMap = mMapInfo->map();

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

#if 0
    // FIXME: no changing of mMap should happen after it is loaded!
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        mMap->addTileLayerGroup(layerGroup);
#endif

    if (!mMapInfo->isBeingEdited()) {
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

    mRoadLayer1 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
    mRoadLayer0 = new TileLayer(QString(), 0, 0, mMap->width(), mMap->height());
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

    mRoadLayer1->erase(QRegion(0, 0, mRoadLayer1->width(), mRoadLayer1->height()));
    mRoadLayer0->erase(QRegion(0, 0, mRoadLayer0->width(), mRoadLayer0->height()));

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
