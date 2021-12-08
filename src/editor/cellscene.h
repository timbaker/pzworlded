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

#ifndef CELLSCENE_H
#define CELLSCENE_H

#include "basegraphicsscene.h"

#include "sceneoverlay.h"
#include "worldcell.h"

#include "map.h"
#include "tile.h"

#include <QGraphicsItem>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_0>
#include <QOpenGLTexture>
#include <QPoint>
#include <QSet>
#include <QSizeF>
#include <QTimer>

#include <array>

class BaseCellSceneTool;
class CellDocument;
class CellGridItem;
class CompositeLayerGroup;
class MapBuildings;
class MapComposite;
class MapImage;
class MapInfo;
class ObjectItem;
class PropertyHolder;
class ResizeHandle;
class Road;
class SceneOverlay;
class SubMap;
class WorldDocument;
/*
class World;
class WorldCell;
class WorldCellLot;
class WorldCellObject;
class WorldCellObjectPoint;
class WorldObjectGroup;
*/
class InGameMapFeature;
class InGameMapFeatureItem;

namespace Tiled {
class MapRenderer;
class Layer;
class ZTileLayerGroup;
}

class ObjectLabelItem : public QGraphicsSimpleTextItem
{
public:
    ObjectLabelItem(ObjectItem *item, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    QPainterPath shape() const;
    bool contains(const QPointF &point) const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    void synch();

    ObjectItem *objectItem() const { return mItem; }

    void setShowSize(bool b) { mShowSize = b; }

private:
    ObjectItem *mItem;
    QColor mBgColor;
    bool mShowSize;
};

/**
  * Item that represents a WorldCellObject.
  */
class ObjectItem : public QGraphicsItem
{
public:
    ObjectItem(WorldCellObject *object, CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const override;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    QPainterPath shape() const override;

    void setEditable(bool editable);
    bool isEditable() const { return mIsEditable; }

    /**
      * Note: QGraphicsItem already defines these, but I'm avoiding any
      * QGraphicsScene() selection behavior.
      */
    void setSelected(bool selected);
    bool isSelected() const { return mIsSelected; }

    WorldCellObject *object() const { return mObject; }

    void synchWithObject();

    void setDragOffset(const QPointF &offset);
    QPointF dragOffset() const { return mDragOffset; }

    void setResizeDelta(const QSizeF &delta);
    QSizeF resizeDelta() const { return mResizeDelta; }

    QRectF tileBounds() const;

    bool isMouseOverHighlighted() const;

    virtual bool isSpawnPoint() const { return false; }
    virtual bool hoverToolCurrent() const;

    void setAdjacent(bool adjacent) { mAdjacent = adjacent; }
    bool isAdjacent() { return mAdjacent; }

    ObjectLabelItem* labelItem() const { return mLabel; }

    bool isPoint() const;
    bool isPolygon() const;
    bool isPolyline() const;

    int pointAt(qreal sceneX, qreal sceneY);

    void movePoint(int pointIndex, const WorldCellObjectPoint& point);

    QPolygonF createPolylineOutline();

protected:
    friend class ObjectPointHandle;
    friend class EditPolygonObjectTool;

    CellScene* mScene;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    WorldCellObject *mObject;
    QPolygonF mPolygon;
    QPolygonF mPolylineOutline;
    bool mSyncing;
    bool mIsEditable;
    bool mIsSelected;
    int mHoverRefCount;
    QPointF mDragOffset;
    QSizeF mResizeDelta;
    ResizeHandle *mResizeHandle;
    ObjectLabelItem *mLabel;
    bool mAdjacent;
    int mAddPointIndex;
    QPointF mAddPointPos;
};

/////

class ObjectPointHandle : public QGraphicsItem
{
public:
    ObjectPointHandle(ObjectItem *objectItem, int pointIndex);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
        Q_UNUSED(event)
        if (++mHoverRefCount == 1) {
            update();
        }
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
        Q_UNUSED(event)
        if (--mHoverRefCount == 0) {
            update();
        }
    }

    WorldCellObjectPoint geometryPoint() const {
        auto& coords = mObjectItem->object()->points();
        if (mPointIndex < 0 || mPointIndex >= coords.size())
            return { -1, -1 };
        return coords.at(mPointIndex);
    }

    bool isSelected() const;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

protected:
    ObjectItem *mObjectItem;
    int mPointIndex;
    WorldCellObjectPoint mOldPos;
    bool mMoveAllPoints = false;
    int mHoverRefCount = 0;
};

/////

class SpawnPointItem : public ObjectItem
{
public:
    SpawnPointItem(WorldCellObject *object, CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);
    bool isSpawnPoint() const { return true; }
    bool hoverToolCurrent() const;
};

/**
 * Item that represents a WorldCellLot.
 */
class SubMapItem : public QGraphicsItem
{
public:
    SubMapItem(MapComposite *map, WorldCellLot *lot, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    QPainterPath shape() const;

    void setEditable(bool editable);
    bool isEditable() const { return mIsEditable; }

    void subMapMoved();

    WorldCellLot *lot() const { return mLot; }
    MapComposite *subMap() const { return mMap; }

    void checkValidPos();

private:
    MapComposite *mMap;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    WorldCellLot *mLot;
    bool mIsEditable;
    bool mIsMouseOver;
    bool mIsValidPos;
};

/**
  * This item represents a road.
  */
class CellRoadItem : public QGraphicsItem
{
public:
    CellRoadItem(CellScene *scene, Road *road);

    QRectF boundingRect() const;

    QPainterPath shape() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    Road *road() const
    { return mRoad; }

    void synchWithRoad();

    void setSelected(bool selected);
    void setEditable(bool editable);

    void setDragging(bool dragging);
    void setDragOffset(const QPoint &offset);
    QPoint dragOffset() const { return mDragOffset; }

    QPolygonF polygon() const
    { return mPolygon; }

private:
    CellScene *mScene;
    QRectF mBoundingRect;
    Road *mRoad;
    bool mSelected;
    bool mEditable;
    bool mDragging;
    QPoint mDragOffset;
    QPolygonF mPolygon;
};

/**
 * Item that represents a map during drag-and-drop.
 */
class DnDItem : public QGraphicsItem
{
public:
    DnDItem(const QString &path, Tiled::MapRenderer *renderer, int level, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    QPainterPath shape() const;

    void setTilePosition(QPoint tilePos);

    void setHotSpot(const QPoint &pos);
    void setHotSpot(int x, int y) { setHotSpot(QPoint(x, y)); }
    QPoint hotSpot() { return mHotSpot; }

    QPoint dropPosition();

    MapInfo *mapInfo();

private:
    MapImage *mMapImage;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    QPoint mPositionInMap;
    QPoint mHotSpot;
    int mLevel;
};

/**
  * MiniMap item for drawing a single cell's map and Lots.
  */
class CellMiniMapItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    CellMiniMapItem(CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateCellImage();
    void updateLotImage(int index);
    void updateBoundingRect();

    void lotAdded(int index);
    void lotRemoved(int index);
    void lotMoved(int index);

    void cellContentsAboutToChange();
    void cellContentsChanged();

private slots:
    void sceneRectChanged(const QRectF &sceneRect);
    void mapImageChanged(MapImage *mapImage);

private:
    struct LotImage {
        LotImage()
            : mMapImage(0)
        {
        }

        LotImage(const QRectF &bounds, MapImage *mapImage)
            : mBounds(bounds)
            , mMapImage(mapImage)
        {
        }

        QRectF mBounds;
        MapImage *mMapImage;
    };

    CellScene *mScene;
    WorldCell *mCell;
    QRectF mBoundingRect;
    MapImage *mMapImage;
    QRectF mMapImageBounds;
    QVector<LotImage> mLotImages;
};

class LayerGroupVBO;

/**
  * Item that draws all the TileLayers on a single level.
  */
class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CellScene *scene, CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);
    ~CompositeLayerGroupItem() override;

    void synchWithTileLayers();

    QRectF boundingRect() const override;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *) override;

    CompositeLayerGroup *layerGroup() const { return mLayerGroup; }

private:
    CellScene *mScene;
    CompositeLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    friend class LayerGroupVBO;
    std::array<LayerGroupVBO*,9> mVBO;
};

class AdjacentMap : public QObject
{
    Q_OBJECT
public:
    AdjacentMap(CellScene *scene, WorldCell *cell);
    ~AdjacentMap();

    CellScene *scene() const
    { return mScene; }

    WorldDocument *worldDocument() const;

    WorldCell *cell() const
    { return mCell; }

    ObjectItem *itemForObject(WorldCellObject *obj);
    InGameMapFeatureItem *itemForFeature(InGameMapFeature *feature);

    void removeItems();

    void synchObjectItemVisibility();

private slots:
    void cellMapFileChanged(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void lotLevelChanged(WorldCellLot *lot);
    void cellLotReordered(WorldCellLot *lot);

    void cellObjectAdded(WorldCell *cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectMoved(WorldCellObject *obj);
    void cellObjectResized(WorldCellObject *obj);
    void objectLevelChanged(WorldCellObject *obj);
    void objectXXXXChanged(WorldCellObject *obj);
    void cellObjectGroupChanged(WorldCellObject *obj);
    void cellObjectReordered(WorldCellObject *obj);
    void cellObjectPointMoved(WorldCell *cell, int objectIndex, int pointIndex);
    void cellObjectPointsChanged(WorldCell *cell, int index);

    void propertiesChanged(PropertyHolder* ph);

    void inGameMapFeatureAdded(WorldCell* cell, int index);
    void inGameMapFeatureAboutToBeRemoved(WorldCell* cell, int index);
    void inGameMapPointMoved(WorldCell* cell, int featureIndex, int coordIndex, int pointIndex);
    void inGameMapPropertiesChanged(WorldCell* cell, int featureIndex);
    void inGameMapGeometryChanged(WorldCell* cell, int featureIndex);

    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

    void sceneRectChanged();

    // FIXME: world resizing

private:
    void loadMap();
    bool alreadyLoading(WorldCellLot *lot);
    QRectF lotSceneBounds(WorldCellLot *lot);
    void setZOrder();
    bool shouldObjectItemBeVisible(ObjectItem *item);

    struct LoadingSubMap {
        LoadingSubMap(WorldCellLot *lot, MapInfo *mapInfo) :
            lot(lot),
            mapInfo(mapInfo)
        {}
        WorldCellLot *lot;
        MapInfo *mapInfo;
    };
    QList<LoadingSubMap> mSubMapsLoading;

    CellScene *mScene;
    WorldCell *mCell;
    MapComposite *mMapComposite;
    MapInfo *mMapInfo;
    QMap<WorldCellLot*,MapComposite*> mLotToMC;
    QGraphicsItem *mObjectItemParent;
    QList<ObjectItem*> mObjectItems;
    QGraphicsItem *mInGameMapFeatureParent;
    QList<InGameMapFeatureItem*> mInGameMapFeatureItems;
};

class QOpenGLContext;
class QOpenGLTexture;

class TilesetTexture
{
public:
#define TILESET_TEXTURE_GL 0
#if TILESET_TEXTURE_GL
    QOpenGLTexture *mTexture = nullptr;
#else
    int mID = -1; // OpenGL ID
#endif
    int mChangeCount = -1;
};

class TilesetTexturesPerContext
{
public:
    ~TilesetTexturesPerContext();

    QOpenGLContext *mContext;
    QMap<QString,int> mChanged;
    QSet<QString> mMissing;
    QMap<QString, TilesetTexture*> mTextureMap;
    QList<TilesetTexture*> mTextures;
};

class TilesetTextures : public QObject
{
    Q_OBJECT
public:
    TilesetTexture *get(const QString& tilesetName, const QList<Tiled::Tileset*> &tilesets);
    Tiled::Tileset *findTileset(const QString& tilesetName, const QList<Tiled::Tileset*> &tilesets);

public slots:
    void aboutToBeDestroyed();
    void tilesetChanged(Tiled::Tileset *tileset);

private:
    QMap<QOpenGLContext*,TilesetTexturesPerContext*> mContextToTextures;
    bool mConnected = false;
};

struct VBOTile
{
    MapComposite *mSubMap = nullptr; // This tile belongs to a Lot.
    MapComposite *mHideIfVisible = nullptr; // This tile belongs to a Cell, hide it if the Lot=mHideIfVisible is visible.
    int mLayerIndex;
    QRect mRect;
    QString mTilesetName;
    Tiled::Tile::UVST mAtlasUVST;
    TilesetTexture *mTexture = nullptr;
};

const int VBO_SQUARES = 10 * 3;
const int VBO_PER_CELL = 300 / VBO_SQUARES;

struct VBOTiles
{
    VBOTiles()
        : mIndexBuffer(QOpenGLBuffer::Type::IndexBuffer)
        , mVertexBuffer(QOpenGLBuffer::Type::VertexBuffer)
    {
        mTileFirst.fill(-1);
        mTileCount.fill(0);
    }

    QOpenGLBuffer mIndexBuffer;
    QOpenGLBuffer mVertexBuffer;

    bool mCreated = false;
    bool mGathered = false;
    QRect mBounds;
    QList<VBOTile> mTiles;
    std::array<int, VBO_SQUARES * VBO_SQUARES> mTileFirst; // Index into mTiles, or -1 for none. Per-square.
    std::array<int, VBO_SQUARES * VBO_SQUARES> mTileCount; // Number of VBOTile in mTiles. Per-square.
};

class MapCompositeVBO;

class LayerGroupVBO : public QObject, QOpenGLFunctions_3_0
{
    Q_OBJECT
public:
    LayerGroupVBO();
    ~LayerGroupVBO();

    void paint(QPainter *painter, Tiled::MapRenderer *renderer, const QRectF& exposedRect);
    void paint2(QPainter *painter, Tiled::MapRenderer *renderer, const QRectF& exposedRect);
    void gatherTiles(Tiled::MapRenderer *renderer, const QRectF& exposedRect, QList<VBOTiles *> &exposedTiles);
    VBOTiles *getTilesFor(const QPoint& square, bool bCreate);
    void getSquaresInRect(Tiled::MapRenderer *renderer, const QRectF &exposedRect, QList<QPoint>& out);
    bool isEmpty() const;

public slots:
    void aboutToBeDestroyed();

public:
    MapCompositeVBO *mMapCompositeVBO = nullptr;
    QOpenGLContext *mContext = nullptr;
    CompositeLayerGroupItem *mLayerGroupItem = nullptr;
    int mChangeCount = -1;
    CompositeLayerGroup *mLayerGroup = nullptr;
    bool mCreated = false;
    bool mDestroying = false;
    std::array<VBOTiles*,VBO_PER_CELL * VBO_PER_CELL> mTiles;
};

class MapCompositeVBO
{
public:
    MapCompositeVBO();
    ~MapCompositeVBO();

    LayerGroupVBO *getLayerVBO(CompositeLayerGroupItem *item);

    CellScene *mScene = nullptr;
    MapComposite *mMapComposite = nullptr;
    QRect mBounds;
    std::array<LayerGroupVBO*,8> mLayerVBOs;
    QList<Tiled::Tileset*> mUsedTilesets;
    QMap<QString,int> mLayerNameToIndex;
};

class CellScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    explicit CellScene(QObject *parent = nullptr);
    ~CellScene();

    void setTool(AbstractTool *tool);

    void viewTransformChanged(BaseGraphicsView *view);

    Tiled::Map *map() const { return mMap; }

    MapComposite *mapComposite() const { return mMapComposite; }

    void setDocument(CellDocument *doc);
    CellDocument *document() const { return mDocument; }
    WorldDocument *worldDocument() const;
    World *world() const;

    WorldCell *cell() const;

    SubMapItem *itemForLot(WorldCellLot *lot);
    WorldCellLot *lotForItem(SubMapItem *item);
    QList<SubMapItem*> subMapItemsUsingMapInfo(MapInfo *mapInfo);

    ObjectItem *itemForObject(WorldCellObject *obj);
    InGameMapFeatureItem* itemForInGameMapFeature(InGameMapFeature* feature);

    void setSelectedSubMapItems(const QSet<SubMapItem*> &selected);
    const QSet<SubMapItem*> &selectedSubMapItems() const
    { return mSelectedSubMapItems; }

    void setSelectedObjectItems(const QSet<ObjectItem*> &selected);
    const QSet<ObjectItem*> &selectedObjectItems() const
    { return mSelectedObjectItems; }

    void setSelectedInGameMapFeatureItems(const QSet<InGameMapFeatureItem*> &selected);
    const QSet<InGameMapFeatureItem*> &selectedInGameMapFeatureItems() const
    { return mSelectedFeatureItems; }

    void setGraphicsSceneZOrder();

    void setSubMapVisible(WorldCellLot *lot, bool visible);
    void setObjectVisible(WorldCellObject *obj, bool visible);
    void setInGameMapFeatureVisible(InGameMapFeature *feature, bool visible);

    void setLevelOpacity(int level, qreal opacity);
    qreal levelOpacity(int level);

    void setLayerOpacity(int level, Tiled::TileLayer *tl, qreal opacity);
    qreal layerOpacity(int level, Tiled::TileLayer *tl) const;

    void setHighlightRoomPosition(const QPoint &tilePos);
    QRegion getBuildingRegion(const QPoint &tilePos, QRegion &roomRgn);
    QString roomNameAt(const QPointF &scenePos);

    void keyPressEvent(QKeyEvent *event);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);

    Tiled::MapRenderer *renderer() const { return mRenderer; }

    QPoint pixelToRoadCoords(qreal x, qreal y) const;

    inline QPoint pixelToRoadCoords(const QPointF &point) const
    { return pixelToRoadCoords(point.x(), point.y()); }

    QPointF roadToSceneCoords(const QPoint &pt) const;
    QPolygonF roadRectToScenePolygon(const QRect &roadRect) const;

    CellRoadItem *itemForRoad(Road *road);

    QList<Road*> roadsInRect(const QRectF &bounds);

    static const int ZVALUE_GRID;
    static const int ZVALUE_ROADITEM_CREATING;
    static const int ZVALUE_ROADITEM_SELECTED;
    static const int ZVALUE_ROADITEM_UNSELECTED;

    bool isDestroying() const
    { return mDestroying; }

protected:
    void loadMap();
    void updateCurrentLevelHighlight();
    bool shouldObjectItemBeVisible(ObjectItem *item);
    void synchAdjacentMapObjectItemVisibility();
    void sortSubMaps();

    typedef Tiled::Tileset Tileset;
signals:
    void mapContentsChanged();

public slots:
    void tilesetChanged(Tileset *tileset);

    bool mapAboutToChange(MapInfo *mapInfo);
    bool mapChanged(MapInfo *mapInfo);
    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

    void cellAdded(WorldCell *cell);
    void cellAboutToBeRemoved(WorldCell *cell);

    void cellMapFileChanged();
    void cellContentsChanged();

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void lotLevelChanged(WorldCellLot *lot);
    void selectedLotsChanged();
    void cellLotReordered(WorldCellLot *lot);

    void cellObjectAdded(WorldCell *cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectMoved(WorldCellObject *obj);
    void cellObjectResized(WorldCellObject *obj);
    void objectLevelChanged(WorldCellObject *obj);
    void objectXXXXChanged(WorldCellObject *obj);
    void cellObjectGroupChanged(WorldCellObject *obj);
    void cellObjectReordered(WorldCellObject *obj);
    void cellObjectPointMoved(WorldCell* cell, int objectIndex, int pointIndex);
    void cellObjectPointsChanged(WorldCell* cell, int objectIndex);
    void selectedObjectsChanged();
    void selectedObjectPointsChanged();

    void layerVisibilityChanged(Tiled::Layer *layer);
    void layerGroupAdded(int level);
    void layerGroupVisibilityChanged(Tiled::ZTileLayerGroup *layerGroup);
    void lotLevelVisibilityChanged(int level);
    void objectLevelVisibilityChanged(int level);

    void objectGroupVisibilityChanged(WorldObjectGroup *og, int level);
    void objectGroupColorChanged(WorldObjectGroup *og);
    void objectGroupReordered(int index);

    void currentLevelChanged(int index);
    void setGridVisible(bool visible);
    void gridColorChanged(const QColor &gridColor);
    void showObjectsChanged(bool show);
    void showObjectNamesChanged(bool show);
    void setHighlightCurrentLevel(bool highlight);
    void highlightRoomUnderPointerChanged(bool highlight);
    void handlePendingUpdates();

    void propertiesChanged(PropertyHolder* ph);

    void inGameMapFeatureAdded(WorldCell* cell, int index);
    void inGameMapFeatureAboutToBeRemoved(WorldCell* cell, int index);
    void inGameMapPointMoved(WorldCell* cell, int featureIndex, int coordIndex, int pointIndex);
    void inGameMapGeometryChanged(WorldCell* cell, int featureIndex);
    void inGameMapHoleAdded(WorldCell* cell, int featureIndex, int holeIndex);
    void inGameMapHoleRemoved(WorldCell* cell, int featureIndex, int holeIndex);
    void selectedInGameMapFeaturesChanged();
    void selectedInGameMapPointsChanged();

    void roadAdded(int index);
    void roadRemoved(Road *road);
    void roadCoordsChanged(int index);
    void roadWidthChanged(int index);
    void selectedRoadsChanged();
    void roadsChanged();

    void mapCompositeNeedsSynch();

    MapCompositeVBO *mapCompositeVBO()
    {
        return &mMapCompositeVBO[4];
    }

    MapCompositeVBO *mapCompositeVBO(int adjacent)
    {
        return &mMapCompositeVBO[adjacent];
    }

public:
    enum Pending {
        None = 0,
        AllGroups = 0x01,
        Bounds = 0x02,
        LotVisibility = 0x04,
        ObjectVisibility = 0x08,
        Paint = 0x10,
        Synch = 0x20,
        ZOrder = 0x40
    };
    Q_DECLARE_FLAGS(PendingFlags, Pending)

private:
    void synchLayerGroupsLater();

    struct LoadingSubMap {
        LoadingSubMap(WorldCellLot *lot, MapInfo *mapInfo) :
            lot(lot),
            mapInfo(mapInfo)
        {}
        WorldCellLot *lot;
        MapInfo *mapInfo;
    };
    QList<LoadingSubMap> mSubMapsLoading;

    QList<AdjacentMap*> mAdjacentMaps;
    void initAdjacentMaps();

    Tiled::Map *mMap;
    MapInfo *mMapInfo;
    MapComposite *mMapComposite;
    QVector<QGraphicsItem*> mLayerItems;
    QMap<int,CompositeLayerGroupItem*> mTileLayerGroupItems;
    CellDocument *mDocument;
    Tiled::MapRenderer *mRenderer;
    DnDItem *mDnDItem;
    QList<SubMapItem*> mSubMapItems;
    QSet<SubMapItem*> mSelectedSubMapItems;
    QList<ObjectItem*> mObjectItems;
    QSet<ObjectItem*> mSelectedObjectItems;
    QList<CellRoadItem*> mRoadItems;
    QSet<CellRoadItem*> mSelectedRoadItems;
    QList<InGameMapFeatureItem*> mFeatureItems;
    QSet<InGameMapFeatureItem*> mSelectedFeatureItems;
    QGraphicsRectItem *mDarkRectangle;
    CellGridItem *mGridItem;
    bool mHighlightCurrentLevel;
    bool mWasHighlightCurrentLevel;
    QGraphicsPolygonItem *mMapBordersItem;

    QPoint mHighlightRoomPosition;
    MapBuildings *mMapBuildings;
    bool mMapBuildingsInvalid;

    void doLater(PendingFlags flags);
    PendingFlags mPendingFlags;
    bool mPendingActive;
    bool mPendingDefer;
    QList<CompositeLayerGroupItem*> mPendingGroupItems;

    BaseCellSceneTool *mActiveTool;

    LightSwitchOverlays mLightSwitchOverlays;
    friend class LightSwitchOverlays;

    WaterFlowOverlay* mWaterFlowOverlay;

    bool mDestroying;
    std::array<MapCompositeVBO,9> mMapCompositeVBO;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CellScene::PendingFlags)

#endif // CELLSCENE_H
