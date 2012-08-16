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

#include "scenetools.h"

#include "basegraphicsview.h"
#include "celldocument.h"
#include "cellscene.h"
#include "clipboard.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "progress.h"
#include "toolmanager.h"
#include "undoredo.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "maprenderer.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QUndoStack>

using namespace Tiled;

/////

AbstractTool::AbstractTool(const QString &name, const QIcon &icon, const QKeySequence &shortcut, ToolType type, QObject *parent)
    : QObject(parent)
    , mName(name)
    , mIcon(icon)
    , mShortcut(shortcut)
    , mEnabled(false)
    , mType(type)
    , mScene(0)
{
}

void AbstractTool::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
        return;

    mEnabled = enabled;
    emit enabledChanged(mEnabled);
}

bool AbstractTool::isCurrent() const
{
    return ToolManager::instance()->selectedTool() == this;
}

BaseWorldSceneTool *AbstractTool::asWorldTool()
{
    return isWorldTool() ? static_cast<BaseWorldSceneTool*>(this) : 0;
}

BaseCellSceneTool *AbstractTool::asCellTool()
{
    return isCellTool() ? static_cast<BaseCellSceneTool*>(this) : 0;
}

/////

BaseCellSceneTool::BaseCellSceneTool(const QString &name, const QIcon &icon, const QKeySequence &shortcut, QObject *parent)
    : AbstractTool(name, icon, shortcut, CellToolType, parent)
    , mScene(0)
    , mEventView(0)
{

}

BaseCellSceneTool::~BaseCellSceneTool()
{
}

void BaseCellSceneTool::setScene(BaseGraphicsScene *scene)
{
    mScene = scene ? scene->asCellScene() : 0;
}

void BaseCellSceneTool::updateEnabledState()
{
    setEnabled(mScene != 0);
}

void BaseCellSceneTool::setEventView(BaseGraphicsView *view)
{
    mEventView = view;
}

void BaseCellSceneTool::activate()
{
}

void BaseCellSceneTool::deactivate()
{
}

/////

CreateObjectTool *CreateObjectTool::mInstance = 0;

CreateObjectTool *CreateObjectTool::instance()
{
    if (!mInstance)
        mInstance = new CreateObjectTool();
    return mInstance;
}

void CreateObjectTool::deleteInstance()
{
    delete mInstance;
}

CreateObjectTool::CreateObjectTool()
    : BaseCellSceneTool(QLatin1String("Create Object"),
                        QIcon(QLatin1String(":/images/24x24/insert-object.png")),
                        QKeySequence())
    , mItem(0)
{
}

void CreateObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
#if 1
        mAnchorPos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(), mScene->document()->currentLevel());
#else
        mAnchorPos = mScene->renderer()->pixelToTileCoords(event->scenePos(), mScene->document()->currentLevel());

        bool snapToGrid = Preferences::instance()->snapToGrid();
        if (event->modifiers() & Qt::ControlModifier)
            snapToGrid = !snapToGrid;
        if (snapToGrid)
            mAnchorPos = mAnchorPos.toPoint();
#endif

        startNewMapObject(mAnchorPos);
        event->accept();
    }

    if (event->button() == Qt::RightButton) {
        if (mItem) {
            cancelNewMapObject();
            event->accept();
        }
    }
}

void CreateObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mItem) {
#if 1
        QPointF pos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(), mScene->document()->currentLevel());
#else
        QPointF pos = mScene->renderer()->pixelToTileCoords(event->scenePos(), mScene->document()->currentLevel());

        bool snapToGrid = Preferences::instance()->snapToGrid();
        if (event->modifiers() & Qt::ControlModifier)
            snapToGrid = !snapToGrid;
        if (snapToGrid)
            pos = pos.toPoint();
#endif

        QRectF bounds(mAnchorPos, pos);
        bounds = bounds.normalized();
        mItem->object()->setPos(bounds.topLeft());
        mItem->object()->setWidth(qMax(MIN_OBJECT_SIZE, bounds.width() + 1));
        mItem->object()->setHeight(qMax(MIN_OBJECT_SIZE, bounds.height() + 1));
        mItem->synchWithObject();
        event->accept();
    }
}

void CreateObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && mItem) {
        if ((mItem->object()->width() < MIN_OBJECT_SIZE) || (mItem->object()->height() < MIN_OBJECT_SIZE))
            cancelNewMapObject();
        else
            finishNewMapObject();
        event->accept();
    }
}

void CreateObjectTool::startNewMapObject(const QPointF &pos)
{
    WorldObjectGroup *og = mScene->document()->currentObjectGroup();
    WorldCellObject *obj = new WorldCellObject(mScene->cell(),
                                               QString(), og->type(), og,
                                               pos.x(), pos.y(),
                                               mScene->document()->currentLevel(),
                                               MIN_OBJECT_SIZE, MIN_OBJECT_SIZE);
    mItem = new ObjectItem(obj, mScene);
    mItem->setZValue(10000);
    mScene->addItem(mItem);
}

WorldCellObject *CreateObjectTool::clearNewMapObjectItem()
{
    WorldCellObject *obj = mItem->object();
    mScene->removeItem(mItem);
    delete mItem;
    mItem = 0;
    return obj;
}

void CreateObjectTool::cancelNewMapObject()
{
    WorldCellObject *obj = clearNewMapObjectItem();
    delete obj;
}

void CreateObjectTool::finishNewMapObject()
{
    WorldCellObject *obj = clearNewMapObjectItem();
    mScene->worldDocument()->addCellObject(mScene->cell(),
                                           mScene->cell()->objects().size(),
                                           obj);
#if 0 // only select objects when the ObjectTool is current
    mScene->document()->setSelectedObjects(QList<WorldCellObject*>() << obj);
#endif
}

/////

ObjectTool *ObjectTool::mInstance = 0;

ObjectTool *ObjectTool::instance()
{
    if (!mInstance)
        mInstance = new ObjectTool();
    return mInstance;
}

void ObjectTool::deleteInstance()
{
    delete mInstance;
}

ObjectTool::ObjectTool()
    : BaseCellSceneTool(QLatin1String("Select and Move Objects"),
                        QIcon(QLatin1String(":/images/22x22/tool-select-objects.png")),
                        QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mClickedItem(0)
{
}

void ObjectTool::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (mMode == Moving) {
            foreach (ObjectItem *item, mMovingItems)
                item->setDragOffset(QPointF());
            mMovingItems.clear();
            mMode = CancelMoving;
            event->accept();
        }
    }
}

void ObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{

    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mClickedItem = topmostItemAt(mStartScenePos);
        event->accept();
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            foreach (ObjectItem *item, mMovingItems)
                item->setDragOffset(QPointF());
            mMovingItems.clear();
            mMode = CancelMoving;
        }
        break;
    default:
        break;
    }
}

void ObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
//        mSelectionRectangle->setRectangle(QRectF(mStart, pos).normalized());
        break;
    case Moving:
        updateMovingItems(event->scenePos(), event->modifiers());
        break;
    case NoMode:
        break;
    case CancelMoving:
        break;
    }

    if (mMode != NoMode)
        event->accept();
}

void ObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedItem) {
            QSet<ObjectItem*> selection = mScene->selectedObjectItems();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedItem))
                    selection.remove(mClickedItem);
                else
                    selection.insert(mClickedItem);
            } else {
                selection.clear();
                selection.insert(mClickedItem);
            }
            mScene->setSelectedObjectItems(selection);
        } else {
            mScene->setSelectedObjectItems(QSet<ObjectItem*>());
        }
        break;
    case Selecting:
//        updateSelection(event->scenePos(), event->modifiers());
//        mapScene()->removeItem(mSelectionRectangle);
        mMode = NoMode;
        break;
    case Moving:
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMousePressed = false;
    mClickedItem = 0;
}

void ObjectTool::startSelecting()
{
    mMode = Selecting;
//    mScene->addItem(mSelectionRectangle);
}

void ObjectTool::startMoving()
{
    mMovingItems = mScene->selectedObjectItems();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingItems.contains(mClickedItem)) {
        mMovingItems.clear();
        mMovingItems.insert(mClickedItem);
        mScene->setSelectedObjectItems(mMovingItems);
    }

    mMode = Moving;
}

void ObjectTool::updateMovingItems(const QPointF &pos,
                                   Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    MapRenderer *renderer = mScene->renderer();

    // When dragging multiple items, snap the top-left corner of the
    // collective bounding rectangle so the items stay in the same
    // position relative to each other.
    int level = mScene->document()->currentLevel();
    WorldCellObject *obj = mClickedItem->object();
    QRectF allBounds = obj->bounds().translated((level - obj->level()) * QPoint(3,3)); // Assumes LevelIsometric renderer
    foreach (ObjectItem *item, mMovingItems) {
        obj = item->object();
        allBounds |= obj->bounds().translated((level - obj->level()) * QPoint(3,3)); // Assumes LevelIsometric renderer
    }

    QPointF startTilePos = allBounds.topLeft();
    QPointF startScenePos = renderer->tileToPixelCoords(startTilePos, level);
    QPointF newTilePos = renderer->pixelToTileCoords(startScenePos + (pos - mStartScenePos), level);

#if 1
    newTilePos = newTilePos.toPoint();
#else
    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (modifiers & Qt::ControlModifier)
        snapToGrid = !snapToGrid;
    if (snapToGrid)
        newTilePos = newTilePos.toPoint();
#endif

    foreach (ObjectItem *item, mMovingItems)
        item->setDragOffset(newTilePos - startTilePos);
}

void ObjectTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    if (!mClickedItem->dragOffset().isNull()) {
        QUndoStack *undoStack = mScene->document()->undoStack();
        undoStack->beginMacro(tr("Move %n Object(s)", "", mMovingItems.size()));
        foreach (ObjectItem *item, mMovingItems) {
            QPointF tilePos = item->tileBounds().topLeft();
            item->setDragOffset(QPointF());
            mScene->worldDocument()->moveCellObject(item->object(), tilePos);
        }
        undoStack->endMacro();
    }

    mMovingItems.clear();
}

ObjectItem *ObjectTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (ObjectItem *objectItem = dynamic_cast<ObjectItem*>(item))
            return objectItem;
    }
    return 0;
}

/////

SubMapTool *SubMapTool::mInstance = 0;

SubMapTool *SubMapTool::instance()
{
    if (!mInstance)
        mInstance = new SubMapTool();
    return mInstance;
}

void SubMapTool::deleteInstance()
{
    delete mInstance;
}

SubMapTool::SubMapTool()
    : BaseCellSceneTool(QLatin1String("Select and Move Lots"),
                        QIcon(QLatin1String(":/images/22x22/stock-tool-rect-select.png")),
                        QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mClickedItem(0)
{
}

void SubMapTool::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (mMode == Moving) {
            cancelMoving();
            mMode = CancelMoving;
            event->accept();
        }
    }
}

void SubMapTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mClickedItem = topmostItemAt(mStartScenePos);
        event->accept();
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            cancelMoving();
            mMode = CancelMoving;
            event->accept();
        }
        break;
    default:
        break;
    }
}

void SubMapTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
//        mSelectionRectangle->setRectangle(QRectF(mStart, pos).normalized());
        break;
    case Moving:
        updateMovingItems(event->scenePos(), event->modifiers());
        break;
    case NoMode:
        break;
    case CancelMoving:
        break;
    }

    if (mMode != NoMode)
        event->accept();
}

void SubMapTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedItem) {
            QSet<SubMapItem*> selection = mScene->selectedSubMapItems();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedItem))
                    selection.remove(mClickedItem);
                else
                    selection.insert(mClickedItem);
            } else {
                selection.clear();
                selection.insert(mClickedItem);
            }
            mScene->setSelectedSubMapItems(selection);
        } else {
            mScene->setSelectedSubMapItems(QSet<SubMapItem*>());
        }
        break;
    case Selecting:
//        updateSelection(event->scenePos(), event->modifiers());
//        mapScene()->removeItem(mSelectionRectangle);
        mMode = NoMode;
        break;
    case Moving:
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMousePressed = false;
    mClickedItem = 0;
}

void SubMapTool::startSelecting()
{
    mMode = Selecting;
//    mScene->addItem(mSelectionRectangle);
}

void SubMapTool::startMoving()
{
    mMovingItems = mScene->selectedSubMapItems();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingItems.contains(mClickedItem)) {
        mMovingItems.clear();
        mMovingItems.insert(mClickedItem);
        mScene->setSelectedSubMapItems(mMovingItems);
    }

    mMode = Moving;

    foreach (SubMapItem *item, mMovingItems) {
        item->subMap()->setHiddenDuringDrag(true);
        QString path = item->subMap()->mapInfo()->path();
        DnDItem *dndItem = new DnDItem(path, mScene->renderer(), item->subMap()->levelOffset());
        dndItem->setHotSpot(0, 0);
        mDnDItems.append(dndItem);
        dndItem->setZValue(10000);
        mScene->addItem(dndItem);
    }
}

void SubMapTool::updateMovingItems(const QPointF &pos,
                                   Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    MapRenderer *renderer = mScene->renderer();

    int i = 0;
    foreach (SubMapItem *item, mMovingItems) {
        int level = mScene->document()->currentLevel();
        QPoint startTilePos = renderer->pixelToTileCoordsInt(mStartScenePos, level);
        QPoint newTilePos = renderer->pixelToTileCoordsInt(pos, level);
        const QPoint diff = newTilePos - startTilePos;
        if (mDnDItems[i]) {
            mDnDItems[i]->setTilePosition(item->lot()->pos() + diff);
        }
        ++i;
    }
}

void SubMapTool::finishMoving(const QPointF &pos)
{
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (SubMapItem *item, mMovingItems)
        item->subMap()->setHiddenDuringDrag(false);

    int level = mScene->document()->currentLevel();
    QPoint startTilePos = mScene->renderer()->pixelToTileCoordsInt(mStartScenePos, level);
    QPoint endTilePos = mScene->renderer()->pixelToTileCoordsInt(pos, level);
    if (startTilePos != endTilePos) {
        QUndoStack *undoStack = mScene->document()->undoStack();
        undoStack->beginMacro(tr("Move %n Lots(s)", "", mMovingItems.size()));
        int i = 0;
        foreach (SubMapItem *item, mMovingItems) {
            DnDItem *dndItem = mDnDItems[i];
            mScene->worldDocument()->moveCellLot(item->lot(), dndItem->dropPosition());
            ++i;
        }
        undoStack->endMacro();
    }

    foreach (DnDItem *item, mDnDItems)
        mScene->removeItem(item);
    qDeleteAll(mDnDItems);
    mDnDItems.clear();
    mMovingItems.clear();
}

void SubMapTool::cancelMoving()
{
    foreach (SubMapItem *item, mMovingItems) {
        item->subMap()->setHiddenDuringDrag(false);
        item->update();
    }

    foreach (DnDItem *item, mDnDItems)
        mScene->removeItem(item);
    qDeleteAll(mDnDItems);
    mDnDItems.clear();
    mMovingItems.clear();
}

SubMapItem *SubMapTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (SubMapItem *subMapItem = dynamic_cast<SubMapItem*>(item))
            return subMapItem;
    }
    return 0;
}

/////

#include "worldscene.h"

BaseWorldSceneTool::BaseWorldSceneTool(const QString &name, const QIcon &icon, const QKeySequence &shortcut, QObject *parent)
    : AbstractTool(name, icon, shortcut, WorldToolType, parent)
    , mScene(0)
    , mEventView(0)
{

}

BaseWorldSceneTool::~BaseWorldSceneTool()
{
}

void BaseWorldSceneTool::setScene(BaseGraphicsScene *scene)
{
    mScene = scene ? scene->asWorldScene() : 0;
}

void BaseWorldSceneTool::activate()
{
}

void BaseWorldSceneTool::deactivate()
{
}

void BaseWorldSceneTool::updateEnabledState()
{
    setEnabled(mScene != 0);
}

void BaseWorldSceneTool::setEventView(BaseGraphicsView *view)
{
    mEventView = view;
}

QPointF BaseWorldSceneTool::restrictDragging(const QVector<QPoint> &cellPositions,
                                             const QPointF &startScenePos,
                                             const QPointF &currentScenePos)
{
    // Snap-to-grid
    QPointF pt = mScene->pixelToCellCoords(startScenePos);
    qreal x1 = pt.x() - (int)pt.x();
    qreal y1 = pt.y() - (int)pt.y();

    pt = mScene->pixelToCellCoords(currentScenePos);

    // Pick the correct quadrant of the mouse-over cell depending on
    // the quadrant the hot-spot is in
    qreal x2 = pt.x() - int(pt.x());
    qreal y2 = pt.y() - int(pt.y());
    if (x2 - x1 > 0.5)
        pt.setX(pt.x() + 1);
    else if (x2 - x1 < -0.5)
        pt.setX(pt.x() - 1);
    if (y2 - y1 > 0.5)
        pt.setY(pt.y() + 1);
    else if (y2 - y1 < -0.5)
        pt.setY(pt.y() - 1);

    Q_ASSERT(mEventView);
    qreal frac = 0.25, halfFrac = frac/2;
    qreal scale = qMin(qMax(frac, mEventView->zoomable()->scale()),1.0);
    QPolygonF poly = mScene->cellRectToPolygon(QRectF(int(pt.x())+x1-halfFrac/scale,
                                                      int(pt.y())+y1-halfFrac/scale,
                                                      frac/scale, frac/scale));
    if (poly.containsPoint(currentScenePos, Qt::OddEvenFill))
        pt = mScene->cellToPixelCoords(int(pt.x())+x1, int(pt.y())+y1);
    else
        pt = currentScenePos;

    // Restrict the drop position so the cells stay in bounds
    QPoint startCellPos = mScene->pixelToCellCoordsInt(startScenePos);
    QPoint dropCellPos = mScene->pixelToCellCoordsInt(currentScenePos);
    QPoint deltaCellPos = dropCellPos - startCellPos;

    QRect cellBounds = QRect(cellPositions.first(), QSize(1, 1)).translated(deltaCellPos);
    foreach (QPoint cellPos, cellPositions)
        cellBounds |= QRect(cellPos, QSize(1, 1)).translated(deltaCellPos);

    QRect worldBounds = mScene->world()->bounds();
    if (!worldBounds.contains(cellBounds, true)) {
        if (cellBounds.left() < worldBounds.left())
            dropCellPos += QPoint(worldBounds.left() - cellBounds.left(), 0);
        if (cellBounds.top() < worldBounds.top())
            dropCellPos += QPoint(0, worldBounds.top() - cellBounds.top());
        if (cellBounds.right() > worldBounds.right())
            dropCellPos += QPoint(worldBounds.right() - cellBounds.right(), 0);
        if (cellBounds.bottom() > worldBounds.bottom())
            dropCellPos += QPoint(0, worldBounds.bottom() - cellBounds.bottom());

//        QPointF pt = mScene->pixelToCellCoords(startScenePos);
//        qreal x1 = pt.x() - (int)pt.x();
//        qreal y1 = pt.y() - (int)pt.y();
        return mScene->cellToPixelCoords(dropCellPos + QPointF(x1, y1));
    }
    return pt;
}

/////

WorldCellTool *WorldCellTool::mInstance = 0;

WorldCellTool *WorldCellTool::instance()
{
    if (!mInstance)
        mInstance = new WorldCellTool();
    return mInstance;
}

void WorldCellTool::deleteInstance()
{
    delete mInstance;
}

WorldCellTool::WorldCellTool()
    : BaseWorldSceneTool(tr("Select and Move Cells"),
                         QIcon(QLatin1String(":/images/22x22/stock-tool-rect-select.png")),
                         QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mSelectionRectItem(new QGraphicsPolygonItem)
{
    mSelectionRectItem->setZValue(1000);
    mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
    mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
}

WorldCellTool::~WorldCellTool()
{
    delete mSelectionRectItem;
}

void WorldCellTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;

        qDeleteAll(mDnDItems);
        mDnDItems.clear();

        foreach (WorldCell *cell, mMovingCells)
            mScene->itemForCell(cell)->setVisible(true);
        mMovingCells.clear();
        event->accept();
    }
}

void WorldCellTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mDropTilePos = mScene->pixelToCellCoordsInt(mStartScenePos);
        mClickedItem = topmostItemAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            qDeleteAll(mDnDItems);
            mDnDItems.clear();
            foreach (WorldCell *cell, mMovingCells)
                mScene->itemForCell(cell)->setVisible(true);
            mMovingCells.clear();
        }
        break;
    default:
        break;
    }
}

void WorldCellTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem &&
                    mScene->worldDocument()->selectedCells().contains(mClickedItem->cell()) &&
                    !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
    {
        QPointF start = mScene->pixelToCellCoords(mStartScenePos);
        QPointF end = mScene->pixelToCellCoords(event->scenePos());
        QRectF bounds = QRectF(start, end).normalized();
        mSelectionRectItem->setPolygon(mScene->cellRectToPolygon(bounds));
        break;
    }
    case Moving:
        updateMovingItems(event->scenePos(), event->modifiers());
        break;
    case NoMode:
        break;
    case CancelMoving:
        break;
    }
}

void WorldCellTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
    {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        QList<WorldCell*> newSelection;
        if (extend || toggle)
            newSelection = mScene->worldDocument()->selectedCells();
        if (mClickedItem) {
            if (toggle && newSelection.contains(mClickedItem->cell()))
                newSelection.removeOne(mClickedItem->cell());
            else if (!newSelection.contains(mClickedItem->cell()))
                newSelection += mClickedItem->cell();
        }
        mScene->worldDocument()->setSelectedCells(newSelection);
        break;
    }
    case Selecting:
        updateSelection(event);
        mScene->removeItem(mSelectionRectItem);
        mMode = NoMode;
        break;
    case Moving:
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMousePressed = false;
    mClickedItem = 0;
}

void WorldCellTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void WorldCellTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mScene->pixelToCellCoords(mStartScenePos);
    QPointF end = mScene->pixelToCellCoords(event->scenePos());
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    QList<WorldCell*> selection;
    if (extend || toggle)
        selection = mScene->worldDocument()->selectedCells();
    for (int y = bounds.top(); y <= bounds.bottom(); y++) {
        for (int x = bounds.left(); x <= bounds.right(); x++) {
            if (WorldCell *cell = mScene->world()->cellAt(x, y)) {
                if (toggle && selection.contains(cell))
                    selection.removeOne(cell);
                else if (!selection.contains(cell))
                    selection += cell;
            }
        }
    }
    mScene->worldDocument()->setSelectedCells(selection);
}

void WorldCellTool::startMoving()
{
    mMovingCells = mScene->worldDocument()->selectedCells();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingCells.contains(mClickedItem->cell())) {
        mMovingCells.clear();
        mMovingCells += mClickedItem->cell();
        mScene->worldDocument()->setSelectedCells(mMovingCells);
    }

    mMode = Moving;

    foreach (WorldCell *cell, mMovingCells) {
        mScene->itemForCell(cell)->setVisible(false);
        DragCellItem *dndItem = new DragCellItem(cell, mScene);
        mDnDItems.append(dndItem);
        dndItem->setZValue(1000);
        mScene->addItem(dndItem);
    }
}

void WorldCellTool::updateMovingItems(const QPointF &pos,
                                      Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
#if 0
    // Snap-to-grid
    QPointF pt = mScene->pixelToCellCoords(mStartScenePos);
    qreal x1 = pt.x() - (int)pt.x();
    qreal y1 = pt.y() - (int)pt.y();

    pt = mScene->pixelToCellCoords(pos);

    // Pick the correct quadrant of the mouse-over cell depending on
    // the quadrant the hot-spot is in
    qreal x2 = pt.x() - int(pt.x());
    qreal y2 = pt.y() - int(pt.y());
    if (x2 - x1 > 0.5)
        pt.setX(pt.x() + 1);
    else if (x2 - x1 < -0.5)
        pt.setX(pt.x() - 1);
    if (y2 - y1 > 0.5)
        pt.setY(pt.y() + 1);
    else if (y2 - y1 < -0.5)
        pt.setY(pt.y() - 1);

    Q_ASSERT(mEventView);
    qreal frac = 0.25, halfFrac = frac/2;
    qreal scale = qMin(qMax(frac, mEventView->zoomable()->scale()),1.0);
    QPolygonF poly = mScene->cellRectToPolygon(QRectF(int(pt.x())+x1-halfFrac/scale,
                                                      int(pt.y())+y1-halfFrac/scale,
                                                      frac/scale, frac/scale));
    if (poly.containsPoint(pos, Qt::OddEvenFill))
        pt = mScene->cellToPixelCoords(int(pt.x())+x1, int(pt.y())+y1);
    else
        pt = pos;
#endif
    // Restrict the drop position so the cells stay in bounds
    QVector<QPoint> cellPositions;(mDnDItems.size());
    cellPositions.clear();
    foreach (DragCellItem *item, mDnDItems)
        cellPositions.append(item->cellPos());
    QPointF pt = restrictDragging(cellPositions, mStartScenePos, pos);

    foreach (DragCellItem *item, mDnDItems)
        item->setDragOffset(pt - mStartScenePos);

    mDropTilePos = mScene->pixelToCellCoordsInt(pt);
}

void WorldCellTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    QPoint startCellPos = mScene->pixelToCellCoordsInt(mStartScenePos);
    QPoint dropCellPos = mDropTilePos;
    if (startCellPos != dropCellPos) {
        QUndoStack *undoStack = mScene->worldDocument()->undoStack();
        int count = mMovingCells.size();
        undoStack->beginMacro(tr("Move %1 Cell%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        undoStack->push(new ProgressBegin(tr("Moving Cells"))); // in case of multiple loadMap() calls
        mOrderedMovingCells.clear();
        foreach (WorldCell *cell, mMovingCells)
            pushCellToMove(cell, dropCellPos - startCellPos);
        QList<WorldCell*> newSelection;
        foreach (WorldCell *cell, mOrderedMovingCells) {
            QPoint newPos = cell->pos() + dropCellPos - startCellPos;
            mScene->worldDocument()->moveCell(cell, newPos);
            newSelection += mScene->world()->cellAt(newPos);
        }
        mScene->worldDocument()->setSelectedCells(newSelection, true);
        undoStack->push(new ProgressEnd(tr("Undoing Move Cells"))); // in case of multiple loadMap() calls
        undoStack->endMacro();
    }

    qDeleteAll(mDnDItems);
    mDnDItems.clear();

    foreach (WorldCell *cell, mMovingCells)
        mScene->itemForCell(cell)->setVisible(true);
    mMovingCells.clear();
}

void WorldCellTool::pushCellToMove(WorldCell *cell, const QPoint &offset)
{
    if (mOrderedMovingCells.contains(cell))
        return;
    QPoint newPos = cell->pos() + offset;
    foreach (WorldCell *test, mMovingCells) {
        if (test == cell)
            continue;
        if (test->pos() == newPos)
            pushCellToMove(test, offset);
    }
    mOrderedMovingCells += cell;
}

WorldCellItem *WorldCellTool::topmostItemAt(const QPointF &scenePos)
{
    WorldCell *cell = mScene->pointToCell(scenePos);
    return cell ? mScene->itemForCell(cell) : 0;
}

/////

PasteCellsTool *PasteCellsTool::mInstance = 0;

PasteCellsTool *PasteCellsTool::instance()
{
    if (!mInstance)
        mInstance = new PasteCellsTool();
    return mInstance;
}

void PasteCellsTool::deleteInstance()
{
    delete mInstance;
}

PasteCellsTool::PasteCellsTool()
    : BaseWorldSceneTool(QLatin1String("Paste Cells"),
                         QIcon(QLatin1String(":/images/24x24/edit-paste.png")),
                         QKeySequence())
{
}

PasteCellsTool::~PasteCellsTool()
{
}

void PasteCellsTool::activate()
{
    BaseWorldSceneTool::activate();
    startMoving();
}

void PasteCellsTool::deactivate()
{
    BaseWorldSceneTool::deactivate();

    // The user might switch tools while this tool is active
    cancelMoving();
}

void PasteCellsTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene) {
        mScene->disconnect(this);
    }

    mScene = scene ? scene->asWorldScene() : 0;

    if (mScene) {
        connect(Clipboard::instance(), SIGNAL(clipboardChanged()),
                SLOT(updateEnabledState()));
    }
}

void PasteCellsTool::updateEnabledState()
{
    setEnabled(mScene && Clipboard::instance()->cellsInClipboardCount());
}

void PasteCellsTool::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        ToolManager::instance()->selectTool(WorldCellTool::instance());
        if (!isCurrent()) {
            cancelMoving();
        }
        event->accept();
    }
}

void PasteCellsTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        pasteCells(event->scenePos());
        event->accept();
        return;
    }
    // Right-clicks exits pasting mode, same as the Escape key.
    if (event->button() == Qt::RightButton) {
        ToolManager::instance()->selectTool(WorldCellTool::instance());
        if (!isCurrent()) {
            cancelMoving();
        }
        event->accept();
    }
}

void PasteCellsTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateMovingItems(event->scenePos(), event->modifiers());
    event->accept();
}

void PasteCellsTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        event->accept();
}

void PasteCellsTool::startMoving()
{
    QRectF tileBounds;

    foreach (WorldCellContents *contents, Clipboard::instance()->cellsInClipboard()) {
        PasteCellItem *dndItem = new PasteCellItem(contents, mScene);
        mDnDItems.append(dndItem);
        dndItem->setZValue(1000);
        mScene->addItem(dndItem);

        tileBounds |= QRectF(contents->pos(), QSize(1, 1));
    }

    mStartScenePos = mScene->cellToPixelCoords(tileBounds.center());
}

void PasteCellsTool::updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
#if 0
    // Snap-to-grid
    QPointF pt = mScene->pixelToCellCoords(mStartScenePos);
    qreal x1 = pt.x() - (int)pt.x();
    qreal y1 = pt.y() - (int)pt.y();

    pt = mScene->pixelToCellCoords(pos);

    // Pick the correct quadrant of the mouse-over cell depending on
    // the quadrant the hot-spot is in
    qreal x2 = pt.x() - int(pt.x());
    qreal y2 = pt.y() - int(pt.y());
    if (x2 - x1 > 0.5)
        pt.setX(pt.x() + 1);
    else if (x2 - x1 < -0.5)
        pt.setX(pt.x() - 1);
    if (y2 - y1 > 0.5)
        pt.setY(pt.y() + 1);
    else if (y2 - y1 < -0.5)
        pt.setY(pt.y() - 1);

    Q_ASSERT(mEventView);
    qreal frac = 0.25, halfFrac = frac/2;
    qreal scale = qMin(qMax(frac, mEventView->zoomable()->scale()),1.0);
    QPolygonF poly = mScene->cellRectToPolygon(QRectF(int(pt.x())+x1-halfFrac/scale, int(pt.y())+y1-halfFrac/scale, frac/scale, frac/scale));
    if (poly.containsPoint(pos, Qt::OddEvenFill))
        pt = mScene->cellToPixelCoords(int(pt.x())+x1, int(pt.y())+y1);
    else
        pt = pos;
#endif
    QVector<QPoint> cellPositions;(mDnDItems.size());
    cellPositions.clear();
    foreach (PasteCellItem *item, mDnDItems)
        cellPositions.append(item->cellPos());
    QPointF pt = restrictDragging(cellPositions, mStartScenePos, pos);

    foreach (PasteCellItem *item, mDnDItems)
        item->setDragOffset(pt - mStartScenePos);

    mDropTilePos = mScene->pixelToCellCoordsInt(pt);
}

void PasteCellsTool::pasteCells(const QPointF &pos)
{
    Q_UNUSED(pos)
    QPoint startCellPos = mScene->pixelToCellCoordsInt(mStartScenePos);
    QPoint dropCellPos = mDropTilePos;

    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    int count = mDnDItems.size();
    undoStack->beginMacro(tr("Paste %1 Cell%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
    undoStack->push(new ProgressBegin(tr("Pasting Cells"))); // in case of multiple loadMap() calls
#if 1
    // This can be called multiple times.
    Clipboard::instance()->pasteEverythingButCells(mScene->worldDocument());
#endif
    QList<WorldCell*> newSelection;
    foreach (PasteCellItem *item, mDnDItems) {
        QPoint newPos = item->cellPos() + dropCellPos - startCellPos;
        WorldCell *replace = mScene->world()->cellAt(newPos);
        WorldCellContents *contents = new WorldCellContents(item->contents(), replace);
#if 1
        // PropertyDefs, Templates, ObjectTypes, ObjectGroups -> from clipboard-world to document-world
        contents->swapWorld(mScene->world());
        contents->mergeOnto(replace);
#endif
        undoStack->push(new ReplaceCell(mScene->worldDocument(), replace, contents));
        newSelection += mScene->world()->cellAt(newPos);
    }
    mScene->worldDocument()->setSelectedCells(newSelection, true);
    undoStack->push(new ProgressEnd(tr("Undoing Paste Cells"))); // in case of multiple loadMap() calls
    undoStack->endMacro();
}

void PasteCellsTool::cancelMoving()
{
    qDeleteAll(mDnDItems);
    mDnDItems.clear();
}
