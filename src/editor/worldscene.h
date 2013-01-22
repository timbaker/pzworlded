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

#ifndef WORLDSCENE_H
#define WORLDSCENE_H

#include "basegraphicsscene.h"

#include "worldcell.h"

#include <QGraphicsItem>
#include <QPair>
#include <QPolygonF>

class BaseWorldSceneTool;
class MapImage;
class MapInfo;
class PasteCellsTool;
class Road;
class World;
class WorldBMP;
class WorldCellTool;
class WorldDocument;
class WorldScene;

#define GRID_WIDTH (512)
#define GRID_HEIGHT (256)

/**
  * Item that draws the grid-lines in a WorldScene.
  */
class WorldGridItem : public QGraphicsItem
{
public:
    WorldGridItem(WorldScene *scene);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateBoundingRect();

private:
    WorldScene *mScene;
    QRectF mBoundingRect;
};

/**
  * Item that draws all cell coordinates in a WorldScene.
  */
class WorldCoordItem : public QGraphicsItem
{
public:
    WorldCoordItem(WorldScene *scene);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateBoundingRect();

private:
    WorldScene *mScene;
    QRectF mBoundingRect;
};

/**
  * Item that draws the selection over all selected cells in a WorldScene.
  */
class WorldSelectionItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    WorldSelectionItem(WorldScene *scene);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateBoundingRect();

    void highlightCellDuringDnD(const QPoint &pos);

private slots:
    void selectedCellsChanged();

private:
    WorldScene *mScene;
    QRectF mBoundingRect;
    QPoint mHighlightedCellDuringDnD;
};

/**
  * Base item for drawing a single cell's map and Lots.
  */
class BaseCellItem : public QGraphicsItem
{
public:
    BaseCellItem(WorldScene *scene, QGraphicsItem *parent = 0);

    // Ugliness to work around not being allowed to call pure-virtual methods
    // from the constructor.
    void initialize();

    QRectF boundingRect() const;

    QPainterPath shape() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    virtual QPoint cellPos() const = 0;
    virtual QString mapFilePath() const = 0;
    virtual const QList<WorldCellLot*> &lots() const = 0;

    void updateCellImage();
    void updateLotImage(int index);
    void updateBoundingRect();

    void mapImageChanged(MapImage *mapImage);

    void worldResized();

protected:
    void calcMapImageBounds();
    void calcLotImageBounds(int index);
    QPointF calcLotImagePosition(WorldCellLot *lot, int scaledImageWidth, MapImage *mapImage);

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

    WorldScene *mScene;
    QRectF mBoundingRect;
    MapImage *mMapImage;
    QRectF mMapImageBounds;
    QVector<LotImage> mLotImages;
    QPointF mDrawOffset;
};

/**
  * There is one of these for each cell in the world.
  */
class WorldCellItem : public BaseCellItem
{
public:
    WorldCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent = 0);

    QPoint cellPos() const { return mCell->pos(); }
    QString mapFilePath() const { return mCell->mapFilePath(); }
    const QList<WorldCellLot*> &lots() const { return mCell->lots(); }

    WorldCell *cell() const { return mCell; }

    void lotAdded(int index);
    void lotRemoved(int index);
    void lotMoved(WorldCellLot *lot);
    void cellContentsChanged();

    void mapFileCreated(const QString &path);

protected:
    WorldCell *mCell;
};

/**
  * This item is used when dragging cells to a new location.
  */
class DragCellItem : public WorldCellItem
{
public:
    DragCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent = 0);

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void setDragOffset(const QPointF &offset);
};

/**
  * This item is used by the PasteCellsTool.
  */
class PasteCellItem : public BaseCellItem
{
public:
    PasteCellItem(WorldCellContents *contents, WorldScene *scene, QGraphicsItem *parent = 0);

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    QPoint cellPos() const { return mContents->pos(); }
    QString mapFilePath() const { return mContents->mapFilePath(); }
    const QList<WorldCellLot*> &lots() const { return mContents->lots(); }

    void setDragOffset(const QPointF &offset);

    WorldCellContents *contents() const { return mContents; }

private:
    WorldCellContents *mContents;
};

/**
  * This item is used when dragging a map file from the Maps Dock.
  */
class DragMapImageItem : public BaseCellItem
{
public:
    DragMapImageItem(MapInfo *mapInfo, WorldScene *scene, QGraphicsItem *parent = 0);

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    QPoint cellPos() const;
    QString mapFilePath() const;
    const QList<WorldCellLot*> &lots() const;

    void setScenePos(const QPointF &scenePos);
    QPoint dropPos() const;

private:
    MapInfo *mMapInfo;
    QPoint mDropPos;
    QList<WorldCellLot*> mLots;
};

/**
  * This item represents a road.
  */
class WorldRoadItem : public QGraphicsItem
{
public:
    WorldRoadItem(WorldScene *scene, Road *road);

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

    QPolygonF polygon() const;

private:
    WorldScene *mScene;
    QRectF mBoundingRect;
    Road *mRoad;
    bool mSelected;
    bool mEditable;
    bool mDragging;
    QPoint mDragOffset;
};

class WorldBMPItem : public QGraphicsItem
{
public:
    WorldBMPItem(WorldScene *scene, WorldBMP *bmp);

    QRectF boundingRect() const;

    QPainterPath shape() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    WorldBMP *bmp() const
    { return mBMP; }

    void synchWithBMP();

    void setSelected(bool selected);

    void setDragging(bool dragging);
    void setDragOffset(const QPoint &offset);

    QPoint dragOffset() const
    { return mDragOffset; }

    QPolygonF polygon() const;

private:
    WorldScene *mScene;
    WorldBMP *mBMP;
    MapImage *mMapImage;
    QRectF mMapImageBounds;
    bool mSelected;
    bool mDragging;
    QPoint mDragOffset;
};

class WorldScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    explicit WorldScene(WorldDocument *worldDoc, QObject *parent = 0);
    
    static const int ZVALUE_CELLITEM;
    static const int ZVALUE_ROADITEM_UNSELECTED;
    static const int ZVALUE_ROADITEM_SELECTED;
    static const int ZVALUE_ROADITEM_CREATING;
    static const int ZVALUE_GRIDITEM;
    static const int ZVALUE_SELECTIONITEM;
    static const int ZVALUE_COORDITEM;
    static const int ZVALUE_DNDITEM;

    void setTool(AbstractTool *tool);

    QPointF pixelToCellCoords(qreal x, qreal y) const;

    inline QPointF pixelToCellCoords(const QPointF &point) const
    { return pixelToCellCoords(point.x(), point.y()); }

    QPoint pixelToCellCoordsInt(const QPointF &point) const;

    WorldCell *pointToCell(const QPointF &point);

    QPointF cellToPixelCoords(qreal x, qreal y) const;

    inline QPointF cellToPixelCoords(const QPointF &point) const
    { return cellToPixelCoords(point.x(), point.y()); }

    QRectF boundingRect(const QRect &rect) const;
    QRectF boundingRect(const QPoint &pos) const;
    QRectF boundingRect(int x, int y) const;

    QPolygonF cellRectToPolygon(const QRectF &rect) const;
    QPolygonF cellRectToPolygon(WorldCell *cell) const;

    WorldDocument *worldDocument() const { return mWorldDoc; }

    World *world() const;
    WorldCellItem *itemForCell(WorldCell *cell);
    WorldCellItem *itemForCell(int x, int y);

    QPoint pixelToRoadCoords(qreal x, qreal y) const;

    inline QPoint pixelToRoadCoords(const QPointF &point) const
    { return pixelToRoadCoords(point.x(), point.y()); }

    QPointF roadToSceneCoords(const QPoint &pt) const;
    QPolygonF roadRectToScenePolygon(const QRect &roadRect) const;

    WorldRoadItem *itemForRoad(Road *road);

    QList<Road*> roadsInRect(const QRectF &bounds);

    QList<WorldBMP*> bmpsInRect(const QRectF &cellRect);

    void pasteCellsFromClipboard();

signals:
    
public slots:
    void worldAboutToResize(const QSize &newSize);
    void worldResized(const QSize &oldSize);

    void selectedCellsChanged();
    void cellMapFileChanged(WorldCell *cell);
    void cellLotAdded(WorldCell *cell, int index);
    void cellLotAboutToBeRemoved(WorldCell *cell, int index);
    void cellLotMoved(WorldCellLot *lot);
    void cellContentsChanged(WorldCell *cell);
    void setShowGrid(bool show);
    void setShowCoordinates(bool show);
    void setShowBMPs(bool show);

    void selectedRoadsChanged();
    void roadAdded(int index);
    void roadAboutToBeRemoved(int index);
    void roadCoordsChanged(int index);
    void roadWidthChanged(int index);

    void selectedBMPsChanged();
    void bmpAdded(int index);
    void bmpAboutToBeRemoved(int index);
    void bmpCoordsChanged(int index);

    WorldBMP *pointToBMP(const QPointF &scenePos);
    WorldBMPItem *itemForBMP(WorldBMP *bmp);

    void mapFileCreated(const QString &path);
    void mapImageChanged(MapImage *mapImage);

protected:
    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);

private:
    WorldDocument *mWorldDoc;
    WorldGridItem *mGridItem;
    WorldCoordItem *mCoordItem;
    WorldSelectionItem *mSelectionItem;
    QVector<WorldCellItem*> mSelectedCellItems;
    QVector<WorldCellItem*> mCellItems;
    PasteCellsTool *mPasteCellsTool;
    BaseWorldSceneTool *mActiveTool;
    DragMapImageItem *mDragMapImageItem;
    QList<WorldRoadItem*> mRoadItems;
    QSet<WorldRoadItem*> mSelectedRoadItems;
    QList<WorldBMPItem*> mBMPItems;
    QSet<WorldBMPItem*> mSelectedBMPItems;
    WorldBMPItem *mDragBMPItem;
    bool mBMPToolActive;
};

#endif // WORLDSCENE_H
