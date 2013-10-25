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
#include "bmptotmx.h"
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

#include <qmath.h>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QUrl>

const int WorldScene::ZVALUE_CELLITEM = 1;
const int WorldScene::ZVALUE_ROADITEM_UNSELECTED = 2;
const int WorldScene::ZVALUE_ROADITEM_SELECTED = 3;
const int WorldScene::ZVALUE_ROADITEM_CREATING = 4;
const int WorldScene::ZVALUE_GRIDITEM = 5;
const int WorldScene::ZVALUE_SELECTIONITEM = 6;
const int WorldScene::ZVALUE_COORDITEM = 7;
const int WorldScene::ZVALUE_DNDITEM = 100;

WorldScene::WorldScene(WorldDocument *worldDoc, QObject *parent)
    : BaseGraphicsScene(WorldSceneType, parent)
    , mWorldDoc(worldDoc)
    , mGridItem(new WorldGridItem(this))
    , mCoordItem(new WorldCoordItem(this))
    , mSelectionItem(new WorldSelectionItem(this))
    , mPasteCellsTool(0)
    , mActiveTool(0)
    , mDragMapImageItem(0)
    , mDragBMPItem(0)
    , mBMPToolActive(false)

{
    setBackgroundBrush(Qt::darkGray);

    mGridItem->setZValue(ZVALUE_GRIDITEM);
    addItem(mGridItem);

    mSelectionItem->setZValue(ZVALUE_SELECTIONITEM);
    addItem(mSelectionItem);

    mCoordItem->setZValue(ZVALUE_COORDITEM);
    addItem(mCoordItem);

    connect(mWorldDoc, SIGNAL(worldAboutToResize(QSize)),
            SLOT(worldAboutToResize(QSize)));
    connect(mWorldDoc, SIGNAL(worldResized(QSize)),
            SLOT(worldResized(QSize)));

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

    connect(mWorldDoc, SIGNAL(generateLotSettingsChanged()),
            SLOT(generateLotsSettingsChanged()));

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

    connect(mWorldDoc, SIGNAL(selectedBMPsChanged()),
            SLOT(selectedBMPsChanged()));
    connect(mWorldDoc, SIGNAL(bmpAdded(int)),
            SLOT(bmpAdded(int)));
    connect(mWorldDoc, SIGNAL(bmpCoordsChanged(int)),
            SLOT(bmpCoordsChanged(int)));
    connect(mWorldDoc, SIGNAL(bmpAboutToBeRemoved(int)),
            SLOT(bmpAboutToBeRemoved(int)));

    mGridItem->updateBoundingRect();
    setSceneRect(mGridItem->boundingRect());
    mCoordItem->updateBoundingRect();

    mCellItems.resize(world()->width() * world()->height());
    for (int y = 0; y < world()->height(); y++) {
        for (int x = 0; x < world()->width(); x++) {
            WorldCell *cell = world()->cellAt(x, y);
            WorldCellItem *item = new WorldCellItem(cell, this);
            addItem(item);
            item->setZValue(ZVALUE_CELLITEM); // below mGridItem
            mCellItems[y * world()->width() + x] = item;
            mPendingThumbnails += item;
        }
    }

    foreach (Road *road, world()->roads()) {
        WorldRoadItem *item = new WorldRoadItem(this, road);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
        addItem(item);
        mRoadItems += item;
    }

    Preferences *prefs = Preferences::instance();
    mGridItem->setVisible(prefs->showWorldGrid());
    mCoordItem->setVisible(prefs->showCoordinates());
    connect(prefs, SIGNAL(showWorldGridChanged(bool)), SLOT(setShowGrid(bool)));
    connect(prefs, SIGNAL(showCoordinatesChanged(bool)), SLOT(setShowCoordinates(bool)));
    connect(prefs, SIGNAL(showBMPsChanged(bool)),
            SLOT(setShowBMPs(bool)));
    connect(prefs, SIGNAL(worldThumbnailsChanged(bool)),
            SLOT(worldThumbnailsChanged(bool)));

    mPasteCellsTool = PasteCellsTool::instance();

    foreach (WorldBMP *bmp, world()->bmps()) {
        WorldBMPItem *item = new WorldBMPItem(this, bmp);
        addItem(item);
        mBMPItems += item;
    }

    connect(MapManager::instance(), SIGNAL(mapFileCreated(QString)),
            SLOT(mapFileCreated(QString)));

    connect(MapImageManager::instance(), SIGNAL(mapImageChanged(MapImage*)),
            SLOT(mapImageChanged(MapImage*)));

    handlePendingThumbnails();
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

    if (mActiveTool != WorldEditRoadTool::instance())
        foreach (WorldRoadItem *item, mRoadItems)
            item->setEditable(false);

    bool bmpToolActive = mActiveTool == WorldBMPTool::instance();
    if (bmpToolActive != mBMPToolActive) {
        if (bmpToolActive) {
            worldDocument()->setSelectedCells(QList<WorldCell*>());
            foreach (WorldCellItem *item, mCellItems)
                item->setVisible(false); //item->setOpacity(0.2);
            setShowBMPs(true);
        } else {
            worldDocument()->setSelectedBMPs(QList<WorldBMP*>());
            foreach (WorldCellItem *item, mCellItems)
                item->setVisible(true); //item->setOpacity(1.0);
            setShowBMPs(Preferences::instance()->showBMPs());
        }
        mBMPToolActive = bmpToolActive;
    }
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
    QRect adjusted = roadRect.adjusted(0, 0, 1, 1);
    polygon += roadToSceneCoords(adjusted.topLeft());
    polygon += roadToSceneCoords(adjusted.topRight());
    polygon += roadToSceneCoords(adjusted.bottomRight());
    polygon += roadToSceneCoords(adjusted.bottomLeft());
    return polygon;
}

WorldRoadItem *WorldScene::itemForRoad(Road *road)
{
    foreach (WorldRoadItem *item, mRoadItems)
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
        if (WorldRoadItem *roadItem = dynamic_cast<WorldRoadItem*>(item))
            result += roadItem->road();
    }
    return result;
}

QList<WorldBMP *> WorldScene::bmpsInRect(const QRectF &cellRect)
{
    QPolygonF polygon = cellRectToPolygon(cellRect);

    QList<WorldBMP*> result;
    foreach (QGraphicsItem *item, items(polygon)) {
        if (WorldBMPItem *bmpItem = dynamic_cast<WorldBMPItem*>(item))
            result += bmpItem->bmp();
    }
    return result;
}

void WorldScene::pasteCellsFromClipboard()
{
    ToolManager::instance()->selectTool(mPasteCellsTool);
    //    mActiveTool = mPasteCellsTool;
}

void WorldScene::worldAboutToResize(const QSize &newSize)
{
    // Delete items for cells that are getting chopped off.
    QRect newBounds = QRect(QPoint(0, 0), newSize);
    for (int x = 0; x < world()->width(); x++)
        for (int y = 0; y < world()->height(); y++) {
            if (!newBounds.contains(x, y)) {
                delete itemForCell(x, y);
                mCellItems[x + y * world()->width()] = 0;
            }
        }
}

void WorldScene::worldResized(const QSize &oldSize)
{
    QVector<WorldCellItem*> items(world()->width() * world()->height());

    // Reuse items still in bounds.
    for (int x = 0; x < qMin(oldSize.width(), world()->width()); x++) {
        for (int y = 0; y < qMin(oldSize.height(), world()->height()); y++) {
            WorldCellItem *item = mCellItems[x + y * oldSize.width()];
            item->worldResized();
            items[x + y * world()->width()] = item;
        }
    }

    // Create new items for new cells.
    QRect oldBounds = QRect(QPoint(0, 0), oldSize);
    for (int x = 0; x < world()->width(); x++) {
        for (int y = 0; y < world()->height(); y++) {
            if (!oldBounds.contains(x, y)) {
                WorldCellItem *item = new WorldCellItem(world()->cellAt(x, y), this);
                item->setZValue(ZVALUE_CELLITEM);
                items[x + y * world()->width()] = item;
                addItem(item);
            }
        }
    }

    mCellItems = items;
    mGridItem->updateBoundingRect();
    setSceneRect(mGridItem->boundingRect());
    mCoordItem->updateBoundingRect();
    mSelectionItem->updateBoundingRect();
    foreach (WorldBMPItem *item, mBMPItems)
        item->synchWithBMP();
    foreach (WorldRoadItem *item, mRoadItems)
        item->synchWithRoad();
}

void WorldScene::generateLotsSettingsChanged()
{
    mCoordItem->update();
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

QPoint WorldScene::pixelToCellCoordsInt(const QPointF &point) const
{
    QPointF pos = pixelToCellCoords(point.x(), point.y());
    return QPoint(qFloor(pos.x()), qFloor(pos.y()));
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

void WorldScene::setShowBMPs(bool show)
{
    foreach (WorldBMPItem *bmpItem, mBMPItems)
        bmpItem->setVisible(show);
}

void WorldScene::selectedRoadsChanged()
{
    const QList<Road*> &selection = worldDocument()->selectedRoads();

    QSet<WorldRoadItem*> items;
    foreach (Road *road, selection)
        items.insert(itemForRoad(road));

    foreach (WorldRoadItem *item, mSelectedRoadItems - items) {
        item->setSelected(false);
        item->setEditable(false);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    }

    bool editable = WorldEditRoadTool::instance()->isCurrent();
    foreach (WorldRoadItem *item, items - mSelectedRoadItems) {
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
    WorldRoadItem *item = new WorldRoadItem(this, road);
    item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    addItem(item);
    mRoadItems += item;
}

void WorldScene::roadAboutToBeRemoved(int index)
{
    Road *road = world()->roads().at(index);
    WorldRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    mRoadItems.removeAll(item);
    mSelectedRoadItems.remove(item); // paranoia
    removeItem(item);
    delete item;
}

void WorldScene::roadCoordsChanged(int index)
{
    Road *road = world()->roads().at(index);
    WorldRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();
    item->update();
}

void WorldScene::roadWidthChanged(int index)
{
    Road *road = world()->roads().at(index);
    WorldRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();
    item->update();
}

void WorldScene::selectedBMPsChanged()
{
    const QList<WorldBMP*> &selection = worldDocument()->selectedBMPs();

    QSet<WorldBMPItem*> items;
    foreach (WorldBMP *bmp, selection)
        items.insert(itemForBMP(bmp));

    foreach (WorldBMPItem *item, mSelectedBMPItems - items) {
        item->setSelected(false);
    }

    foreach (WorldBMPItem *item, items - mSelectedBMPItems) {
        item->setSelected(true);
    }

    mSelectedBMPItems = items;
}

void WorldScene::bmpAdded(int index)
{
    WorldBMP *bmp = world()->bmps().at(index);
    Q_ASSERT(!itemForBMP(bmp));
    WorldBMPItem *item = new WorldBMPItem(this, bmp);
    addItem(item);
    mBMPItems += item;
}

void WorldScene::bmpAboutToBeRemoved(int index)
{
    WorldBMP *bmp = world()->bmps().at(index);
    WorldBMPItem *item = itemForBMP(bmp);
    Q_ASSERT(item);
    mBMPItems.removeAll(item);
    mSelectedBMPItems.remove(item); // paranoia
    removeItem(item);
    delete item;
}

void WorldScene::bmpCoordsChanged(int index)
{
    WorldBMP *bmp = world()->bmps().at(index);
    WorldBMPItem *item = itemForBMP(bmp);
    Q_ASSERT(item);
    item->synchWithBMP();
}

WorldBMP *WorldScene::pointToBMP(const QPointF &scenePos)
{
    QPoint cellPos = pixelToCellCoordsInt(scenePos);
    foreach (WorldBMP *bmp, world()->bmps()) {
        if (bmp->bounds().contains(cellPos))
            return bmp;
    }
    return 0;
}

WorldBMPItem *WorldScene::itemForBMP(WorldBMP *bmp)
{
    foreach (WorldBMPItem *item, mBMPItems) {
        if (item->bmp() == bmp)
            return item;
    }
    return 0;
}

void WorldScene::mapFileCreated(const QString &path)
{
    foreach (WorldCellItem *item, mCellItems)
        item->mapFileCreated(path);
}

void WorldScene::mapImageChanged(MapImage *mapImage)
{
    foreach (WorldCellItem *item, mCellItems)
        item->mapImageChanged(mapImage);
    handlePendingThumbnails();
}

void WorldScene::worldThumbnailsChanged(bool thumbs)
{
    mPendingThumbnails.clear();
    if (thumbs) {
        foreach (WorldCellItem *item, mCellItems)
            mPendingThumbnails += item;
        handlePendingThumbnails();
    } else {
        foreach (WorldCellItem *item, mCellItems)
            item->thumbnailsAreFail();
    }
}

void WorldScene::handlePendingThumbnails()
{
    if (!Preferences::instance()->worldThumbnails())
        return;

    if (mPendingThumbnails.size()) {
        WorldCellItem *item = mPendingThumbnails.first();
        int loaded = item->thumbnailsAreGo();
        if (loaded != 2) {
            mPendingThumbnails.takeFirst();
            if (mPendingThumbnails.size()) {
                QMetaObject::invokeMethod(this, "handlePendingThumbnails",
                                          Qt::QueuedConnection);
            }
        }
    }
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
        mDoubleClick = false;
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

        if (mDoubleClick) {
            if (WorldCell *cell = pointToCell(event->scenePos())) {
                mWorldDoc->editCell(cell->x(), cell->y());
            }
        }
    }
}

void WorldScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *)
{
    if (mActiveTool != WorldCellTool::instance())
        return;

    mDoubleClick = true;
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

        if (BMPToTMX::supportedImageFormats().contains(info.suffix())) {
            QSize size = BMPToTMX::instance()->validateImages(info.canonicalFilePath());
            if (size.isEmpty())
                continue;
            QPoint center = pixelToCellCoordsInt(event->scenePos());
            int width = size.width() / 300;
            int height = size.height() / 300;
            int x = center.x() - width / 2;
            int y = center.y() - height / 2;
            WorldBMP *bmp = new WorldBMP(world(), x, y, width, height, info.canonicalFilePath());
            mDragBMPItem = new WorldBMPItem(this, bmp);
            mDragBMPItem->setZValue(ZVALUE_DNDITEM);
            addItem(mDragBMPItem);
            event->accept();
            return;
        }

        if (info.suffix() != QLatin1String("tmx")) continue;

        MapInfo *mapInfo = MapManager::instance()->mapInfo(info.canonicalFilePath());
        if (!mapInfo)
            continue;

        if (mapInfo->size() != QSize(300, 300))
            continue;

        mDragMapImageItem = new DragMapImageItem(mapInfo, this);
        mDragMapImageItem->setZValue(ZVALUE_DNDITEM);
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

    if (mDragBMPItem) {
        QPoint center = pixelToCellCoordsInt(event->scenePos());
        mDragBMPItem->bmp()->setPos(center.x() - mDragBMPItem->bmp()->width() / 2,
                                    center.y() - mDragBMPItem->bmp()->height() / 2);
        mDragBMPItem->synchWithBMP();
    }
}

void WorldScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (mDragMapImageItem) {
        delete mDragMapImageItem;
        mDragMapImageItem = 0;
    }

    if (mDragBMPItem) {
        delete mDragBMPItem->bmp();
        delete mDragBMPItem;
        mDragBMPItem = 0;
    }
}

void WorldScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDragMapImageItem) {
        if (WorldCell *cell = world()->cellAt(mDragMapImageItem->dropPos()))
            mWorldDoc->setCellMapName(cell, mDragMapImageItem->mapFilePath());
        delete mDragMapImageItem;
        mDragMapImageItem = 0;
        event->accept();
    }

    if (mDragBMPItem) {
        mWorldDoc->insertBMP(mWorldDoc->world()->bmps().count(), mDragBMPItem->bmp());
        delete mDragBMPItem;
        mDragBMPItem = 0;
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
    , mWantsImages(true)
{
    setAcceptedMouseButtons(0);
#ifndef QT_NO_DEBUG
    mUpdatingImage = false;
#endif
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

QPainterPath BaseCellItem::shape() const
{
    QPainterPath path;
    path.addPolygon(mScene->cellRectToPolygon(QRect(cellPos(), QSize(1, 1))).translated(mDrawOffset));
    return path;
}

void BaseCellItem::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *)
{
    Q_UNUSED(option)

    if (mMapImage && mMapImage->isLoaded()) {
        QRectF target = mMapImageBounds.translated(mDrawOffset);
        QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
        painter->drawImage(target, mMapImage->image(), source);
    }

    foreach (const LotImage &lotImage, mLotImages) {
        if (!lotImage.mMapImage || !lotImage.mMapImage->isLoaded()) continue;
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
    if (mWantsImages && !mapFilePath().isEmpty()) {
#ifndef QT_NO_DEBUG
        Q_ASSERT(!mUpdatingImage);
        mUpdatingImage = true;
#endif
        mMapImage = MapImageManager::instance()->getMapImage(mapFilePath());
#ifndef QT_NO_DEBUG
        mUpdatingImage = false;
#endif
        if (mMapImage) {
            calcMapImageBounds();
        }
    }

    setToolTip(QDir::toNativeSeparators(mapFilePath()));
}

void BaseCellItem::updateLotImage(int index)
{
    WorldCellLot *lot = lots().at(index);
    MapImage *mapImage = mWantsImages
            ? MapImageManager::instance()->getMapImage(lot->mapName()/*, mapFilePath()*/)
            : 0;
    if (mapImage) {
        mLotImages.insert(index, LotImage(QRectF(), mapImage));
        calcLotImageBounds(index);
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

void BaseCellItem::mapImageChanged(MapImage *mapImage)
{
    bool changed = false;
    if (mapImage == mMapImage) {
        calcMapImageBounds();
        changed = true;
    }
#if 0
    // Perhaps the image now exists and didn't before.
    if (!mMapImage) {
        updateCellImage();
        if (mMapImage)
            changed = true;
    }
#endif
    int index = 0;
    foreach (LotImage lotImage, mLotImages) {
        if (mapImage == lotImage.mMapImage) {
            calcLotImageBounds(index);
            changed = true;
        }
#if 0
        // Perhaps the image now exists and didn't before.
        if (!lotImage.mMapImage) {
            updateLotImage(index);
            if (mLotImages[index].mMapImage)
                changed = true;
        }
#endif
        ++index;
    }

    if (changed) {
        updateBoundingRect();
        update();
    }
}

void BaseCellItem::worldResized()
{
    calcMapImageBounds();
    for (int i = 0; i < mLotImages.size(); i++)
        calcLotImageBounds(i);
    updateBoundingRect();
}

void BaseCellItem::calcMapImageBounds()
{
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

void BaseCellItem::calcLotImageBounds(int index)
{
    WorldCellLot *lot = lots().at(index);
    LotImage &lotImage = mLotImages[index];
    MapImage *mapImage = lotImage.mMapImage;
    if (!mapImage)
        return;

    QSizeF gridSize = mapSize(300, 300, 64, 32);
    QSizeF unscaledMapSize = mapImage->bounds().size();
    const qreal scaleMapToCell = unscaledMapSize.width() / gridSize.width();
    int scaledImageWidth = GRID_WIDTH * scaleMapToCell;
    const qreal scaleImageToCell = qreal(scaledImageWidth) / mapImage->image().width();
    int scaledImageHeight = mapImage->image().height() * scaleImageToCell;
    QSizeF scaledImageSize(scaledImageWidth, scaledImageHeight);

    lotImage.mBounds = QRectF(calcLotImagePosition(lot, scaledImageWidth, mapImage),
                              scaledImageSize);

    // Update lot with current width and height of the map
    lot->setWidth(mapImage->mapInfo()->width());
    lot->setHeight(mapImage->mapInfo()->height());
}

/////

WorldCellItem::WorldCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent)
    : BaseCellItem(scene, parent)
    , mCell(cell)
{
    setFlag(ItemIsSelectable);
    mWantsImages = false;
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

void WorldCellItem::mapFileCreated(const QString &path)
{
    // If BMPtoTMX creates our cell's .tmx file and that .tmx file didn't exist
    // before, we need to create the cell/lot images.
    if (mapFilePath().isEmpty() || mMapImage)
        return;
    if (QFileInfo(path) == QFileInfo(mapFilePath()))
        cellContentsChanged();
}

int WorldCellItem::thumbnailsAreGo()
{
    if (mMapImage && mMapImage->isLoaded())
        return 1;
    mWantsImages = true;
    cellContentsChanged();
    if (mMapImage && mMapImage->isLoaded())
        return 1;
    return (mMapImage != 0) ? 2 : 0;
}

void WorldCellItem::thumbnailsAreFail()
{
    if (mWantsImages) {
        mWantsImages = false;
        cellContentsChanged();
    }
}

/////

DragCellItem::DragCellItem(WorldCell *cell, WorldScene *scene, QGraphicsItem *parent)
    : WorldCellItem(cell, scene, parent)
{
    mWantsImages = Preferences::instance()->worldThumbnails();
    initialize(); // again?!?!?!?!
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

    QPoint worldOrigin = mScene->world()->getGenerateLotsSettings().worldOrigin;

    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            QString text = QString(QLatin1String("%1,%2")).arg(worldOrigin.x() + x).arg(worldOrigin.y() + y);

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

WorldRoadItem::WorldRoadItem(WorldScene *scene, Road *road)
    : QGraphicsItem()
    , mScene(scene)
    , mRoad(road)
    , mSelected(false)
    , mEditable(false)
    , mDragging(false)
{
    synchWithRoad();
}

QRectF WorldRoadItem::boundingRect() const
{
    return mBoundingRect;
}

QPainterPath WorldRoadItem::shape() const
{
    QPainterPath path;
    path.addPolygon(polygon());
    return path;
}

void WorldRoadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

void WorldRoadItem::synchWithRoad()
{
    QRectF bounds = polygon().boundingRect();
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void WorldRoadItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void WorldRoadItem::setEditable(bool editable)
{
    mEditable = editable;
    update();
}

void WorldRoadItem::setDragging(bool dragging)
{
    mDragging = dragging;
    synchWithRoad();
}

void WorldRoadItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    synchWithRoad();
}

QPolygonF WorldRoadItem::polygon() const
{
    QPoint offset = mDragging ? mDragOffset : QPoint();

    return mScene->roadRectToScenePolygon(mRoad->bounds().translated(offset));
}

/////

WorldBMPItem::WorldBMPItem(WorldScene *scene, WorldBMP *bmp)
    : QGraphicsItem()
    , mScene(scene)
    , mBMP(bmp)
    , mSelected(false)
    , mDragging(false)
{
    mMapImage = MapImageManager::instance()->getMapImage(bmp->filePath());
    if (!mMapImage) qDebug() << MapImageManager::instance()->errorString();

    // I chopped up the image to make OpenGL happy (no 6000x3000 textures), but
    // performance is way better without OpenGL, probably due to pixel format.
    if (mMapImage) {
        int columns = (mMapImage->image().width() + 511) / 512;
        int rows = (mMapImage->image().height() + 511) / 512;
        mSubImages.resize(columns * rows);
        QRect r(QPoint(), mMapImage->image().size());
        for (int x = 0; x < columns; x++) {
            for (int y = 0; y < rows; y++) {
                QRect subr = QRect(x * 512, y * 512, 512, 512) & r;
                mSubImages[x + y * columns] = mMapImage->image().copy(subr);
            }
        }
    }

    setToolTip(QDir::toNativeSeparators(bmp->filePath()));

    synchWithBMP();
}

QRectF WorldBMPItem::boundingRect() const
{
    return mMapImageBounds;
}

QPainterPath WorldBMPItem::shape() const
{
    QPainterPath path;
    path.addPolygon(this->polygon());
    return path;
}

void WorldBMPItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                         QWidget *)
{
    if (mMapImage) {
#if 1
        int columns = (mMapImage->image().width() + 511) / 512;
        int rows = (mMapImage->image().height() + 511) / 512;
        qreal scale = mMapImageBounds.width() / qreal(mMapImage->image().width());
        for (int x = 0; x < columns; x++) {
            for (int y = 0; y < rows; y++) {
                QImage &img = mSubImages[x + y * columns];
                int imgw = img.width(), imgh = img.height();
                QRectF target = QRectF(mMapImageBounds.x() + x * 512 * scale,
                                       mMapImageBounds.y() + y * 512 * scale,
                                       imgw * scale, imgh * scale);
                QRectF source = QRect(QPoint(), img.size());
                painter->drawImage(target, img, source);
            }
        }
#else
        QRectF target = mMapImageBounds;
        QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
        painter->drawImage(target, mMapImage->image(), source);
#endif
    } else {
        QPolygonF polygon = this->polygon();

        painter->save();

        QPen pen(Qt::black);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);
        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawPolygon(polygon);

        QColor color = Qt::red;
        pen.setColor(color);
        painter->setPen(pen);
        QColor brushColor = color;
        brushColor.setAlpha(50);
        QBrush brush(brushColor);
        painter->setBrush(brush);
        polygon.translate(0, -1);
        painter->drawPolygon(polygon);

        painter->restore();
    }

    if (mSelected) {
        QPolygonF polygon = this->polygon();

        painter->save();

        QPen pen(Qt::black);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);
        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->drawPolygon(polygon);

        QColor color = Qt::blue;
        pen.setColor(color);
        painter->setPen(pen);
        QColor brushColor = QColor(0x33,0x99,0xff,255/8);
        QBrush brush(brushColor);
        painter->setBrush(brush);
        polygon.translate(0, -1);
        painter->drawPolygon(polygon);

        painter->restore();
    }
}

void WorldBMPItem::synchWithBMP()
{
    QRectF bounds = mScene->boundingRect(mBMP->bounds()
            .translated(mDragging ? mDragOffset : QPoint()));
    if (bounds != mMapImageBounds) {
        prepareGeometryChange();
        mMapImageBounds = bounds;
    }
}

void WorldBMPItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void WorldBMPItem::setDragging(bool dragging)
{
    mDragging = dragging;
    synchWithBMP();
}

void WorldBMPItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    synchWithBMP();
}

QPolygonF WorldBMPItem::polygon() const
{
    return mScene->cellRectToPolygon(mBMP->bounds()
                                     .translated(mDragging ? mDragOffset : QPoint()));
}
