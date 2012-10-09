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

#include "worldscene.h"

#include "basegraphicsview.h"
#include "celldocument.h"
#include "documentmanager.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "preferences.h"
#include "scenetools.h"
#include "toolmanager.h"
#include "undoredo.h"
#include "world.h"
#include "worlddocument.h"
#include "zoomable.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QUrl>

WorldScene::WorldScene(WorldDocument *worldDoc, QObject *parent)
    : BaseGraphicsScene(WorldSceneType, parent)
    , mWorldDoc(worldDoc)
    , mGridItem(new WorldGridItem(this))
    , mCoordItem(new WorldCoordItem(this))
    , mSelectionItem(new WorldSelectionItem(this))
    , mPasteCellsTool(0)
    , mActiveTool(0)

{
    setBackgroundBrush(Qt::darkGray);

    mGridItem->setZValue(2);
    addItem(mGridItem);

    mSelectionItem->setZValue(3);
    addItem(mSelectionItem);

    mCoordItem->setZValue(4);
    addItem(mCoordItem);

    connect(mWorldDoc, SIGNAL(selectedCellsChanged()),
            SLOT(selectedCellsChanged()));
    connect(mWorldDoc, SIGNAL(cellMapFileChanged(WorldCell*)),
            SLOT(cellMapFileChanged(WorldCell*)));
    connect(mWorldDoc, SIGNAL(cellLotAdded(WorldCell*,int)),
            SLOT(cellLotAdded(WorldCell*,int)));
    connect(mWorldDoc, SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)),
            SLOT(cellLotAboutToBeRemoved(WorldCell*,int)));
    connect(mWorldDoc, SIGNAL(cellLotMoved(WorldCellLot*)),
            SLOT(cellLotMoved(WorldCellLot*)));
    connect(mWorldDoc, SIGNAL(cellContentsChanged(WorldCell*)),
            SLOT(cellContentsChanged(WorldCell*)));

    connect(mWorldDoc, SIGNAL(selectedRoadsChanged()),
            SLOT(selectedRoadsChanged()));
    connect(mWorldDoc, SIGNAL(roadAdded(int)),
            SLOT(roadAdded(int)));
    connect(mWorldDoc, SIGNAL(roadAboutToBeRemoved(int)),
            SLOT(roadAboutToBeRemoved(int)));
    connect(mWorldDoc, SIGNAL(roadCoordsChanged(int)),
            SLOT(roadCoordsChanged(int)));
    connect(mWorldDoc, SIGNAL(roadWidthChanged(int)),
            SLOT(roadWidthChanged(int)));

    mGridItem->updateBoundingRect();
    setSceneRect(mGridItem->boundingRect());
    mCoordItem->updateBoundingRect();

    mCellItems.resize(world()->width() * world()->height());
    for (int y = 0; y < world()->height(); y++) {
        for (int x = 0; x < world()->width(); x++) {
            WorldCell *cell = world()->cellAt(x, y);
            WorldCellItem *item = new WorldCellItem(cell, this);
            addItem(item);
            item->setZValue(1); // below mGridItem
            mCellItems[y * world()->width() + x] = item;
        }
    }

    foreach (Road *road, world()->roads()) {
        RoadItem *item = new RoadItem(this, road);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
        addItem(item);
        mRoadItems += item;
    }

    Preferences *prefs = Preferences::instance();
    mGridItem->setVisible(prefs->showWorldGrid());
    mCoordItem->setVisible(prefs->showCoordinates());
    connect(prefs, SIGNAL(showWorldGridChanged(bool)), SLOT(setShowGrid(bool)));
    connect(prefs, SIGNAL(showCoordinatesChanged(bool)), SLOT(setShowCoordinates(bool)));

    mPasteCellsTool = PasteCellsTool::instance();
}

void WorldScene::setTool(AbstractTool *tool)
{
    BaseWorldSceneTool *worldTool = tool ? tool->asWorldTool() : 0;

    if (mActiveTool == worldTool)
        return;

    if (mActiveTool) {
        mActiveTool->deactivate();
    }

    mActiveTool = worldTool;

    if (mActiveTool) {
        mActiveTool->activate();
    }

    if (mActiveTool != EditRoadTool::instance())
        foreach (RoadItem *item, mRoadItems)
            item->setEditable(false);
}

World *WorldScene::world() const
{
    return mWorldDoc ? mWorldDoc->world() : 0;
}

WorldCellItem *WorldScene::itemForCell(WorldCell *cell)
{
    return itemForCell(cell->x(), cell->y());
}

WorldCellItem *WorldScene::itemForCell(int x, int y)
{
    if (!world()->contains(x, y))
        return 0;
    return mCellItems[y * world()->width() + x];
}

QPoint WorldScene::pixelToRoadCoords(qreal x, qreal y) const
{
    QPointF cellPos = pixelToCellCoords(x, y);
    return QPoint(cellPos.x() * 300, cellPos.y() * 300);
}

QPointF WorldScene::roadToSceneCoords(const QPoint &pt) const
{
    return cellToPixelCoords(QPointF(pt) / 300.0);
}

QPolygonF WorldScene::roadRectToScenePolygon(const QRect &roadRect) const
{
    QPolygonF polygon;
    polygon += roadToSceneCoords(roadRect.topLeft());
    polygon += roadToSceneCoords(roadRect.topRight());
    polygon += roadToSceneCoords(roadRect.bottomRight());
    polygon += roadToSceneCoords(roadRect.bottomLeft());
    return polygon;
}

RoadItem *WorldScene::itemForRoad(Road *road)
{
    foreach (RoadItem *item, mRoadItems)
        if (item->road() == road)
            return item;

    return 0;
}

QList<Road *> WorldScene::roadsInRect(const QRectF &bounds)
{
    QPolygonF polygon;
    polygon += cellToPixelCoords(bounds.topLeft());
    polygon += cellToPixelCoords(bounds.topRight());
    polygon += cellToPixelCoords(bounds.bottomRight());
    polygon += cellToPixelCoords(bounds.bottomLeft());

    QList<Road*> result;
    foreach (QGraphicsItem *item, items(polygon)) {
        if (RoadItem *roadItem = dynamic_cast<RoadItem*>(item))
            result += roadItem->road();
    }
    return result;
}

void WorldScene::pasteCellsFromClipboard()
{
    ToolManager::instance()->selectTool(mPasteCellsTool);
//    mActiveTool = mPasteCellsTool;
}

QPointF WorldScene::pixelToCellCoords(qreal x, qreal y) const
{
    const int tileWidth = GRID_WIDTH;
    const int tileHeight = GRID_HEIGHT;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= world()->height() * tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

WorldCell *WorldScene::pointToCell(const QPointF &point)
{
    QPoint cellCoords = pixelToCellCoordsInt(point);
    if (world()->contains(cellCoords))
        return world()->cellAt(cellCoords);
    return NULL;
}

QPointF WorldScene::cellToPixelCoords(qreal x, qreal y) const
{
    const int tileWidth = GRID_WIDTH;
    const int tileHeight = GRID_HEIGHT;
    const int originX = world()->height() * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

QRectF WorldScene::boundingRect(const QRect &rect) const
{
    const int tileWidth = GRID_WIDTH;
    const int tileHeight = GRID_HEIGHT;

    const int originX = world()->height() * tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

    return QRect(pos, size);
}

QRectF WorldScene::boundingRect(const QPoint &pos) const
{
    return boundingRect(QRect(pos, QSize(1,1)));
}

QRectF WorldScene::boundingRect(int x, int y) const
{
    return boundingRect(QRect(x, y, 1, 1));
}

QPolygonF WorldScene::cellRectToPolygon(const QRectF &rect) const
{
    const QPointF topLeft = cellToPixelCoords(rect.topLeft());
    const QPointF topRight = cellToPixelCoords(rect.topRight());
    const QPointF bottomRight = cellToPixelCoords(rect.bottomRight());
    const QPointF bottomLeft = cellToPixelCoords(rect.bottomLeft());
    QPolygonF polygon;
    polygon << topLeft << topRight << bottomRight << bottomLeft;
    return polygon;
}

QPolygonF WorldScene::cellRectToPolygon(WorldCell *cell) const
{
    return cellRectToPolygon(QRect(cell->pos(), QSize(1,1)));
}

void WorldScene::selectedCellsChanged()
{
    foreach (WorldCellItem *item, mSelectedCellItems)
        item->setSelected(false);

    mSelectedCellItems.clear();
    foreach (WorldCell *cell, mWorldDoc->selectedCells()) {
        itemForCell(cell)->setSelected(true);
        mSelectedCellItems += itemForCell(cell);
    }
}

void WorldScene::cellMapFileChanged(WorldCell *cell)
{
    itemForCell(cell)->updateCellImage();
    itemForCell(cell)->updateBoundingRect();
    itemForCell(cell)->update();
}

void WorldScene::cellLotAdded(WorldCell *cell, int index)
{
    itemForCell(cell)->lotAdded(index);
    itemForCell(cell)->update();
}

void WorldScene::cellLotAboutToBeRemoved(WorldCell *cell, int index)
{
    itemForCell(cell)->lotRemoved(index);
    itemForCell(cell)->update();
}

void WorldScene::cellLotMoved(WorldCellLot *lot)
{
    itemForCell(lot->cell())->lotMoved(lot);
}

void WorldScene::cellContentsChanged(WorldCell *cell)
{
    itemForCell(cell)->cellContentsChanged();
}

void WorldScene::setShowGrid(bool show)
{
    mGridItem->setVisible(show);
}

void WorldScene::setShowCoordinates(bool show)
{
    mCoordItem->setVisible(show);
}

void WorldScene::selectedRoadsChanged()
{
    const QList<Road*> &selection = worldDocument()->selectedRoads();

    QSet<RoadItem*> items;
    foreach (Road *road, selection)
        items.insert(itemForRoad(road));

    foreach (RoadItem *item, mSelectedRoadItems - items) {
        item->setSelected(false);
        item->setEditable(false);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    }

    bool editable = EditRoadTool::instance()->isCurrent();
    foreach (RoadItem *item, items - mSelectedRoadItems) {
        item->setSelected(true);
        item->setEditable(editable);
        item->setZValue(ZVALUE_ROADITEM_SELECTED);
    }

    mSelectedRoadItems = items;
}

void WorldScene::roadAdded(int index)
{
    Road *road = world()->roads().at(index);
    Q_ASSERT(itemForRoad(road) == 0);
    RoadItem *item = new RoadItem(this, road);
    item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    addItem(item);
    mRoadItems += item;
}

void WorldScene::roadAboutToBeRemoved(int index)
{
    Road *road = world()->roads().at(index);
    RoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    mRoadItems.removeAll(item);
    mSelectedRoadItems.remove(item); // paranoia
    removeItem(item);
    delete item;
}

void WorldScene::roadCoordsChanged(int index)
{
    Road *road = world()->roads().at(index);
    RoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();
    item->update();
}

void WorldScene::roadWidthChanged(int index)
{
    Road *road = world()->roads().at(index);
    RoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();
    item->update();
}

void WorldScene::keyPressEvent(QKeyEvent *event)
{
    QGraphicsScene::keyPressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->keyPressEvent(event);
}

void WorldScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool) {
        mActiveTool->setEventView(mEventView);
        mActiveTool->mousePressEvent(event);
    }
}

void WorldScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool) {
        mActiveTool->setEventView(mEventView);
        mActiveTool->mouseMoveEvent(event);
    }
}

void WorldScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool) {
        mActiveTool->setEventView(mEventView);
        mActiveTool->mouseReleaseEvent(event);
    }
}

void WorldScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (mActiveTool != WorldCellTool::instance())
        return;
    if (WorldCell *cell = pointToCell(event->scenePos())) {
        mWorldDoc->editCell(cell->x(), cell->y());
    }
}

void WorldScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (!world()) {
        event->ignore();
        return;
    }

    foreach (const QUrl &url, event->mimeData()->urls()) {
        QFileInfo info(url.toLocalFile());
        if (!info.exists()) continue;
        if (!info.isFile()) continue;
        if (info.suffix() != QLatin1String("tmx")) continue;

        MapInfo *mapInfo = MapManager::instance()->mapInfo(info.canonicalFilePath());
        if (!mapInfo)
            continue;

        if (mapInfo->size() != QSize(300, 300))
            continue;

        mDragMapImageItem = new DragMapImageItem(mapInfo, this);
        mDragMapImageItem->setZValue(1000);
        mDragMapImageItem->setScenePos(event->scenePos());
        addItem(mDragMapImageItem);
        event->accept();
        return;
    }

    event->ignore();
}

void WorldScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDragMapImageItem) {
        mDragMapImageItem->setScenePos(event->scenePos());
    }
}

void WorldScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (mDragMapImageItem) {
        delete mDragMapImageItem;
        mDragMapImageItem = 0;
    }
}

void WorldScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDragMapImageItem) {
        if (WorldCell *cell = world()->cellAt(mDragMapImageItem->dropPos()))
            mWorldDoc->undoStack()->push(new SetCellMainMap(mWorldDoc, cell, mDragMapImageItem->mapFilePath()));
        delete mDragMapImageItem;
        mDragMapImageItem = 0;
        event->accept();
    }
}

///// ///// ///// ///// /////

static QSize mapSize(int mapWidth, int mapHeight, int tileWidth, int tileHeight)
{
    // Map width and height contribute equally in both directions
    const int side = mapHeight + mapWidth;
    return QSize(side * tileWidth / 2,
                 side * tileHeight / 2);
}

BaseCellItem::BaseCellItem(WorldScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(scene)
    , mMapImage(0)
{
    setAcceptedMouseButtons(0);
}

void BaseCellItem::initialize()
{
    updateCellImage();

    for (int i = 0; i < lots().size(); i++)
        updateLotImage(i);

    updateBoundingRect();
}

QRectF BaseCellItem::boundingRect() const
{
    return mBoundingRect;
}

void BaseCellItem::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *)
{
    Q_UNUSED(option)

    if (mMapImage) {
        QRectF target = mMapImageBounds.translated(mDrawOffset);
        QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
        painter->drawImage(target, mMapImage->image(), source);
    }

    foreach (const LotImage &lotImage, mLotImages) {
        if (!lotImage.mMapImage) continue;
        QRectF target = lotImage.mBounds.translated(mDrawOffset);
        QRectF source = QRect(QPoint(0, 0), lotImage.mMapImage->image().size());
        painter->drawImage(target, lotImage.mMapImage->image(), source);
    }

#if _DEBUG
    painter->drawRect(mBoundingRect);
#endif
}

void BaseCellItem::updateCellImage()
{
    mMapImage = 0;
    mMapImageBounds = QRect();
    if (!mapFilePath().isEmpty()) {
        mMapImage = MapImageManager::instance()->getMapImage(mapFilePath());
        if (mMapImage) {
            QSizeF gridSize = mapSize(300, 300, 64, 32);
            QSizeF unscaledMapSize = mMapImage->bounds().size();
            const qreal scaleMapToCell = unscaledMapSize.width() / gridSize.width();
            int scaledImageWidth = GRID_WIDTH * scaleMapToCell;

            const qreal scaleImageToCell = qreal(scaledImageWidth) / mMapImage->image().width();
            QPointF offset = mMapImage->tileToImageCoords(0, 0) * scaleImageToCell;
            int scaledImageHeight = mMapImage->image().height() * scaleImageToCell;
            mMapImageBounds = QRectF(mScene->cellToPixelCoords(cellPos()) - offset,
                                    QSizeF(scaledImageWidth, scaledImageHeight));
        }
    }
}

void BaseCellItem::updateLotImage(int index)
{
    WorldCellLot *lot = lots().at(index);
    MapImage *mapImage = MapImageManager::instance()->getMapImage(lot->mapName()/*, mapFilePath()*/);
    if (mapImage) {
        QSizeF gridSize = mapSize(300, 300, 64, 32);
        QSizeF unscaledMapSize = mapImage->bounds().size();
        const qreal scaleMapToCell = unscaledMapSize.width() / gridSize.width();
        int scaledImageWidth = GRID_WIDTH * scaleMapToCell;
        const qreal scaleImageToCell = qreal(scaledImageWidth) / mapImage->image().width();
        int scaledImageHeight = mapImage->image().height() * scaleImageToCell;
        QSizeF scaledImageSize(scaledImageWidth, scaledImageHeight);

        QRectF bounds = QRectF(calcLotImagePosition(lot, scaledImageWidth, mapImage),
                               scaledImageSize);
        mLotImages.insert(index, LotImage(bounds, mapImage));

        // Update lot with current width and height of the map
        lot->setWidth(mapImage->mapInfo()->width());
        lot->setHeight(mapImage->mapInfo()->height());
    } else {
        mLotImages.insert(index, LotImage());
    }
}

QPointF BaseCellItem::calcLotImagePosition(WorldCellLot *lot, int scaledImageWidth, MapImage *mapImage)
{
    if (!mapImage)
        return QPointF();

    // Assume LevelIsometric
    QPoint lotPos = lot->pos();
    lotPos += QPoint(-3, -3) * lot->level();

    const qreal cellX = cellPos().x() + (lotPos.x() / 300.0);
    const qreal cellY = cellPos().y() + (lotPos.y() / 300.0);
    QPointF pos = mScene->cellToPixelCoords(cellX, cellY);

    const qreal scaleImageToCell = qreal(scaledImageWidth) / mapImage->image().width();
    pos -= mapImage->tileToImageCoords(0, 0) * scaleImageToCell;
    return pos;
}

void BaseCellItem::updateBoundingRect()
{
    QRectF bounds = mScene->boundingRect(cellPos());

    if (!mMapImageBounds.isEmpty())
        bounds |= mMapImageBounds;

    foreach (LotImage lotImage, mLotImages) {
        if (!lotImage.mBounds.isEmpty())
            bounds |= lotImage.mBounds;
    }

    bounds.adjust(mDrawOffset.x(), mDrawOffset.y(), mDrawOffset.x(), mDrawOffset.y());

    if (mBoundingRect != bounds) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

/////

WorldCellItem::WorldCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent)
    : BaseCellItem(scene, parent)
    , mCell(cell)
{
    setFlag(ItemIsSelectable);
    initialize();
}

void WorldCellItem::lotAdded(int index)
{
    updateLotImage(index);
    updateBoundingRect();
}

void WorldCellItem::lotRemoved(int index)
{
    mLotImages.remove(index);
    updateBoundingRect();
}

void WorldCellItem::lotMoved(WorldCellLot *lot)
{
    int index = mCell->lots().indexOf(lot);
    LotImage *lotImage = &mLotImages[index];
    lotImage->mBounds.moveTopLeft(calcLotImagePosition(lot, lotImage->mBounds.width(),
                                                       lotImage->mMapImage));
    updateBoundingRect();
    update();
}

void WorldCellItem::cellContentsChanged()
{
    updateCellImage();
    mLotImages.clear();
    for (int i = 0; i < mCell->lots().size(); i++)
        updateLotImage(i);
    updateBoundingRect();
}

/////

DragCellItem::DragCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent)
    : WorldCellItem(cell, scene, parent)
{
}

void DragCellItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    WorldCellItem::paint(painter, option, widget);

    QPen pen(Qt::blue);
//    pen.setWidth(2);
    painter->setPen(pen);
    painter->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
    QPolygonF poly = mScene->cellRectToPolygon(mCell).translated(mDrawOffset);
    painter->drawPolygon(poly);
}

void DragCellItem::setDragOffset(const QPointF &offset)
{
    mDrawOffset = offset;
    updateBoundingRect();
}

/////

PasteCellItem::PasteCellItem(WorldCellContents *contents, WorldScene *scene, QGraphicsItem *parent)
    : BaseCellItem(scene, parent)
    , mContents(contents)
{
    initialize();
}

void PasteCellItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    BaseCellItem::paint(painter, option, widget);

    QPen pen(Qt::blue);
//    pen.setWidth(2);
    painter->setPen(pen);
    painter->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
    QPolygonF poly = mScene->cellRectToPolygon(QRectF(cellPos(), QSize(1, 1))).translated(mDrawOffset);
    painter->drawPolygon(poly);
}

void PasteCellItem::setDragOffset(const QPointF &offset)
{
    mDrawOffset = offset;
    updateBoundingRect();
}

/////

DragMapImageItem::DragMapImageItem(MapInfo *mapInfo, WorldScene *scene, QGraphicsItem *parent)
    : BaseCellItem(scene, parent)
    , mMapInfo(mapInfo)
{
    initialize();
}

void DragMapImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    BaseCellItem::paint(painter, option, widget);

    QPen pen(Qt::blue);
    painter->setPen(pen);
    painter->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
    QPolygonF poly = mScene->cellRectToPolygon(QRectF(cellPos(), QSize(1, 1))).translated(mDrawOffset);
    painter->drawPolygon(poly);
}

QPoint DragMapImageItem::cellPos() const
{
    return QPoint(0,0);
}

QString DragMapImageItem::mapFilePath() const
{
    return mMapInfo->path();
}

const QList<WorldCellLot *> &DragMapImageItem::lots() const
{
    return mLots;
}

void DragMapImageItem::setScenePos(const QPointF &scenePos)
{
    // scenePos is at the center of the item
    mDropPos = mScene->pixelToCellCoordsInt(scenePos);
    mDrawOffset = mScene->cellToPixelCoords(mDropPos) - mScene->cellToPixelCoords(cellPos() + QPoint(0.5, 0.5));
    updateBoundingRect();
}

QPoint DragMapImageItem::dropPos() const
{
    return mDropPos;
}

///// ///// ///// ///// /////

WorldGridItem::WorldGridItem(WorldScene *scene)
    : QGraphicsItem()
    , mScene(scene)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
}

QRectF WorldGridItem::boundingRect() const
{
    return mBoundingRect;
}

void WorldGridItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *)
{
    if (!mScene->world())
        return;

    const int tileWidth = GRID_WIDTH;
    const int tileHeight = GRID_HEIGHT;

    QRectF r = option->exposedRect;
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), mScene->pixelToCellCoords(r.topLeft()).x());
    const int startY = qMax(qreal(0), mScene->pixelToCellCoords(r.topRight()).y());
    const int endX = qMin(qreal(mScene->world()->width()),
                          mScene->pixelToCellCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(mScene->world()->height()),
                          mScene->pixelToCellCoords(r.bottomLeft()).y());

    QColor gridColor(Qt::black);
//    gridColor.setAlpha(128);

    QPen gridPen(gridColor);
    painter->setPen(gridPen);

    for (int y = startY; y <= endY; ++y) {
        const QPointF start = mScene->cellToPixelCoords(startX, (qreal)y);
        const QPointF end = mScene->cellToPixelCoords(endX, (qreal)y);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = mScene->cellToPixelCoords(x, (qreal)startY);
        const QPointF end = mScene->cellToPixelCoords(x, (qreal)endY);
        painter->drawLine(start, end);
    }
}

void WorldGridItem::updateBoundingRect()
{
    prepareGeometryChange();
    QRect bounds(0, 0, mScene->world()->width(), mScene->world()->height());
    mBoundingRect = mScene->boundingRect(bounds);
}

/////

WorldCoordItem::WorldCoordItem(WorldScene *scene)
    : QGraphicsItem()
    , mScene(scene)
{
    setFlag(ItemUsesExtendedStyleOption);
//    setFlag(ItemIgnoresTransformations);
}

QRectF WorldCoordItem::boundingRect() const
{
    return mBoundingRect;
}

void WorldCoordItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget *)
{
    if (!mScene->world())
        return;

    if (mScene->worldDocument()->view()->zoomable()->scale() < 0.25) return;

    // This only works if there is a single view on the scene
    qreal scale = 1 / mScene->worldDocument()->view()->zoomable()->scale();

    const int tileWidth = GRID_WIDTH;
    const int tileHeight = GRID_HEIGHT;

    QRectF r = option->exposedRect;
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), mScene->pixelToCellCoords(r.topLeft()).x());
    const int startY = qMax(qreal(0), mScene->pixelToCellCoords(r.topRight()).y());
    const int endX = qMin(qreal(mScene->world()->width() - 1),
                          mScene->pixelToCellCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(mScene->world()->height() - 1),
                          mScene->pixelToCellCoords(r.bottomLeft()).y());

//    painter->setPen(Qt::black);

    QFont font = QFont(painter->font(), painter->device());
    font.setPointSizeF(font.pointSizeF() * scale);
    painter->setFont(font);
    const QFontMetrics fm = painter->fontMetrics();
    int lineHeight = fm.lineSpacing();

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            QString text = QString(QLatin1String("%1,%2")).arg(x).arg(y);

            int textWidth = fm.width(text);
            QPointF center = mScene->cellToPixelCoords(x + 0.5, y + 0.5);
            QRectF r(center.x() - textWidth/2.0, center.y() - lineHeight/2.0, textWidth, lineHeight);
            r.adjust(-5 * scale, -5 * scale, 4 * scale, 4 * scale);
            painter->setBrush(Qt::lightGray);
            painter->drawRect(r);

            painter->drawText(mScene->boundingRect(x, y), Qt::AlignHCenter | Qt::AlignVCenter, text);
        }
    }
}

void WorldCoordItem::updateBoundingRect()
{
    prepareGeometryChange();
    QRect bounds(0, 0, mScene->world()->width(), mScene->world()->height());
    mBoundingRect = mScene->boundingRect(bounds);
}

/////

WorldSelectionItem::WorldSelectionItem(WorldScene *scene)
    : QGraphicsItem()
    , mScene(scene)
    , mHighlightedCellDuringDnD(-1, -1)
{
    setFlag(ItemUsesExtendedStyleOption);
    connect(mScene->worldDocument(), SIGNAL(selectedCellsChanged()), SLOT(selectedCellsChanged()));
    updateBoundingRect();
}

QRectF WorldSelectionItem::boundingRect() const
{
    return mBoundingRect;
}

void WorldSelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    Q_UNUSED(option)
    QPen pen(Qt::blue);
//    pen.setWidth(4);
    painter->setPen(pen);
    painter->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
    foreach (WorldCell *cell, mScene->worldDocument()->selectedCells()) {
        QPolygonF poly = mScene->cellRectToPolygon(cell);
        painter->drawPolygon(poly);
    }

    if (mHighlightedCellDuringDnD.x() >= 0) {
        QPolygonF poly = mScene->cellRectToPolygon(QRect(mHighlightedCellDuringDnD, QSize(1,1)));
        painter->drawPolygon(poly);
    }
}

void WorldSelectionItem::updateBoundingRect()
{
    QRectF bounds;
    foreach (WorldCell *cell, mScene->worldDocument()->selectedCells())
        bounds |= mScene->boundingRect(cell->pos());
    if (mHighlightedCellDuringDnD.x() >= 0)
        bounds |= mScene->boundingRect(mHighlightedCellDuringDnD);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void WorldSelectionItem::highlightCellDuringDnD(const QPoint &pos)
{
    mHighlightedCellDuringDnD = pos;
    updateBoundingRect();
    update();
}

void WorldSelectionItem::selectedCellsChanged()
{
    updateBoundingRect();
    update();
}

/////

RoadItem::RoadItem(WorldScene *scene, Road *road)
    : QGraphicsItem()
    , mScene(scene)
    , mRoad(road)
    , mSelected(false)
    , mEditable(false)
    , mDragging(false)
{
    synchWithRoad();
}

QRectF RoadItem::boundingRect() const
{
    return mBoundingRect;
}

QPainterPath RoadItem::shape() const
{
    QPainterPath path;
    path.addPolygon(polygon());
    return path;
}

void RoadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QColor c = Qt::blue;
    if (mSelected)
        c = Qt::green;
    if (mEditable)
        c = Qt::yellow;
    painter->fillPath(shape(), c);
}

void RoadItem::synchWithRoad()
{
    QRectF bounds = polygon().boundingRect();
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void RoadItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void RoadItem::setEditable(bool editable)
{
    mEditable = editable;
    update();
}

void RoadItem::setDragging(bool dragging)
{
    mDragging = dragging;
    synchWithRoad();
}

void RoadItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    synchWithRoad();
}

QPolygonF RoadItem::polygon() const
{
    QPolygonF polygon;

    QPoint offset = mDragging ? mDragOffset : QPoint();
    QPoint start = mRoad->start() + offset;
    QPoint end = mRoad->end() + offset;
    int w = mRoad->width();

    if (mRoad->start().x() == mRoad->end().x()) {
        polygon += mScene->roadToSceneCoords(start + QPoint(-w/2, 0));
        polygon += mScene->roadToSceneCoords(start + QPoint(w-w/2, 0));
        polygon += mScene->roadToSceneCoords(end + QPoint(w-w/2, 0));
        polygon += mScene->roadToSceneCoords(end + QPoint(-w/2, 0));
    } else {
        polygon += mScene->roadToSceneCoords(start + QPoint(0, -w/2));
        polygon += mScene->roadToSceneCoords(start + QPoint(0, w-w/2));
        polygon += mScene->roadToSceneCoords(end + QPoint(0, w-w/2));
        polygon += mScene->roadToSceneCoords(end + QPoint(0, -w/2));
    }

    return polygon;
}
