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

#include "map.h"

#include <QGraphicsItem>
#include <QPoint>
#include <QSet>
#include <QSizeF>
#include <QTimer>

class BaseCellSceneTool;
class CellDocument;
class CellGridItem;
class CompositeLayerGroup;
class MapComposite;
class MapImage;
class MapInfo;
class ResizeHandle;
class SubMap;
class WorldCell;
class WorldCellLot;
class WorldCellObject;
class WorldDocument;
class WorldObjectGroup;

namespace Tiled {
class MapRenderer;
class Layer;
class ZTileLayerGroup;
};

/**
  * Item that represents a WorldCellObject.
  */
class ObjectItem : public QGraphicsItem
{
public:
    ObjectItem(WorldCellObject *object, CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    QPainterPath shape() const;

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

private:
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    WorldCellObject *mObject;
    bool mIsEditable;
    bool mIsSelected;
    bool mIsMouseOver;
    QPointF mDragOffset;
    QSizeF mResizeDelta;
    ResizeHandle *mResizeHandle;
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

private:
    MapComposite *mMap;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    WorldCellLot *mLot;
    bool mIsEditable;
    bool mIsMouseOver;
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

/**
  * Item that draws all the TileLayers on a single level.
  */
class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);

    void synchWithTileLayers();

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

    CompositeLayerGroup *layerGroup() const { return mLayerGroup; }

private:
    CompositeLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
};

class CellScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    explicit CellScene(QObject *parent = 0);
    ~CellScene();

    void setTool(AbstractTool *tool);

    Tiled::Map *map() const { return mMap; }

    MapComposite *mapComposite() const { return mMapComposite; }

    void setDocument(CellDocument *doc);
    CellDocument *document() const { return mDocument; }
    WorldDocument *worldDocument() const;

    WorldCell *cell() const;

    SubMapItem *itemForLot(WorldCellLot *lot);
    WorldCellLot *lotForItem(SubMapItem *item);

    ObjectItem *itemForObject(WorldCellObject *obj);

    void setSelectedSubMapItems(const QSet<SubMapItem*> &selected);
    const QSet<SubMapItem*> &selectedSubMapItems() const
    { return mSelectedSubMapItems; }

    void setSelectedObjectItems(const QSet<ObjectItem*> &selected);
    const QSet<ObjectItem*> &selectedObjectItems() const
    { return mSelectedObjectItems; }

    int levelZOrder(int level);
    void setGraphicsSceneZOrder();

    void setSubMapVisible(WorldCellLot *lot, bool visible);
    void setObjectVisible(WorldCellObject *obj, bool visible);

    void keyPressEvent(QKeyEvent *event);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    void dropEvent(QGraphicsSceneDragDropEvent *event);

    Tiled::MapRenderer *renderer() const { return mRenderer; }

protected:
    void loadMap();
    void updateCurrentLevelHighlight();

public slots:
    void cellMapFileChanged();
    void cellContentsChanged();

    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void lotLevelChanged(WorldCellLot *lot);
    void selectedLotsChanged();

    void cellObjectAdded(WorldCell *cell, int index);
    void cellObjectAboutToBeRemoved(WorldCell *cell, int index);
    void cellObjectMoved(WorldCellObject *obj);
    void cellObjectResized(WorldCellObject *obj);
    void objectLevelChanged(WorldCellObject *obj);
    void objectXXXXChanged(WorldCellObject *obj);
    void cellObjectGroupChanged(WorldCellObject *obj);
    void cellObjectReordered(WorldCellObject *obj);
    void selectedObjectsChanged();

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
    void setHighlightCurrentLevel(bool highlight);
    void handlePendingUpdates();

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
    QGraphicsRectItem *mDarkRectangle;
    CellGridItem *mGridItem;
    bool mHighlightCurrentLevel;
    bool mWasHighlightCurrentLevel;

    void doLater(PendingFlags flags);
    PendingFlags mPendingFlags;
    bool mPendingActive;
    QList<CompositeLayerGroupItem*> mPendingGroupItems;

    BaseCellSceneTool *mActiveTool;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CellScene::PendingFlags)

#endif // CELLSCENE_H
