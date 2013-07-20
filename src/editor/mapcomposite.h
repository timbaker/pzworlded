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

#ifndef MAPCOMPOSITE_H
#define MAPCOMPOSITE_H

#include "map.h"
#define BUILDINGED
#ifdef BUILDINGED
#include "tilelayer.h" // for Cell
#endif
#include "ztilelayergroup.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class MapInfo;
#if 1 // ROAD_CRUD
class Road;
#endif // ROAD_CRUD

namespace Tiled {
class Layer;
namespace Internal {
class BmpBlender;
}
}

class MapComposite;

class CompositeLayerGroup : public Tiled::ZTileLayerGroup
{
public:
    CompositeLayerGroup(MapComposite *owner, int level);

    void addTileLayer(Tiled::TileLayer *layer, int index);
    void removeTileLayer(Tiled::TileLayer *layer);

    void prepareDrawing(const Tiled::MapRenderer *renderer, const QRect &rect);
    bool orderedCellsAt(const QPoint &pos, QVector<const Tiled::Cell*>& cells,
                        QVector<qreal> &opacities) const;

    QRect bounds() const;
    QMargins drawMargins() const;

    QRectF boundingRect(const Tiled::MapRenderer *renderer);

#ifdef WORLDED
    void prepareDrawing2();
    bool orderedCellsAt2(const QPoint &pos, QVector<const Tiled::Cell*>& cells) const;
#endif

    bool setLayerVisibility(const QString &layerName, bool visible);
    bool setLayerVisibility(Tiled::TileLayer *tl, bool visible);
    bool isLayerVisible(Tiled::TileLayer *tl);
    void layerRenamed(Tiled::TileLayer *layer);

    bool setLayerOpacity(const QString &layerName, qreal opacity);
    bool setLayerOpacity(Tiled::TileLayer *tl, qreal opacity);
    void synchSubMapLayerOpacity(const QString &layerName, qreal opacity);

    MapComposite *owner() const { return mOwner; }

    bool regionAltered(Tiled::TileLayer *tl);

    void setNeedsSynch(bool synch) { mNeedsSynch = synch; }
    bool needsSynch() const { return mNeedsSynch; }
    bool isLayerEmpty(int index) const;
    void synch();

    void saveVisibility();
    void restoreVisibility();
    void saveOpacity();
    void restoreOpacity();

    bool setBmpBlendLayers(const QList<Tiled::TileLayer*> &layers);
    const QVector<Tiled::TileLayer*> &bmpBlendLayers() const
    { return mBmpBlendLayers; }

#ifdef BUILDINGED
    void setToolTiles(const QVector<QVector<Tiled::Cell> > &tiles,
                      const QPoint &pos, Tiled::TileLayer *layer)
    {
        mToolTiles = tiles;
        mToolTilesPos = pos;
        mToolTileLayer = layer;
    }

    void clearToolTiles()
    { mToolTiles.clear(); mToolTileLayer = 0; mToolTilesPos = QPoint(-1, -1); }

    bool setLayerNonEmpty(const QString &layerName, bool force);
    bool setLayerNonEmpty(Tiled::TileLayer *tl, bool force);

    void setHighlightLayer(const QString &layerName)
    { mHighlightLayer = layerName; }
#endif

private:
    MapComposite *mOwner;
    bool mAnyVisibleLayers;
    bool mNeedsSynch;
    QRect mTileBounds;
    QRect mSubMapTileBounds;
    QMargins mDrawMargins;
    QVector<bool> mVisibleLayers;
    QVector<bool> mEmptyLayers;
    QVector<qreal> mLayerOpacity;
    int mMaxFloorLayer;
    QMap<QString,QVector<Tiled::Layer*> > mLayersByName;
    QVector<bool> mSavedVisibleLayers;
    QVector<qreal> mSavedOpacity;

    struct SubMapLayers
    {
        SubMapLayers()
            : mSubMap(0)
            , mLayerGroup(0)
        {
        }
        SubMapLayers(MapComposite *subMap, CompositeLayerGroup *layerGroup);

        MapComposite *mSubMap;
        CompositeLayerGroup *mLayerGroup;
        QRect mBounds;
    };

    QVector<SubMapLayers> mPreparedSubMapLayers;
    QVector<SubMapLayers> mVisibleSubMapLayers;

    QVector<Tiled::TileLayer*> mBmpBlendLayers;
#ifdef BUILDINGED
    QVector<Tiled::TileLayer*> mBlendLayers;
    QVector<QVector<Tiled::Cell> > mToolTiles;
    QPoint mToolTilesPos;
    Tiled::TileLayer *mToolTileLayer;
    QString mHighlightLayer;
    QVector<bool> mForceNonEmpty;
#endif // BUILDINGED
#if 1 // ROAD_CRUD
    Tiled::TileLayer *mRoadLayer0; // 0_Floor
    Tiled::TileLayer *mRoadLayer1; // 0_FloorOverlay
#endif // ROAD_CRUD
};

class MapComposite : public QObject
{
    Q_OBJECT
public:
    MapComposite(MapInfo *mapInfo, Tiled::Map::Orientation orientRender = Tiled::Map::Unknown,
                 MapComposite *parent = 0, const QPoint &positionInParent = QPoint(),
                 int levelOffset = 0);
    ~MapComposite();

    static bool levelForLayer(const QString &layerName, int *levelPtr = 0);
    static bool levelForLayer(Tiled::Layer *layer, int *levelPtr = 0);
    static QString layerNameWithoutPrefix(const QString &name);
    static QString layerNameWithoutPrefix(Tiled::Layer *layer);

    MapComposite *addMap(MapInfo *mapInfo, const QPoint &pos, int levelOffset,
                         bool creating = false);
    void removeMap(MapComposite *subMap);
    void moveSubMap(MapComposite *subMap, const QPoint &pos);

    Tiled::Map *map() const { return mMap; }
    MapInfo *mapInfo() const { return mMapInfo; }

    void layerAdded(int index);
    void layerAboutToBeRemoved(int index);
    void layerRenamed(int index);

    int layerGroupCount() const { return mLayerGroups.size(); }
    const QMap<int,CompositeLayerGroup*>& layerGroups() const { return mLayerGroups; }
    CompositeLayerGroup *tileLayersForLevel(int level) const;
    CompositeLayerGroup *layerGroupForLevel(int level) const;
    const QList<CompositeLayerGroup*> &sortedLayerGroups() const { return mSortedLayerGroups; }
    CompositeLayerGroup *layerGroupForLayer(Tiled::TileLayer *tl) const;

    const QVector<MapComposite*>& subMaps() const { return mSubMaps; }
    QList<MapComposite*> maps();

    MapComposite *parent() const { return mParent; }

    void setOrigin(const QPoint &origin);
    QPoint origin() const { return mPos; }

    QPoint originRecursive() const;
    int levelRecursive() const;

    void setLevel(int level) { mLevelOffset = level; }
    int levelOffset() const { return mLevelOffset; }

    void setVisible(bool visible) { mVisible = visible; }
    bool isVisible() const { return mVisible; }

    void setGroupVisible(bool visible) { mGroupVisible = visible; }
    bool isGroupVisible() const { return mGroupVisible; }

    int maxLevel() const { return mMaxLevel; }

    QPoint orientAdjustPos() const { return mOrientAdjustPos; }
    QPoint orientAdjustTiles() const { return mOrientAdjustTiles; }

    /**
      * Hack: when dragging a MapObjectItem representing a Lot, the map is hidden
      * at the start of the drag and shown when dragging is finished.  But I don't
      * want to affect the scene bounds, so instead of calling setVisible(false)
      * I call this.
      */
    void setHiddenDuringDrag(bool hidden) { mHiddenDuringDrag = hidden; }
    bool isHiddenDuringDrag() const { return mHiddenDuringDrag; }

    QRectF boundingRect(Tiled::MapRenderer *renderer, bool forceMapBounds = true) const;

    /**
      * Used when generating map images.
      */
    void saveVisibility();
    void restoreVisibility();
    void saveOpacity();
    void restoreOpacity();

    void ensureMaxLevels(int maxLevel);

    struct ZOrderItem
    {
        ZOrderItem(CompositeLayerGroup *group)
            : layer(0), layerIndex(-1), group(group) {}
        ZOrderItem(Tiled::Layer *layer, int layerIndex)
            : layer(layer), layerIndex(layerIndex), group(0) {}
        Tiled::Layer *layer;
        int layerIndex;
        CompositeLayerGroup *group;
    };
    typedef QList<ZOrderItem> ZOrderList;
    ZOrderList zOrder();

    QStringList getMapFileNames() const;

    bool mapAboutToChange(MapInfo *mapInfo);
    bool mapChanged(MapInfo *mapInfo);

    bool isTilesetUsed(Tiled::Tileset *tileset, bool recurse = true);
    QList<Tiled::Tileset*> usedTilesets();

    void synch();

    Tiled::Internal::BmpBlender *bmpBlender() const
    { return mBmpBlender; }

    void setShowBMPTiles(bool show)
    { mShowBMPTiles = show; }
    bool showBMPTiles() const
    { return mShowBMPTiles; }

    void setShowMapTiles(bool show)
    { mShowMapTiles = show; }
    bool showMapTiles() const
    { return mShowMapTiles; }

#ifdef BUILDINGED
    void setBlendOverMap(MapComposite *mapComposite)
    { mBlendOverMap = mapComposite; }

    MapComposite *blendOverMap() const
    { return mBlendOverMap; }
#endif // BUILDINGED

    void setAdjacentMap(int x, int y, MapInfo *mapInfo);
    MapComposite *adjacentMap(int x, int y);
    bool isAdjacentMap()/* const*/
    { return mIsAdjacentMap;/*mParent ? mParent->mAdjacentMaps.contains(this) : false;*/ }

    bool waitingForMapsToLoad() const;

    void setSuppressRegion(const QRegion &rgn, int level);
    QRegion suppressRegion() const
    { return mSuppressRgn; }
    int suppressLevel() const
    { return mSuppressLevel; }

#if 1 // ROAD_CRUD
    void generateRoadLayers(const QPoint &roadPos, const QList<Road *> &roads);
    Tiled::TileLayer *roadLayer1() const { return mRoadLayer1; }
    Tiled::TileLayer *roadLayer0() const { return mRoadLayer0; }
#endif // ROAD_CRUD

signals:
    void layerGroupAdded(int level);
    void layerAddedToGroup(int index);
    void layerAboutToBeRemovedFromGroup(int index);
    void layerRemovedFromGroup(int index, CompositeLayerGroup *oldGroup);
    void layerLevelChanged(int index, int oldLevel);

    void needsSynch();

private slots:
    void bmpBlenderLayersRecreated();
    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

private:
    void addLayerToGroup(int index);
    void removeLayerFromGroup(int index);

    void recreate();

private:
    MapInfo *mMapInfo;
    Tiled::Map *mMap;
#ifdef BUILDINGED
    MapComposite *mBlendOverMap;
#endif
    QVector<MapComposite*> mSubMaps;
    QMap<int,CompositeLayerGroup*> mLayerGroups;
    QList<CompositeLayerGroup*> mSortedLayerGroups;

    MapComposite *mParent;
    QPoint mPos;
    int mLevelOffset;
    Tiled::Map::Orientation mOrientRender;
    QPoint mOrientAdjustPos;
    QPoint mOrientAdjustTiles;
    int mMinLevel;
    int mMaxLevel;
    bool mVisible;
    bool mGroupVisible;
    bool mSavedGroupVisible;
    bool mSavedVisible;
    bool mHiddenDuringDrag;
    bool mShowBMPTiles;
    bool mShowMapTiles;
    bool mSavedShowBMPTiles;
    bool mSavedShowMapTiles;
    bool mIsAdjacentMap;

    Tiled::Internal::BmpBlender *mBmpBlender;

    QVector<MapComposite*> mAdjacentMaps;

    struct SubMapLoading {
        SubMapLoading(MapInfo *info, const QPoint &pos, int level) :
            mapInfo(info), pos(pos), level(level)
        {}
        MapInfo *mapInfo;
        QPoint pos;
        int level;
    };
    QList<SubMapLoading> mSubMapsLoading;

    QRegion mSuppressRgn;
    int mSuppressLevel;

#if 1 // ROAD_CRUD
    Tiled::TileLayer *mRoadLayer1;
    Tiled::TileLayer *mRoadLayer0;
#endif // ROAD_CRUD

public:
    MapComposite *root();
    MapComposite *rootOrAdjacent();
    int mKeepFloorLayerCount;
};

#endif // MAPCOMPOSITE_H
