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
#include "bmptotmx.h"
#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "clipboard.h"
#include "mapcomposite.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "mainwindow.h"
#include "preferences.h"
#include "progress.h"
#include "spawntooldialog.h"
#include "toolmanager.h"
#include "undoredo.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "maprenderer.h"

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMenu>
#include <QUndoStack>
#include <QUrl>

using namespace Tiled;

/////

AbstractTool::AbstractTool(const QString &name, const QIcon &icon,
                           const QKeySequence &shortcut, ToolType type,
                           QObject *parent)
    : QObject(parent)
    , mName(name)
    , mIcon(icon)
    , mShortcut(shortcut)
    , mEnabled(false)
    , mType(type)
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

BaseGraphicsScene *BaseCellSceneTool::scene() const
{
    return mScene;
}

void BaseCellSceneTool::updateEnabledState()
{
    setEnabled(mScene != 0);
}

void BaseCellSceneTool::setEventView(BaseGraphicsView *view)
{
    mEventView = view;
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
                        QKeySequence(QLatin1String("O")))
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
    mItem->labelItem()->setShowSize(true);
    mItem->setZValue(10000);
    mScene->addItem(mItem);
}

WorldCellObject *CreateObjectTool::clearNewMapObjectItem()
{
    WorldCellObject *obj = mItem->object();
    mScene->removeItem(mItem);
    delete mItem;
    mItem = nullptr;
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

SelectMoveObjectTool *SelectMoveObjectTool::mInstance = nullptr;

SelectMoveObjectTool *SelectMoveObjectTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveObjectTool();
    return mInstance;
}

void SelectMoveObjectTool::deleteInstance()
{
    delete mInstance;
}

void SelectMoveObjectTool::deactivate()
{
    if (mResizeHandle) {
        delete mResizeHandle;
        mResizeHandle = nullptr;
    }
    BaseCellSceneTool::deactivate();
}

void SelectMoveObjectTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene) {
        mScene->worldDocument()->disconnect(this);
    }

    BaseCellSceneTool::setScene(scene);

    if (mScene) {
        connect(mScene->worldDocument(), &WorldDocument::cellObjectAboutToBeRemoved,
                this, &SelectMoveObjectTool::cellObjectAboutToBeRemoved);
        connect(mScene->worldDocument(), &WorldDocument::cellObjectMoved,
                this, &SelectMoveObjectTool::cellObjectMoved);
        connect(mScene->worldDocument(), &WorldDocument::cellObjectResized,
                this, &SelectMoveObjectTool::cellObjectResized);
    }
}

SelectMoveObjectTool::SelectMoveObjectTool()
    : BaseCellSceneTool(QLatin1String("Select and Move Objects"),
                        QIcon(QLatin1String(":/images/22x22/tool-select-objects.png")),
                        QKeySequence(QLatin1String("S")))
    , mMode(NoMode)
    , mMousePressed(false)
    , mClickedItem(nullptr)
    , mResizeHandle(nullptr)
{
}

void SelectMoveObjectTool::keyPressEvent(QKeyEvent *event)
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

void SelectMoveObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
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
            event->accept();
        }
        break;
    default:
        break;
    }
}

void SelectMoveObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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
    {
        ObjectItem *hoverItem = topmostItemAt(event->scenePos(), true);
        if ((hoverItem != nullptr) && (hoverItem->object()->isRectangle() == false)) {
            hoverItem = nullptr;
        }
        CellObjectEdgeResizeHandle::Edge edge = CellObjectEdgeResizeHandle::pickEdge(hoverItem, event->scenePos());
        if (edge != CellObjectEdgeResizeHandle::Edge::NONE) {
            if (mResizeHandle) {
                if (mResizeHandle->object() != hoverItem->object()) {
                    mResizeHandle->setObject(hoverItem->object());
                }
                if (mResizeHandle->edge() != edge) {
                    mResizeHandle->setEdge(edge);
                }
            } else {
                mResizeHandle = new CellObjectEdgeResizeHandle(mScene, hoverItem->object(), edge);
                mResizeHandle->setZValue(CellScene::ZVALUE_GRID + 1);
                mScene->addItem(mResizeHandle);
            }
        } else {
            if (mResizeHandle) {
                delete mResizeHandle;
                mResizeHandle = nullptr;
            }
        }
        break;
    }
    case CancelMoving:
        break;
    }

    if (mMode != NoMode)
        event->accept();
}

void SelectMoveObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (mMode == NoMode) {
            showContextMenu(event->scenePos(), event->screenPos());
            event->accept();
        }
        return;
    }

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
    mClickedItem = nullptr;
}

void SelectMoveObjectTool::cellObjectAboutToBeRemoved(WorldCell *cell, int objectIndex)
{
    if (mResizeHandle == nullptr)
        return;
    if (cell->objects().at(objectIndex) == mResizeHandle->object()) {
        delete mResizeHandle;
        mResizeHandle = nullptr;
    }
}

void SelectMoveObjectTool::cellObjectMoved(WorldCellObject *object)
{
    if (mResizeHandle == nullptr)
        return;
    if (mResizeHandle->object() != object)
        return;
    mResizeHandle->synchWithObject();
}

void SelectMoveObjectTool::cellObjectResized(WorldCellObject *object)
{
    if (mResizeHandle == nullptr)
        return;
    if (mResizeHandle->object() != object)
        return;
    mResizeHandle->synchWithObject();
}

void SelectMoveObjectTool::startSelecting()
{
    mMode = Selecting;
//    mScene->addItem(mSelectionRectangle);
}

void SelectMoveObjectTool::startMoving()
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

void SelectMoveObjectTool::updateMovingItems(const QPointF &pos,
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

void SelectMoveObjectTool::finishMoving(const QPointF &pos)
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

void SelectMoveObjectTool::showContextMenu(const QPointF &scenePos, const QPoint &screenPos)
{
    ObjectItem *item = topmostItemAt(scenePos);
    if (!item) {
        return;
    }

    QList<WorldCellObject*> objects;
    if (mScene->document()->selectedObjects().contains(item->object()))
        objects = mScene->document()->selectedObjects();
    else {
        objects << item->object();
        mScene->document()->setSelectedObjects(objects);
    }
    int count = objects.size();

    QMenu menu;
    QIcon removeIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QAction *removeAction = menu.addAction(removeIcon, (count > 1)
                                           ? tr("Remove %1 Objects").arg(count)
                                           : tr("Remove Object"));

    QAction *action = menu.exec(screenPos);
    if (action == removeAction) {
        if (count > 1)
            mScene->worldDocument()->undoStack()->beginMacro(removeAction->text());
        foreach (WorldCellObject *obj, objects)
            mScene->worldDocument()->removeCellObject(mScene->cell(), obj->index());
        if (count > 1)
            mScene->worldDocument()->undoStack()->endMacro();
    }
}

ObjectItem *SelectMoveObjectTool::topmostItemAt(const QPointF &scenePos, bool editable)
{
    // ObjectLabelItem uses ItemIgnoresTransformations to keep its size the
    // same regardless of the view's scale.
    QTransform xform = mScene->views().at(0)->viewportTransform();
    foreach (QGraphicsItem *item, mScene->items(scenePos,
                                                Qt::IntersectsItemShape,
                                                Qt::DescendingOrder, xform)) {
        if (ObjectItem *objectItem = dynamic_cast<ObjectItem*>(item)) {
            if (editable && objectItem->isEditable() == false)
                continue;
            if (!objectItem->isAdjacent())
                return objectItem;
        }

        if (ObjectLabelItem *labelItem = dynamic_cast<ObjectLabelItem*>(item)) {
            if (editable)
                continue;
            if (!labelItem->objectItem()->isAdjacent())
                return labelItem->objectItem();
        }
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
    , mMapHighlightItem(new QGraphicsPolygonItem)
    , mHighlightedMap(0)
{
    mMapHighlightItem->setPen(Qt::NoPen);
    mMapHighlightItem->setBrush(QColor(128, 128, 128, 128));
    mMapHighlightItem->setZValue(CellScene::ZVALUE_GRID - 1);
}

void SubMapTool::activate()
{
        mScene->addItem(mMapHighlightItem);
}

void SubMapTool::deactivate()
{
    mScene->removeItem(mMapHighlightItem);
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
        if (mMode == NoMode) {
            showContextMenu(event->scenePos(), event->screenPos());
            event->accept();
        }
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

static MapComposite *mapUnderPoint(CellScene *scene, MapComposite *mc, MapRenderer *renderer,
                                   const QPointF &scenePos)
{
    foreach (MapComposite *subMap, mc->subMaps()) {
        if (subMap->isAdjacentMap()) {
            MapComposite *subSubMap = mapUnderPoint(scene, subMap, renderer, scenePos);
            if (subSubMap)
                subMap = subSubMap;
        }

        bool ignore = false;
        QList<SubMapItem*> items = scene->subMapItemsUsingMapInfo(subMap->mapInfo());
        foreach (SubMapItem *item, items) {
            if (item->subMap() == subMap) {
                ignore = true;
                break;
            }
        }
        if (ignore)
            continue;

        QRect tileBounds = subMap->mapInfo()->bounds().translated(subMap->originRecursive());
        QPolygonF scenePolygon = renderer->tileToPixelCoords(tileBounds);
        if (scenePolygon.containsPoint(scenePos, Qt::WindingFill))
            return subMap;
    }
    return 0;
}

void SubMapTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    // Do mouse-over highlighting of Lots that are baked into a map.
    SubMapItem *item = topmostItemAt(event->scenePos());
    MapComposite *highlight = nullptr;
    if (!item && !mMousePressed) {
        if (MapComposite *mc = mapUnderPoint(mScene, mScene->mapComposite(), mScene->renderer(), event->scenePos())) {
            if (mc->isAdjacentMap())
                mc = nullptr;
            highlight = mc;
        }
    }
    if (highlight != mHighlightedMap) {
        if (highlight) {
            QRect tileBounds = highlight->mapInfo()->bounds().translated(highlight->originRecursive());
            QPolygonF polygon = mScene->renderer()->tileToPixelCoords(tileBounds);
            mMapHighlightItem->setPolygon(polygon);
            mMapHighlightItem->setToolTip(QDir::toNativeSeparators(highlight->mapInfo()->path()));
        }
        mMapHighlightItem->setVisible(highlight != nullptr);
        mHighlightedMap = highlight;
    }

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
    mClickedItem = nullptr;
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

void SubMapTool::showContextMenu(const QPointF &scenePos, const QPoint &screenPos)
{
    SubMapItem *item = topmostItemAt(scenePos);
    if (!item) {
        MapComposite *subMap = mapUnderPoint(mScene,
                                             mScene->mapComposite(),
                                             mScene->renderer(),
                                             scenePos);
        QPoint tilePos(mScene->renderer()->pixelToTileCoordsInt(scenePos));
        if (!subMap && mScene->mapComposite()->mapInfo()->bounds().contains(tilePos))
            subMap = mScene->mapComposite();
        if (subMap) {
            QMenu menu;
            QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
            QAction *openAction = menu.addAction(tiledIcon, tr("Open in TileZed"));
            QAction *action = menu.exec(screenPos);
            if (action == openAction) {
                QUrl url = QUrl::fromLocalFile(subMap->mapInfo()->path());
                QDesktopServices::openUrl(url);
            }
        }
        return;
    }

    QMenu menu;
    QIcon lightIcon(QLatin1String(":/images/idea.png"));
    QAction *lightbulbRoomAction = nullptr;
    QString roomName;
    if (LightSwitchOverlay *overlay = topmostSwitchAt(scenePos)) {
        roomName = overlay->mRoomName;
    } else
        roomName = mScene->roomNameAt(scenePos);
    if (roomName.length()) {
        if (LightbulbsMgr::instance().rooms().contains(roomName))
            lightbulbRoomAction = menu.addAction(lightIcon,
                                                 tr("Show lights in rooms called %1").arg(roomName));
        else
            lightbulbRoomAction = menu.addAction(lightIcon,
                                                 tr("Hide lights in rooms called %1").arg(roomName));
    }
    QAction *lightbulbMapAction = nullptr;
    QString mapName = QFileInfo(item->subMap()->mapInfo()->path()).fileName();
    if (LightbulbsMgr::instance().maps().contains(mapName))
        lightbulbMapAction = menu.addAction(lightIcon, tr("Show lights in %1").arg(mapName));
    else
        lightbulbMapAction = menu.addAction(lightIcon, tr("Hide lights in %1").arg(mapName));
    QIcon removeIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QAction *removeAction = menu.addAction(removeIcon, tr("Remove Lot"));
    menu.addSeparator();
    QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
    QAction *openAction = menu.addAction(tiledIcon, tr("Open in TileZed"));

    QAction *action = menu.exec(screenPos);
    if (action == nullptr) return;
    if (action == lightbulbRoomAction)
        LightbulbsMgr::instance().toggleRoom(roomName);
    if (action == lightbulbMapAction)
        LightbulbsMgr::instance().toggleMap(mapName);
    if (action == removeAction) {
        int lotIndex = mScene->cell()->indexOf(item->lot());
        mScene->worldDocument()->removeCellLot(mScene->cell(), lotIndex);
    }
    if (action == openAction) {
        QUrl url = QUrl::fromLocalFile(item->subMap()->mapInfo()->path());
        QDesktopServices::openUrl(url);
    }
}

SubMapItem *SubMapTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (SubMapItem *subMapItem = dynamic_cast<SubMapItem*>(item))
            return subMapItem;
    }
    return nullptr;
}

LightSwitchOverlay *SubMapTool::topmostSwitchAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (LightSwitchOverlay *switchItem = dynamic_cast<LightSwitchOverlay*>(item))
            return switchItem;
    }
    return 0;
}

/////

class RoomToneCursorItem : public QGraphicsItem
{
public:
    RoomToneCursorItem(CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void updateBounds();

    CellScene *mScene;
    MapRenderer *mRenderer;
    QRectF mBoundingRect;
    QPointF mPos;
    int mLevel;
    QImage mImage;
};

RoomToneCursorItem::RoomToneCursorItem(CellScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mRenderer(scene->renderer()),
    mImage(QImage(QStringLiteral(":/images/SpeakerIcon.png")))
{
}

QRectF RoomToneCursorItem::boundingRect() const
{
    return mBoundingRect;
}

void RoomToneCursorItem::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *,
                               QWidget *)
{
    QColor color = mScene->document()->currentObjectGroup()->color();
    color = color.lighter();
    mRenderer->drawFancyRectangle(painter, QRectF(mPos, QSizeF(1, 1)), color, mLevel);

    const QFontMetrics fm = painter->fontMetrics();
    int lineHeight = fm.lineSpacing();

    QPointF scenePos = mRenderer->tileToPixelCoords(mPos + QPointF(0.5, 0.5), mLevel);

    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    zoom = qMin(zoom, 1.0);
    QRectF sceneRect(scenePos - QPointF((mImage.width() / 2) / zoom, (lineHeight + mImage.height()) / zoom), mImage.size() / zoom);
    painter->drawImage(sceneRect, mImage);

}

void RoomToneCursorItem::updateBounds()
{
    QPointF pos = mPos;
    QRectF bounds;
    bounds |= mRenderer->boundingRect(QRect(pos.x() - 3, pos.y() - 3, 1, 1), mLevel);
    bounds |= mRenderer->boundingRect(QRect(pos.x(), pos.y(), 1, 1), mLevel);
    bounds.adjust(-2, -3, 2, 2);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

/////

SINGLETON_IMPL(RoomToneTool)

RoomToneTool::RoomToneTool()
    : BaseCellSceneTool(QLatin1String("Place Room Tone"),
                        QIcon(QLatin1String(":/images/SpeakerIcon.png")),
                        QKeySequence()),
      mContextMenuVisible(false)
{
    new SpawnToolDialog(MainWindow::instance());
}

void RoomToneTool::activate()
{
    BaseCellSceneTool::activate();

    mCursorItem = new RoomToneCursorItem(mScene);
    mCursorItem->setZValue(mScene->ZVALUE_GRID + 1);
    mScene->addItem(mCursorItem);
}

void RoomToneTool::deactivate()
{
    delete mCursorItem;

    BaseCellSceneTool::activate();
}

void RoomToneTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        showContextMenu(event->scenePos(), event->screenPos());
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (RoomToneItem *item = topmostItemAt(event->scenePos())) {
        QSet<WorldCellObject*> selection = mScene->document()->selectedObjects().toSet();
        if (event->modifiers() & Qt::ShiftModifier) {
            selection += item->object();
        } else if (event->modifiers() & Qt::ControlModifier) {
            if (selection.contains(item->object())) {
                selection -= item->object();
            } else {
                selection += item->object();
            }
        } else {
            selection.clear();
            selection += item->object();
        }
        mScene->document()->setSelectedObjects(selection.toList());
        event->accept();
        return;
    }

    // Try to ignore left-click that closes the context menu
    if (mContextMenuVisible || (mContextMenuShown.isValid() &&
            mContextMenuShown.msecsTo(QTime::currentTime()) < 500)) {
        return;
    }

    event->accept();

    mScene->document()->undoStack()->beginMacro(tr("Add RoomTone"));

    createWorldTemplateIfNeeded();
    ObjectType *type = mScene->world()->objectType(QLatin1String("RoomTone"));
    PropertyTemplate *pt = mScene->world()->propertyTemplate(QLatin1String("RoomTone"));

    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(),
                                                              mScene->document()->currentLevel());

    WorldObjectGroup *og = mScene->world()->objectGroups().find(QLatin1String("RoomTone"));
    WorldCellObject *obj = new WorldCellObject(mScene->cell(),
                                               QString(), type, og,
                                               tilePos.x(), tilePos.y(),
                                               mScene->document()->currentLevel(),
                                               1, 1);
    obj->addTemplate(obj->templates().size(), pt);
    for (Property *property : pt->properties()) {
        PropertyDef *pd = mScene->world()->propertyDefinition(property->mDefinition->mName);
        obj->addProperty(obj->properties().size(), new Property(pd, pd->mDefaultValue));
    }
    mScene->worldDocument()->addCellObject(mScene->cell(),
                                           mScene->cell()->objects().size(),
                                           obj);

    mScene->document()->setSelectedObjects(WorldCellObjectList() << obj);

    mScene->document()->undoStack()->endMacro();
}

void RoomToneTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mCursorItem->setVisible(topmostItemAt(event->scenePos()) == 0);

    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(),
                                                              mScene->document()->currentLevel());
    mCursorItem->mPos = tilePos;
    mCursorItem->mLevel = mScene->document()->currentLevel();
    mCursorItem->updateBounds();
}

void RoomToneTool::showContextMenu(const QPointF &scenePos, const QPoint &screenPos)
{
    RoomToneItem *item = topmostItemAt(scenePos);
    if (!item) {
        return;
    }

    mContextMenuVisible = true;

//    mScene->document()->setSelectedObjects(WorldCellObjectList() << item->object());

    QMenu menu;
    QIcon removeIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QAction *removeAction = menu.addAction(removeIcon, tr("Remove Room Tone"));

    QAction *action = menu.exec(screenPos);
    if (action == removeAction) {
        mScene->worldDocument()->removeCellObject(mScene->cell(), item->object()->index());
    }

    mContextMenuVisible = false;
    mContextMenuShown = QTime::currentTime();
}

RoomToneItem *RoomToneTool::topmostItemAt(const QPointF &scenePos)
{
    for (QGraphicsItem *item : mScene->items(scenePos)) {
        if (RoomToneItem *roomToneItem = dynamic_cast<RoomToneItem*>(item)) {
            if (!roomToneItem->isAdjacent()) {
                return roomToneItem;
            }
        }
    }
    return nullptr;
}

void RoomToneTool::createWorldTemplateIfNeeded()
{
    World *world = mScene->world();

    // Create the RoomTone object type if needed
    ObjectType *type = world->objectType(QLatin1String("RoomTone"));
    if (!type) {
        type = new ObjectType(QLatin1String("RoomTone"));
        mScene->worldDocument()->addObjectType(type);
    }

    // Create the RoomTone object group if needed
    WorldObjectGroup *og = world->objectGroups().find(QLatin1String("RoomTone"));
    if (og == nullptr) {
        og = new WorldObjectGroup(type, QLatin1String("RoomTone"), Qt::blue);
        world->insertObjectGroup(world->objectGroups().size(), og);
    }

    // Create the RoomTone property enum if needed
    PropertyEnum *pe = world->propertyEnums().find(QLatin1String("RoomTone"));
    if (!pe) {
        QStringList rooms;
        rooms << QLatin1String("Generic");
        mScene->worldDocument()->addPropertyEnum(QLatin1String("RoomTone"), rooms, true);
        pe = world->propertyEnums().find(QLatin1String("RoomTone"));
    }

    // Create the RoomTone property definition if needed
    PropertyDef *pd = world->propertyDefinition(QLatin1String("RoomTone"));
    if (!pd) {
        pd = new PropertyDef(QLatin1String("RoomTone"), QLatin1String("Generic"),
                             tr("Used to set the game's RoomType FMOD parameter."),
                             pe);
        mScene->worldDocument()->addPropertyDefinition(pd);
    }

    // Create the EntireBuilding property definition if needed
    PropertyDef *pd2 = world->propertyDefinition(QLatin1String("EntireBuilding"));
    if (!pd2) {
        pd2 = new PropertyDef(QLatin1String("EntireBuilding"), QLatin1String("false"),
                             tr("If true, a RoomTone is active in the entire building."),
                             nullptr);
        mScene->worldDocument()->addPropertyDefinition(pd2);
    }

    // Create the RoomTone template if needed
    PropertyTemplate *pt = world->propertyTemplate(QLatin1String("RoomTone"));
    if (!pt) {
        pt = new PropertyTemplate;
        pt->mName = QLatin1String("RoomTone");
        pt->mDescription = tr("This template holds the default set of properties for all RoomTone objects.");
        mScene->worldDocument()->addTemplate(pt);
    }

    if (pt->canAddProperty(pd)) {
        pt->addProperty(pt->properties().size(), new Property(pd, pd->mDefaultValue));
    }
    if (pt->canAddProperty(pd2)) {
        pt->addProperty(pt->properties().size(), new Property(pd2, pd2->mDefaultValue));
    }
}

/////

class SpawnPointCursorItem : public QGraphicsItem
{
public:
    SpawnPointCursorItem(CellScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    void updateBounds();

    CellScene *mScene;
    MapRenderer *mRenderer;
    QRectF mBoundingRect;
    QPointF mPos;
    int mLevel;
};

SpawnPointCursorItem::SpawnPointCursorItem(CellScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mRenderer(scene->renderer())
{
}

QRectF SpawnPointCursorItem::boundingRect() const
{
    return mBoundingRect;
}

void SpawnPointCursorItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *,
                           QWidget *)
{
    QColor color = mScene->document()->currentObjectGroup()->color();
    color = color.lighter();
    mRenderer->drawFancyRectangle(painter, QRectF(mPos, QSizeF(1, 1)), color, mLevel);

    painter->setPen(QPen(Qt::black));
    color = mScene->document()->currentObjectGroup()->color();
    color.setAlpha(200);

    int level = mLevel;

    qreal inset = 0.15;
    QPointF pos = mPos;

    // Bottom-right
    QPolygonF poly;
    poly << mRenderer->tileToPixelCoords(pos + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 0), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.darker(125));
    painter->drawPolygon(poly);
    poly.clear();

    // Bottom-left
    poly << mRenderer->tileToPixelCoords(pos + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(0, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.darker(115));
    painter->drawPolygon(poly);
    poly.clear();

    // Top-right
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 0), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(3, 3) + QPointF(0.5, 0.5), level);
    poly << poly.first();
    painter->setBrush(color.lighter(100));
    painter->drawPolygon(poly);
    poly.clear();

    // Top-left
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(3, 3) + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(0, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.lighter(115));
    painter->drawPolygon(poly);
    poly.clear();
}

void SpawnPointCursorItem::updateBounds()
{
    QPointF pos = mPos;
    QRectF bounds;
    bounds |= mRenderer->boundingRect(QRect(pos.x() - 3, pos.y() - 3, 1, 1), mLevel);
    bounds |= mRenderer->boundingRect(QRect(pos.x(), pos.y(), 1, 1), mLevel);
    bounds.adjust(-2, -3, 2, 2);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

/////

SINGLETON_IMPL(SpawnPointTool)

SpawnPointTool::SpawnPointTool()
    : BaseCellSceneTool(QLatin1String("Place Spawn Point"),
                        QIcon(QLatin1String(":/images/22x22/tool-spawn-point.png")),
                        QKeySequence()),
      mContextMenuVisible(false)
{
    new SpawnToolDialog(MainWindow::instance());
}

void SpawnPointTool::activate()
{
    BaseCellSceneTool::activate();

    mCursorItem = new SpawnPointCursorItem(mScene);
    mCursorItem->setZValue(mScene->ZVALUE_GRID + 1);
    mScene->addItem(mCursorItem);

    SpawnToolDialog::instance().setDocument(mScene->document());
    SpawnToolDialog::instance().show();
}

void SpawnPointTool::deactivate()
{
    SpawnToolDialog::instance().hide();
    SpawnToolDialog::instance().setDocument(0);

    delete mCursorItem;

    BaseCellSceneTool::activate();
}

void SpawnPointTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{

    if (event->button() == Qt::RightButton) {
        showContextMenu(event->scenePos(), event->screenPos());
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (SpawnPointItem *item = topmostItemAt(event->scenePos())) {
        QSet<WorldCellObject*> selection = mScene->document()->selectedObjects().toSet();
        if (event->modifiers() & Qt::ShiftModifier)
            selection += item->object();
        else if (event->modifiers() & Qt::ControlModifier) {
            if (selection.contains(item->object()))
                selection -= item->object();
            else
                selection += item->object();
        } else {
            selection.clear();
            selection += item->object();
        }
        mScene->document()->setSelectedObjects(selection.toList());
        event->accept();
        return;
    }

    // Try to ignore left-click that closes the context menu
    if (mContextMenuVisible || (mContextMenuShown.isValid() &&
            mContextMenuShown.msecsTo(QTime::currentTime()) < 500))
        return;

    event->accept();

    mScene->document()->undoStack()->beginMacro(tr("Add Spawn Point"));

    // Create the SpawnPoint object type if needed
    ObjectType *type = mScene->world()->objectType(QLatin1String("SpawnPoint"));
    if (!type) {
        type = new ObjectType(QLatin1String("SpawnPoint"));
        mScene->worldDocument()->addObjectType(type);
    }

    // Create the Professions property enum if needed
    PropertyEnum *pe = mScene->world()->propertyEnums().find(QLatin1String("Professions"));
    if (!pe) {
        QStringList professions;
        professions << QLatin1String("all")
                    << QLatin1String("unemployed")
                    << QLatin1String("policeofficer")
                    << QLatin1String("constructionworker")
                    << QLatin1String("securityguard")
                    << QLatin1String("parkranger")
                    << QLatin1String("fireofficer");
        mScene->worldDocument()->addPropertyEnum(QLatin1String("Professions"), professions, true);
        pe = mScene->world()->propertyEnums().find(QLatin1String("Professions"));
    }

    // Create the Professions property definition if needed
    PropertyDef *pd = mScene->world()->propertyDefinition(QLatin1String("Professions"));
    if (!pd) {
        pd = new PropertyDef(QLatin1String("Professions"), QLatin1String("all"),
                             tr("Comma-separated list of professions that may spawn here.  Use \"all\" to allow any profession to spawn here."),
                             pe);
        mScene->worldDocument()->addPropertyDefinition(pd);
    }

    // Create the SpawnPoint template if needed
    PropertyTemplate *pt = mScene->world()->propertyTemplate(QLatin1String("SpawnPoint"));
    if (!pt) {
        pt = new PropertyTemplate;
        pt->mName = QLatin1String("SpawnPoint");
        pt->mDescription = tr("This template holds the default set of properties for all spawn points in the world.");
        pt->addProperty(0, new Property(pd, pd->mDefaultValue));
        mScene->worldDocument()->addTemplate(pt);
    }

    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(),
                                                              mScene->document()->currentLevel());

    WorldObjectGroup *og = mScene->document()->currentObjectGroup();
    WorldCellObject *obj = new WorldCellObject(mScene->cell(),
                                               QString(), type, og,
                                               tilePos.x(), tilePos.y(),
                                               mScene->document()->currentLevel(),
                                               1, 1);
    obj->addTemplate(obj->templates().size(), pt);
    mScene->worldDocument()->addCellObject(mScene->cell(),
                                           mScene->cell()->objects().size(),
                                           obj);

    mScene->document()->setSelectedObjects(WorldCellObjectList() << obj);

    mScene->document()->undoStack()->endMacro();
}

void SpawnPointTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mCursorItem->setVisible(topmostItemAt(event->scenePos()) == 0);

    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(event->scenePos(),
                                                              mScene->document()->currentLevel());
    mCursorItem->mPos = tilePos;
    mCursorItem->mLevel = mScene->document()->currentLevel();
    mCursorItem->updateBounds();
}

void SpawnPointTool::showContextMenu(const QPointF &scenePos, const QPoint &screenPos)
{
    SpawnPointItem *item = topmostItemAt(scenePos);
    if (!item) {
        return;
    }

    mContextMenuVisible = true;

//    mScene->document()->setSelectedObjects(WorldCellObjectList() << item->object());

    QMenu menu;
    QIcon removeIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QAction *removeAction = menu.addAction(removeIcon, tr("Remove Spawn Point"));

    QAction *action = menu.exec(screenPos);
    if (action == removeAction) {
        mScene->worldDocument()->removeCellObject(mScene->cell(), item->object()->index());
    }

    mContextMenuVisible = false;
    mContextMenuShown = QTime::currentTime();
}

SpawnPointItem *SpawnPointTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (SpawnPointItem *spawnPtItem = dynamic_cast<SpawnPointItem*>(item)) {
            if (!spawnPtItem->isAdjacent())
                return spawnPtItem;
        }
    }
    return 0;
}

/////

CellCreateRoadTool *CellCreateRoadTool::mInstance = 0;

CellCreateRoadTool *CellCreateRoadTool::instance()
{
    if (!mInstance)
        mInstance = new CellCreateRoadTool();
    return mInstance;
}

void CellCreateRoadTool::deleteInstance()
{
    delete mInstance;
}

CellCreateRoadTool::CellCreateRoadTool()
    : BaseCellSceneTool(tr("Create Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-create.png")),
                         QKeySequence())
    , mCreating(false)
    , mCursorItem(new QGraphicsPolygonItem)
{
    mCursorItem->setBrush(Qt::cyan);
    mCursorItem->setOpacity(0.66);
    mCursorItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING + 1);
}

CellCreateRoadTool::~CellCreateRoadTool()
{
    delete mRoad;
    delete mRoadItem;
    delete mCursorItem;
}

void CellCreateRoadTool::activate()
{
    BaseCellSceneTool::activate();
    mScene->addItem(mCursorItem);
}

void CellCreateRoadTool::deactivate()
{
    mScene->removeItem(mCursorItem);
    BaseCellSceneTool::deactivate();
}

void CellCreateRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mCreating) {
        cancelNewRoadItem();
        event->accept();
    }
}

void CellCreateRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mCreating)
            return;
        startNewRoadItem(event->scenePos());
        mCreating = true;
    }
    if (event->button() == Qt::RightButton) {
        if (!mCreating)
            return;
        cancelNewRoadItem();
        mCreating = false;
    }
}

void CellCreateRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint roadPos = mScene->pixelToRoadCoords(event->scenePos());

    if (mCreating) {
        QPoint delta = roadPos - mStartRoadPos;
        if (qAbs(delta.x()) >= qAbs(delta.y())) {
            delta.setY(0); // horizontal road
        } else {
            delta.setX(0); // vertical road
        }
        mRoad->setCoords(mStartRoadPos, mStartRoadPos + delta);
        mRoadItem->synchWithRoad();

        roadPos = mRoad->end();
    }

    int roadWidth = currentRoadWidth();
    QPoint topLeft = roadPos - QPoint(roadWidth / 2, roadWidth / 2);
    QSize size(roadWidth, roadWidth);

    mCursorItem->setPolygon(mScene->roadRectToScenePolygon(QRect(topLeft, size)));
}

void CellCreateRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!mCreating)
            return;
        if (mRoad->start() == mRoad->end())
            return;
        finishNewRoadItem();
        mCreating = false;
    }
}

int CellCreateRoadTool::currentRoadWidth() const
{
    return WorldCreateRoadTool::instance()->currentRoadWidth();
}

TrafficLines *CellCreateRoadTool::currentTrafficLines() const
{
    return WorldCreateRoadTool::instance()->currentTrafficLines();
}

QString CellCreateRoadTool::currentTileName() const
{
    return WorldCreateRoadTool::instance()->currentTileName();
}

void CellCreateRoadTool::startNewRoadItem(const QPointF &scenePos)
{
    mStartRoadPos = mScene->pixelToRoadCoords(scenePos);
    mRoad = new Road(mScene->world(),
                     mStartRoadPos.x(), mStartRoadPos.y(),
                     mStartRoadPos.x(), mStartRoadPos.y(),
                     currentRoadWidth(), -1);
    mRoad->setTileName(currentTileName());
    mRoad->setTrafficLines(currentTrafficLines());
    mRoadItem = new CellRoadItem(mScene, mRoad);
    mRoadItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);
    mScene->addItem(mRoadItem);
}

void CellCreateRoadTool::clearNewRoadItem()
{
    mScene->removeItem(mRoadItem);
    delete mRoadItem;
    mRoadItem = 0;
}

void CellCreateRoadTool::cancelNewRoadItem()
{
    clearNewRoadItem();
    delete mRoad;
    mRoad = 0;
}

void CellCreateRoadTool::finishNewRoadItem()
{
    clearNewRoadItem();

    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    undoStack->push(new AddRoad(mScene->worldDocument(),
                                mScene->world()->roads().count(),
                                mRoad));
    mRoad = 0;
}

/////

CellEditRoadTool *CellEditRoadTool::mInstance = 0;

CellEditRoadTool *CellEditRoadTool::instance()
{
    if (!mInstance)
        mInstance = new CellEditRoadTool();
    return mInstance;
}

void CellEditRoadTool::deleteInstance()
{
    delete mInstance;
}

CellEditRoadTool::CellEditRoadTool()
    : BaseCellSceneTool(tr("Edit Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                         QKeySequence())
    , mSelectedRoadItem(0)
    , mRoad(0)
    , mRoadItem(0)
    , mMoving(false)
    , mStartHandle(new QGraphicsPolygonItem)
    , mEndHandle(new QGraphicsPolygonItem)
    , mHandlesVisible(false)
{
    mStartHandle->setBrush(Qt::cyan);
    mStartHandle->setOpacity(0.66);
    mStartHandle->setZValue(CellScene::ZVALUE_ROADITEM_CREATING + 1);

    mEndHandle->setBrush(Qt::cyan);
    mEndHandle->setOpacity(0.66);
    mEndHandle->setZValue(CellScene::ZVALUE_ROADITEM_CREATING + 1);
}

CellEditRoadTool::~CellEditRoadTool()
{
    delete mRoadItem;
    delete mRoad;
    delete mStartHandle;
    delete mEndHandle;
}

void CellEditRoadTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : 0;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(roadAboutToBeRemoved(int)),
                SLOT(roadAboutToBeRemoved(int)));
        connect(mScene->worldDocument(), SIGNAL(roadCoordsChanged(int)),
                SLOT(roadCoordsChanged(int)));
    }
}

void CellEditRoadTool::activate()
{
    BaseCellSceneTool::activate();
}

void CellEditRoadTool::deactivate()
{
    if (mHandlesVisible) {
        mScene->removeItem(mStartHandle);
        mScene->removeItem(mEndHandle);
        mHandlesVisible = false;
    }
    mSelectedRoadItem = 0;

    BaseCellSceneTool::deactivate();
}

void CellEditRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mMoving) {
        cancelMoving();
        mMoving = false;
        event->accept();
    }
}

void CellEditRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMoving)
            return;
        startMoving(event->scenePos());
    }
    if (event->button() == Qt::RightButton) {
        if (!mMoving)
            return;
        cancelMoving();
        mMoving = false;
    }
}

void CellEditRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!mMoving)
        return;
    QPoint curPos = mScene->pixelToRoadCoords(event->scenePos());
    QPoint delta = curPos - (mMovingStart
            ? mSelectedRoadItem->road()->start()
            : mSelectedRoadItem->road()->end());
    if (mSelectedRoadItem->road()->isVertical())
        delta.setX(0);
    else
        delta.setY(0);
    if (mMovingStart)
        mRoad->setCoords(mSelectedRoadItem->road()->start() + delta, mRoad->end());
    else
        mRoad->setCoords(mRoad->start(), mSelectedRoadItem->road()->end() + delta);
    mRoadItem->synchWithRoad();
    updateHandles(mRoad);
}

void CellEditRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!mMoving)
            return;
        if (mRoad->start() == mSelectedRoadItem->road()->start() &&
                mRoad->end() == mSelectedRoadItem->road()->end()) {
            cancelMoving();
            mMoving = false;
            return;
        }
        finishMoving();
        mMoving = false;
    }
}

// The read being edited could be deleted via undo/redo.
void CellEditRoadTool::roadAboutToBeRemoved(int index)
{
    Road *road = mScene->world()->roads().at(index);
    if (mSelectedRoadItem && road == mSelectedRoadItem->road()) {
        if (mMoving) {
            cancelMoving();
            mMoving = false;
        }
        updateHandles(0);
        mSelectedRoadItem = 0;
    }
}

void CellEditRoadTool::roadCoordsChanged(int index)
{
    Road *road = mScene->world()->roads().at(index);
    if (mSelectedRoadItem && road == mSelectedRoadItem->road())
        updateHandles(road);
}

void CellEditRoadTool::startMoving(const QPointF &scenePos)
{
    if (mSelectedRoadItem) {
        QPoint roadPos = mScene->pixelToRoadCoords(scenePos);
        if (mSelectedRoadItem->road()->startBounds().adjusted(0, 0, 1, 1).contains(roadPos)) {
            mMovingStart = true;
        } else if (mSelectedRoadItem->road()->endBounds().adjusted(0, 0, 1, 1).contains(roadPos)) {
            mMovingStart = false;
        } else {
            mSelectedRoadItem->setEditable(false);
            mSelectedRoadItem->setZValue(mSelectedRoadItem->isSelected() ?
                                             CellScene::ZVALUE_ROADITEM_SELECTED :
                                             CellScene::ZVALUE_ROADITEM_UNSELECTED);
            mSelectedRoadItem = 0;
        }
        if (mSelectedRoadItem) {
            mMoving = true;

            mRoad = new Road(mScene->world(),
                             mSelectedRoadItem->road()->x1(), mSelectedRoadItem->road()->y1(),
                             mSelectedRoadItem->road()->x2(), mSelectedRoadItem->road()->y2(),
                             mSelectedRoadItem->road()->width(), -1);
            mRoadItem = new CellRoadItem(mScene, mRoad);
            mRoadItem->setEditable(true);
            mRoadItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);
            mScene->addItem(mRoadItem);
            mSelectedRoadItem->setVisible(false);
            return;
        }
    }

    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (CellRoadItem *roadItem = dynamic_cast<CellRoadItem*>(item)) {
            mSelectedRoadItem = roadItem;
            mSelectedRoadItem->setEditable(true);
            mSelectedRoadItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);
            updateHandles(mSelectedRoadItem->road());
            break;
        }
    }
    updateHandles(mSelectedRoadItem ? mSelectedRoadItem->road() : 0);
}

void CellEditRoadTool::finishMoving()
{
    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    undoStack->push(new ChangeRoadCoords(mScene->worldDocument(),
                                         mSelectedRoadItem->road(),
                                         mRoad->start(), mRoad->end()));
    cancelMoving();
}

void CellEditRoadTool::cancelMoving()
{
    mSelectedRoadItem->setVisible(true);
    updateHandles(mSelectedRoadItem->road());

    mScene->removeItem(mRoadItem);
    delete mRoadItem;
    mRoadItem = 0;

    delete mRoad;
    mRoad = 0;
}

void CellEditRoadTool::updateHandles(Road *road)
{
    if (road) {
        mStartHandle->setPolygon(mScene->roadRectToScenePolygon(road->startBounds()));
        mEndHandle->setPolygon(mScene->roadRectToScenePolygon(road->endBounds()));
        if (mHandlesVisible == false) {
            mScene->addItem(mStartHandle);
            mScene->addItem(mEndHandle);
            mHandlesVisible = true;
        }
    } else if (mHandlesVisible) {
        mScene->removeItem(mStartHandle);
        mScene->removeItem(mEndHandle);
        mHandlesVisible = false;
    }
}

/////

CellSelectMoveRoadTool *CellSelectMoveRoadTool::mInstance = 0;

CellSelectMoveRoadTool *CellSelectMoveRoadTool::instance()
{
    if (!mInstance)
        mInstance = new CellSelectMoveRoadTool();
    return mInstance;
}

void CellSelectMoveRoadTool::deleteInstance()
{
    delete mInstance;
}

CellSelectMoveRoadTool::CellSelectMoveRoadTool()
    : BaseCellSceneTool(tr("Select and Move Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-select.png")),
                         QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mSelectionRectItem(new QGraphicsRectItem)
{
    mSelectionRectItem->setZValue(1000);
    mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
    mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
}

CellSelectMoveRoadTool::~CellSelectMoveRoadTool()
{
    delete mSelectionRectItem;
}

void CellSelectMoveRoadTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : 0;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(roadAboutToBeRemoved(int)),
                SLOT(roadAboutToBeRemoved(int)));
    }
}

void CellSelectMoveRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;
        foreach (Road *road, mMovingRoads)
            mScene->itemForRoad(road)->setDragging(false);
        mMovingRoads.clear();
        event->accept();
    }
}

void CellSelectMoveRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mDropRoadPos = mScene->pixelToRoadCoords(mStartScenePos);
        mClickedItem = topmostItemAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            foreach (Road *road, mMovingRoads)
                mScene->itemForRoad(road)->setDragging(false);
            mMovingRoads.clear();
        }
        break;
    default:
        break;
    }
}

void CellSelectMoveRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem &&
                    mScene->worldDocument()->selectedRoads().contains(mClickedItem->road()) &&
                    !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
    {
        QPointF start = mStartScenePos;
        QPointF end = event->scenePos();
        QRectF bounds = QRectF(start, end).normalized();
        mSelectionRectItem->setRect(bounds);
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

void CellSelectMoveRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
    {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        QList<Road*> newSelection;
        if (extend || toggle)
            newSelection = mScene->worldDocument()->selectedRoads();
        if (mClickedItem) {
            if (toggle && newSelection.contains(mClickedItem->road()))
                newSelection.removeOne(mClickedItem->road());
            else if (!newSelection.contains(mClickedItem->road()))
                newSelection += mClickedItem->road();
        }
        mScene->worldDocument()->setSelectedRoads(newSelection);
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

void CellSelectMoveRoadTool::roadAboutToBeRemoved(int index)
{
    if (mMode == Moving) {
        Road *road = mScene->world()->roads().at(index);
        if (mMovingRoads.contains(road)) {
            mScene->itemForRoad(road)->setDragging(false);
            mMovingRoads.removeAll(road);
            if (mMovingRoads.isEmpty())
                mMode = CancelMoving;
        }
    }
}

void CellSelectMoveRoadTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void CellSelectMoveRoadTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mStartScenePos;
    QPointF end = event->scenePos();
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    QList<Road*> selection;
    if (extend || toggle)
        selection = mScene->worldDocument()->selectedRoads();

    foreach (Road *road, mScene->roadsInRect(bounds)) {
        if (toggle && selection.contains(road))
            selection.removeOne(road);
        else if (!selection.contains(road))
            selection += road;
    }

    mScene->worldDocument()->setSelectedRoads(selection);
}

void CellSelectMoveRoadTool::startMoving()
{
    mMovingRoads = mScene->worldDocument()->selectedRoads();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingRoads.contains(mClickedItem->road())) {
        mMovingRoads.clear();
        mMovingRoads += mClickedItem->road();
        mScene->worldDocument()->setSelectedRoads(mMovingRoads);
    }

    mMode = Moving;
}

void CellSelectMoveRoadTool::updateMovingItems(const QPointF &pos,
                                      Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startPos = mScene->pixelToRoadCoords(mStartScenePos);
    mDropRoadPos = mScene->pixelToRoadCoords(pos);

    foreach (Road *road, mMovingRoads) {
        CellRoadItem *item = mScene->itemForRoad(road);
        item->setDragging(true);
        item->setDragOffset(mDropRoadPos - startPos);
    }
}

void CellSelectMoveRoadTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (Road *road, mMovingRoads)
        mScene->itemForRoad(road)->setDragging(false);

    QPoint startPos = mScene->pixelToRoadCoords(mStartScenePos);
    QPoint dropPos = mDropRoadPos;
    QPoint diff = dropPos - startPos;
    if (startPos != dropPos) {
        QUndoStack *undoStack = mScene->worldDocument()->undoStack();
        int count = mMovingRoads.size();
        undoStack->beginMacro(tr("Move %1 Road%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (Road *road, mMovingRoads) {
            mScene->worldDocument()->changeRoadCoords(road,
                                                      road->start() + diff,
                                                      road->end() + diff);
        }
        undoStack->endMacro();
    }

    mMovingRoads.clear();
}

CellRoadItem *CellSelectMoveRoadTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (CellRoadItem *roadItem = dynamic_cast<CellRoadItem*>(item))
            return roadItem;
    }
    return 0;
}

/////

SINGLETON_IMPL(CreatePointObjectTool)
SINGLETON_IMPL(CreatePolygonObjectTool)
SINGLETON_IMPL(CreatePolylineObjectTool)

AbstractCreatePolygonObjectTool::AbstractCreatePolygonObjectTool(ObjectGeometryType type)
    : BaseCellSceneTool(QString(),
                        QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                        QKeySequence())
    , mGeometryType(type)
    , mPathItem(nullptr)
{
    switch (mGeometryType) {
    case ObjectGeometryType::INVALID:
        break;
    case ObjectGeometryType::Point:
        setName(tr("Create Object (Point)"));
        break;
    case ObjectGeometryType::Polygon:
        setName(tr("Create Object (Polygon)"));
        break;
    case ObjectGeometryType::Polyline:
        setName(tr("Create Object (Polyline)"));
        break;
    }
}

AbstractCreatePolygonObjectTool::~AbstractCreatePolygonObjectTool()
{
    delete mPathItem;
}

void AbstractCreatePolygonObjectTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : nullptr;
}

void AbstractCreatePolygonObjectTool::activate()
{
    BaseCellSceneTool::activate();
}

void AbstractCreatePolygonObjectTool::deactivate()
{
    if (mPathItem != nullptr) {
        mScene->removeItem(mPathItem);
        delete mPathItem;
        mPathItem = nullptr;
        mPointItems.clear();
    }
    mPolygon.clear();
    BaseCellSceneTool::deactivate();
}

void AbstractCreatePolygonObjectTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mPathItem) {
        mPolygon.clear();
        event->accept();
    }
}

void AbstractCreatePolygonObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        addPoint(event->scenePos());
        updatePathItem();
    }
    if (event->button() == Qt::RightButton) {
        finishItem();
    }
}

void AbstractCreatePolygonObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScenePos = event->scenePos();
    updatePathItem();
}

void AbstractCreatePolygonObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

void AbstractCreatePolygonObjectTool::finishItem()
{
    WorldObjectGroup *og = mScene->document()->currentObjectGroup();

    switch (mGeometryType) {
    case ObjectGeometryType::INVALID:
        break;
    case ObjectGeometryType::Point:
        break;
    case ObjectGeometryType::Polygon:
        if (mPolygon.size() > 2) {
            WorldCellObject* object = new WorldCellObject(mScene->cell(),
                                                          QString(), og->type(), og,
                                                          mPolygon[0].x(), mPolygon[0].y(),
                                                          mScene->document()->currentLevel(),
                                                          MIN_OBJECT_SIZE, MIN_OBJECT_SIZE);
            WorldCellObjectPoints points;
            for (const QPoint& point : qAsConst(mPolygon)) {
                points += WorldCellObjectPoint(point.x(), point.y());
            }
            object->setGeometryType(mGeometryType);
            object->setPoints(points);
            mScene->worldDocument()->addCellObject(mScene->cell(), mScene->cell()->objects().size(), object);
        }
        break;
    case ObjectGeometryType::Polyline:
        if (mPolygon.size() > 1) {
            WorldCellObject* object = new WorldCellObject(mScene->cell(),
                                                          QString(), og->type(), og,
                                                          mPolygon[0].x(), mPolygon[0].y(),
                                                          mScene->document()->currentLevel(),
                                                          MIN_OBJECT_SIZE, MIN_OBJECT_SIZE);
            WorldCellObjectPoints points;
            for (const QPoint& point : qAsConst(mPolygon)) {
                points += WorldCellObjectPoint(point.x(), point.y());
            }
            object->setGeometryType(mGeometryType);
            object->setPoints(points);
            mScene->worldDocument()->addCellObject(mScene->cell(), mScene->cell()->objects().size(), object);
        }
        break;
    }

    mPolygon.clear();
    updatePathItem();
}

void AbstractCreatePolygonObjectTool::updatePathItem()
{
    QPainterPath path;

    if (mGeometryType == ObjectGeometryType::Polygon) {
        if (mPolygon.size() > 2) {
            path.addPolygon(mScene->renderer()->tileToPixelCoords(mPolygon));
        }
    }
#if 0
    if (mPolygon.isEmpty()) {
        QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mScenePos);
        QPointF scenePos = mScene->renderer()->tileToPixelCoords(tilePos);
        path.addRect(scenePos.x() - 5, scenePos.y() - 5, 10, 10);
    } else {
        for (const QPoint& point : qAsConst(mPolygon)) {
            QPointF p = mScene->renderer()->tileToPixelCoords(point);
            path.addRect(p.x() - 5, p.y() - 5, 10, 10);
        }
    }
#endif
    if (!mPolygon.isEmpty()) {
        QPointF p1 = mScene->renderer()->tileToPixelCoords(mPolygon[0]);
        path.moveTo(p1);
        for (int i = 1; i < mPolygon.size(); i++) {
            QPointF p2 = mScene->renderer()->tileToPixelCoords(mPolygon[i]);
            path.lineTo(p2);
        }

        // Line to mouse pointer
        QPointF p2 = mScene->renderer()->pixelToTileCoordsNearest(mScenePos);
        p2 = mScene->renderer()->tileToPixelCoords(p2);
        path.lineTo(p2);
    }

    if (mPathItem == nullptr) {
        mPathItem = new QGraphicsPathItem();
        QPen pen(Qt::blue);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(3);
        pen.setCosmetic(true);
        mPathItem->setPen(pen);
        mScene->addItem(mPathItem);
    }

    mPathItem->setPath(path);

    // Add an item for each new point.
    for (int i = 0; i < mPolygon.size(); i++) {
        QPoint point = mPolygon[i];
        if (i >= mPointItems.size()) {
            QGraphicsRectItem *item = new QGraphicsRectItem(-5, -5, 10, 10, mPathItem);
            item->setBrush(Qt::blue);
            item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            item->setPos(mScene->renderer()->tileToPixelCoords(point.x(), point.y()));
            mPointItems += item;
        }
    }

    // Remove excess point items.
    for (int i = mPolygon.size() + 1; i < mPointItems.size(); i++) {
        delete mPointItems.takeAt(i--);
    }

    // Create and/or update the point at the cursor position.
    if (mPointItems.size() == mPolygon.size()) {
        QGraphicsRectItem *item = new QGraphicsRectItem(-5, -5, 10, 10, mPathItem);
        item->setBrush(Qt::blue);
        item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        mPointItems += item;
    }
    QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mScenePos);
    QPointF scenePos = mScene->renderer()->tileToPixelCoords(tilePos);
    mPointItems.last()->setPos(scenePos);
}

void AbstractCreatePolygonObjectTool::addPoint(const QPointF &scenePos)
{
    if (mGeometryType == ObjectGeometryType::Point) {
        WorldObjectGroup *og = mScene->document()->currentObjectGroup();
        QPointF cellPos = mScene->renderer()->pixelToTileCoordsNearest(scenePos);
        WorldCellObjectPoints points;
        points += WorldCellObjectPoint(cellPos.x(), cellPos.y());
        WorldCellObject* object = new WorldCellObject(mScene->cell(),
                                                      QString(), og->type(), og,
                                                      points[0].x, points[0].y,
                                                      mScene->document()->currentLevel(),
                                                      MIN_OBJECT_SIZE, MIN_OBJECT_SIZE);
        object->setGeometryType(mGeometryType);
        object->setPoints(points);
        mScene->worldDocument()->addCellObject(mScene->cell(), mScene->cell()->objects().size(), object);
        return;
    }

    QPoint tilePos = mScene->renderer()->pixelToTileCoordsNearest(scenePos);
    if ((mPolygon.isEmpty() == false) && (mPolygon[0] == tilePos)) {
        // TODO: Allow Polyline to end where it starts?
        finishItem();
        return;
    }
    if (mPolygon.contains(tilePos)) {
        return;
    }

    mPolygon += mScene->renderer()->pixelToTileCoordsNearest(scenePos);
}

/////

SINGLETON_IMPL(EditPolygonObjectTool)

EditPolygonObjectTool::EditPolygonObjectTool()
    : BaseCellSceneTool(tr("Edit Object Points"),
                         QIcon(QLatin1String(":/images/24x24/tool-edit-polygons.png")),
                         QKeySequence())
    , mSelectedObjectItem(nullptr)
    , mSelectedObject(nullptr)
    , mRectItem(nullptr)
{
}

EditPolygonObjectTool::~EditPolygonObjectTool()
{
    delete mRectItem;
}

void EditPolygonObjectTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene) {
        mScene->worldDocument()->disconnect(this);
        mScene->document()->disconnect(this);
    }

    mScene = scene ? scene->asCellScene() : nullptr;

    if (mScene) {
        connect(mScene->worldDocument(), &WorldDocument::cellObjectAboutToBeRemoved,
                this, &EditPolygonObjectTool::cellObjectAboutToBeRemoved);
        connect(mScene->worldDocument(), &WorldDocument::cellObjectMoved,
                this, &EditPolygonObjectTool::cellObjectMoved);
        connect(mScene->worldDocument(), &WorldDocument::cellObjectPointMoved,
                this, &EditPolygonObjectTool::cellObjectPointMoved);
        connect(mScene->worldDocument(), &WorldDocument::cellObjectPointsChanged,
                this, &EditPolygonObjectTool::cellObjectPointsChanged);

        connect(mScene->document(), &CellDocument::selectedObjectsChanged,
                this, &EditPolygonObjectTool::selectedObjectsChanged);
        connect(mScene->document(), &CellDocument::selectedObjectPointsChanged,
                this, &EditPolygonObjectTool::selectedObjectPointsChanged);
    }
}

void EditPolygonObjectTool::activate()
{
    BaseCellSceneTool::activate();
}

void EditPolygonObjectTool::deactivate()
{
    if (mSelectedObjectItem) {
        mSelectedObjectItem->setEditable(false);
        for (ObjectPointHandle* handle : qAsConst(mHandles)) {
            mScene->removeItem(handle);
            delete handle;
        }
        mHandles.clear();
        mSelectedObjectItem = nullptr;
        mSelectedObject = nullptr;
        mScene->document()->setSelectedObjectPoints(QList<int>());
    }

    if (mRectItem != nullptr) {
        mScene->removeItem(mRectItem);
        delete mRectItem;
        mRectItem = nullptr;
    }

    BaseCellSceneTool::deactivate();
}

void EditPolygonObjectTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape)) {
        event->accept();
    }
}

void EditPolygonObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        ObjectItem* clickedItem = nullptr;
        const QList<QGraphicsItem*> items = mScene->items(event->scenePos());
        for (QGraphicsItem *item : items) {
            if (ObjectItem *objectItem = dynamic_cast<ObjectItem*>(item)) {
                if (objectItem->isAdjacent())
                    continue;
                if (objectItem->isRectangle())
                    continue;
                clickedItem = objectItem;
                break;
            }
        }
        if ((clickedItem != nullptr) && (clickedItem == mSelectedObjectItem)) {
            if (mSelectedObjectItem->mAddPointIndex != -1) {
                QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mSelectedObjectItem->mAddPointPos);
                WorldCellObjectPoint point(tilePos.x(), tilePos.y());
                if (mSelectedObject->points().contains(point)) {
                    return;
                }
                WorldCellObjectPoints coords = mSelectedObject->points();
                coords.insert(mSelectedObjectItem->mAddPointIndex + 1, point);
                mScene->worldDocument()->setCellObjectPoints(mScene->cell(), mSelectedObject->index(), coords);
            }
            return;
        }
        if (clickedItem == nullptr)
            mScene->setSelectedObjectItems(QSet<ObjectItem*>());
        else
            mScene->setSelectedObjectItems(QSet<ObjectItem*>() << clickedItem);
        mScene->document()->setSelectedObjectPoints(QList<int>());
    }
    if (event->button() == Qt::RightButton) {
    }
}

void EditPolygonObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mSelectedObjectItem && mSelectedObjectItem->mAddPointIndex != -1) {
        if (mRectItem == nullptr) {
            mRectItem = new QGraphicsRectItem();
            mRectItem->setBrush(Qt::red);
            mRectItem->setRect(0 - 5, 0 - 5, 10, 10);
            mRectItem->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            scene()->addItem(mRectItem);
        }
        QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mSelectedObjectItem->mAddPointPos);
        QPointF scenePos = mScene->renderer()->tileToPixelCoords(tilePos);
        mRectItem->setPos(scenePos);
    } else {
        if (mRectItem) {
            delete mRectItem;
            mRectItem = nullptr;
        }
    }
}

void EditPolygonObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

// The object being edited could be deleted via undo/redo.
void EditPolygonObjectTool::cellObjectAboutToBeRemoved(WorldCell* cell, int objectIndex)
{
    WorldCellObject* object = cell->objects().at(objectIndex);
    if (object == mSelectedObject) {
        setSelectedItem(nullptr);
        mScene->document()->setSelectedObjectPoints(QList<int>());
    }
}

void EditPolygonObjectTool::cellObjectMoved(WorldCellObject *object)
{
    if (object == mSelectedObject) {
        setSelectedItem(mSelectedObjectItem);
    }
}

void EditPolygonObjectTool::cellObjectPointMoved(WorldCell* cell, int objectIndex, int pointIndex)
{
    Q_UNUSED(pointIndex)
    WorldCellObject* object = cell->objects().at(objectIndex);
    if (object == mSelectedObject) {
//        mSelectedObjectItem->synchWithObject();
        setSelectedItem(mSelectedObjectItem);
    }
}

void EditPolygonObjectTool::cellObjectPointsChanged(WorldCell *cell, int objectIndex)
{
    WorldCellObject* object = cell->objects().at(objectIndex);
    if (object == mSelectedObject) {
        setSelectedItem(mSelectedObjectItem);
    }
}

void EditPolygonObjectTool::selectedObjectsChanged()
{
    auto& selected = mScene->document()->selectedObjects();
    if (selected.size() == 1 && isCurrent()) {
        WorldCellObject* object = selected.first();
        if (ObjectItem* item = mScene->itemForObject(object)) {
            setSelectedItem(item);
        }
    } else {
        setSelectedItem(nullptr);
    }
}

void EditPolygonObjectTool::selectedObjectPointsChanged()
{
    for (auto* handle : qAsConst(mHandles)) {
        handle->update();
    }
}

void EditPolygonObjectTool::setSelectedItem(ObjectItem *objectItem)
{
    if (mSelectedObjectItem) {
        // The item may have been deleted if inside objectAboutToBeRemoved()
        if (mScene->itemForObject(mSelectedObject)) {
            for (auto* handle : qAsConst(mHandles)) {
                mScene->removeItem(handle);
                delete handle;
            }
            mSelectedObjectItem->setEditable(false);
            mSelectedObjectItem->setZValue(CellScene::ZVALUE_ROADITEM_UNSELECTED);
        }
        mHandles.clear();
        mSelectedObjectItem = nullptr;
        mSelectedObject = nullptr;
    }

    if (objectItem) {
        objectItem->mSyncing = true;

        auto createHandle = [&](int pointIndex) {
            ObjectPointHandle* handle = new ObjectPointHandle(objectItem, pointIndex);
            WorldCellObjectPoint point = handle->geometryPoint();
            handle->setPos(mScene->renderer()->tileToPixelCoords(point.x, point.y));
//            mScene->addItem(handle);
            mHandles += handle;
        };
        switch (objectItem->object()->geometryType()) {
        case ObjectGeometryType::INVALID:
            break;
        case ObjectGeometryType::Point:
            for (int i = 0; i < objectItem->object()->points().size(); i++) {
                createHandle(i);
            }
            break;
        case ObjectGeometryType::Polygon:
            for (int i = 0; i < objectItem->object()->points().size(); i++) {
                createHandle(i);
            }
            break;
        case ObjectGeometryType::Polyline:
            for (int i = 0; i < objectItem->object()->points().size(); i++) {
                createHandle(i);
            }
            break;
        }

        mSelectedObjectItem = objectItem;
        mSelectedObject = objectItem->object();
        mSelectedObjectItem->setEditable(true);
        mSelectedObjectItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);

        objectItem->mSyncing = false;
    }
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

BaseGraphicsScene *BaseWorldSceneTool::scene() const
{
    return mScene;
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
        if (mMode == NoMode) {
            showContextMenu(event->scenePos(), event->screenPos());
        }
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

void WorldCellTool::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    mClickedItem = topmostItemAt(event->scenePos());
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
    QVector<QPoint> cellPositions(mDnDItems.size());
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

void WorldCellTool::showContextMenu(const QPointF &scenePos, const QPoint &screenPos)
{
    WorldCellItem *item = topmostItemAt(scenePos);
    if (!item)
        return;

    QMenu menu;
    QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
    QAction *openAction = menu.addAction(tiledIcon, tr("Open in TileZed"));
    QAction *thumbnailAction = menu.addAction(tr("Recreate Thumbnail"));
    if (item->cell()->mapFilePath().isEmpty()) {
        openAction->setEnabled(false);
        thumbnailAction->setEnabled(false);
    }

    QAction *action = menu.exec(screenPos);
    if (action == openAction) {
        QUrl url = QUrl::fromLocalFile(item->cell()->mapFilePath());
        QDesktopServices::openUrl(url);
    }
    if (action == thumbnailAction) {
        MapImageManager::instance()->recreateMapImage(item->mapFilePath());
    }
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

/////

WorldCreateRoadTool *WorldCreateRoadTool::mInstance = 0;

WorldCreateRoadTool *WorldCreateRoadTool::instance()
{
    if (!mInstance)
        mInstance = new WorldCreateRoadTool();
    return mInstance;
}

void WorldCreateRoadTool::deleteInstance()
{
    delete mInstance;
}

WorldCreateRoadTool::WorldCreateRoadTool()
    : BaseWorldSceneTool(tr("Create Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-create.png")),
                         QKeySequence())
    , mCreating(false)
    , mCurrentRoadWidth(8)
    , mCurrentTrafficLines(RoadTemplates::instance()->nullTrafficLines())
    , mCursorItem(new QGraphicsPolygonItem)
{
    mCursorItem->setBrush(Qt::cyan);
    mCursorItem->setOpacity(0.66);
    mCursorItem->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING + 0.5);
}

WorldCreateRoadTool::~WorldCreateRoadTool()
{
    delete mRoad;
    delete mRoadItem;
    delete mCursorItem;
}

void WorldCreateRoadTool::activate()
{
    BaseWorldSceneTool::activate();
    mScene->addItem(mCursorItem);
}

void WorldCreateRoadTool::deactivate()
{
    mScene->removeItem(mCursorItem);
    BaseWorldSceneTool::deactivate();
}

void WorldCreateRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mCreating) {
        cancelNewRoadItem();
        event->accept();
    }
}

void WorldCreateRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mCreating)
            return;
        startNewRoadItem(event->scenePos());
        mCreating = true;
    }
    if (event->button() == Qt::RightButton) {
        if (!mCreating)
            return;
        cancelNewRoadItem();
        mCreating = false;
    }
}

void WorldCreateRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint roadPos = mScene->pixelToRoadCoords(event->scenePos());

    if (mCreating) {
        QPoint delta = roadPos - mStartRoadPos;
        if (qAbs(delta.x()) >= qAbs(delta.y())) {
            delta.setY(0); // horizontal road
        } else {
            delta.setX(0); // vertical road
        }
        mRoad->setCoords(mStartRoadPos, mStartRoadPos + delta);
        mRoadItem->synchWithRoad();

        roadPos = mRoad->end();
    }

    QPoint topLeft = roadPos - QPoint(mCurrentRoadWidth / 2, mCurrentRoadWidth / 2);
    QSize size(mCurrentRoadWidth, mCurrentRoadWidth);

    mCursorItem->setPolygon(mScene->roadRectToScenePolygon(QRect(topLeft, size)));
}

void WorldCreateRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!mCreating)
            return;
        if (mRoad->start() == mRoad->end())
            return;
        finishNewRoadItem();
        mCreating = false;
    }
}

void WorldCreateRoadTool::startNewRoadItem(const QPointF &scenePos)
{
    mStartRoadPos = mScene->pixelToRoadCoords(scenePos);
    mRoad = new Road(mScene->world(),
                     mStartRoadPos.x(), mStartRoadPos.y(),
                     mStartRoadPos.x(), mStartRoadPos.y(),
                     mCurrentRoadWidth, -1);
    mRoad->setTileName(mCurrentTileName);
    mRoad->setTrafficLines(mCurrentTrafficLines);
    mRoadItem = new WorldRoadItem(mScene, mRoad);
    mRoadItem->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING);
    mScene->addItem(mRoadItem);
}

void WorldCreateRoadTool::clearNewRoadItem()
{
    mScene->removeItem(mRoadItem);
    delete mRoadItem;
    mRoadItem = 0;
}

void WorldCreateRoadTool::cancelNewRoadItem()
{
    clearNewRoadItem();
    delete mRoad;
    mRoad = 0;
}

void WorldCreateRoadTool::finishNewRoadItem()
{
    clearNewRoadItem();

    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    undoStack->push(new AddRoad(mScene->worldDocument(),
                                mScene->world()->roads().count(),
                                mRoad));
    mRoad = 0;
}

#if 0

/////

/**
 * A handle that allows moving around a point of a Road.
 */
class RoadPointHandle : public QGraphicsItem
{
public:
    RoadPointHandle(RoadItem *roadItem, int pointIndex)
        : QGraphicsItem()
        , mRoadItem(roadItem)
        , mPointIndex(pointIndex)
        , mSelected(false)
        , mDragging(false)
    {
        setFlags(QGraphicsItem::ItemIgnoresTransformations |
                 QGraphicsItem::ItemIgnoresParentOpacity);
        setZValue(10000);
        setCursor(Qt::SizeAllCursor);
    }

    Road *road() const { return mRoadItem->road(); }

    int pointIndex() const { return mPointIndex; }

    QPoint pointPosition() const;
    void setPointPosition(const QPoint &pos);

    // These override the QGraphicsItem members
    void setSelected(bool selected) { mSelected = selected; update(); }
    bool isSelected() const { return mSelected; }

    void setDragging(bool dragging)
    {
        if (dragging != mDragging) {
            mDragging = dragging;
            if (mDragging)
                mStartPos = pointPosition();
        }
    }

    void setDragOffset(const QPoint &offset)
    {
        mDragOffset = offset;
        update();
    }

    QPoint dragOffset() const
    { return mDragOffset; }

    QPoint startPosition() const
    { return mStartPos; }

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    RoadItem *mRoadItem;
    int mPointIndex;
    bool mSelected;
    bool mDragging;
    QPoint mDragOffset;
    QPoint mStartPos;
};

QPoint RoadPointHandle::pointPosition() const
{
    if (mPointIndex == 0)
        return road()->start();
    else
        return road()->end();
}

void RoadPointHandle::setPointPosition(const QPoint &pos)
{
    if (mPointIndex == 0)
        road()->setCoords(pos, road()->end());
    else
        road()->setCoords(road()->start(), pos);
    mRoadItem->synchWithRoad();
}

QRectF RoadPointHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void RoadPointHandle::paint(QPainter *painter,
                            const QStyleOptionGraphicsItem *,
                            QWidget *)
{
    painter->setPen(Qt::black);
    if (mSelected) {
        painter->setBrush(QApplication::palette().highlight());
        painter->drawRect(QRectF(-4, -4, 8, 8));
    } else {
        painter->setBrush(Qt::lightGray);
        painter->drawRect(QRectF(-3, -3, 6, 6));
    }
}

#endif

/////

WorldEditRoadTool *WorldEditRoadTool::mInstance = 0;

WorldEditRoadTool *WorldEditRoadTool::instance()
{
    if (!mInstance)
        mInstance = new WorldEditRoadTool();
    return mInstance;
}

void WorldEditRoadTool::deleteInstance()
{
    delete mInstance;
}

WorldEditRoadTool::WorldEditRoadTool()
    : BaseWorldSceneTool(tr("Edit Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                         QKeySequence())
    , mSelectedRoad(0)
    , mSelectedRoadItem(0)
    , mRoad(0)
    , mRoadItem(0)
    , mMoving(false)
    , mStartHandle(new QGraphicsPolygonItem)
    , mEndHandle(new QGraphicsPolygonItem)
    , mHandlesVisible(false)
{
    mStartHandle->setBrush(Qt::cyan);
    mStartHandle->setOpacity(0.66);
    mStartHandle->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING + 0.5);

    mEndHandle->setBrush(Qt::cyan);
    mEndHandle->setOpacity(0.66);
    mEndHandle->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING + 0.5);
}

WorldEditRoadTool::~WorldEditRoadTool()
{
    delete mRoadItem;
    delete mRoad;
    delete mStartHandle;
    delete mEndHandle;
}

void WorldEditRoadTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asWorldScene() : 0;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(roadCoordsChanged(int)),
                SLOT(roadCoordsChanged(int)));
        connect(mScene->worldDocument(), SIGNAL(roadAboutToBeRemoved(int)),
                SLOT(roadAboutToBeRemoved(int)));
    }
}

void WorldEditRoadTool::activate()
{
    BaseWorldSceneTool::activate();
}

void WorldEditRoadTool::deactivate()
{
    if (mHandlesVisible) {
        mScene->removeItem(mStartHandle);
        mScene->removeItem(mEndHandle);
        mHandlesVisible = false;
    }
    BaseWorldSceneTool::deactivate();
}

void WorldEditRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mMoving) {
        cancelMoving();
        event->accept();
    }
}

void WorldEditRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMoving)
            return;
        startMoving(event->scenePos());
    }
    if (event->button() == Qt::RightButton) {
        if (!mMoving)
            return;
        cancelMoving();
        mMoving = false;
    }
}

void WorldEditRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!mMoving)
        return;
    QPoint curPos = mScene->pixelToRoadCoords(event->scenePos());
    QPoint delta = curPos - (mMovingStart
            ? mSelectedRoadItem->road()->start()
            : mSelectedRoadItem->road()->end());
    if (mSelectedRoadItem->road()->isVertical())
        delta.setX(0);
    else
        delta.setY(0);
    if (mMovingStart)
        mRoad->setCoords(mSelectedRoadItem->road()->start() + delta, mRoad->end());
    else
        mRoad->setCoords(mRoad->start(), mSelectedRoadItem->road()->end() + delta);
    mRoadItem->synchWithRoad();
    updateHandles(mRoad);
}

void WorldEditRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!mMoving)
            return;
        if (mRoad->start() == mSelectedRoadItem->road()->start() &&
                mRoad->end() == mSelectedRoadItem->road()->end()) {
            cancelMoving();
            mMoving = false;
            return;
        }
        finishMoving();
        mMoving = false;
    }
}

// The road we are editing could be deleted via undo/redo
void WorldEditRoadTool::roadAboutToBeRemoved(int index)
{
    Road *road = mScene->world()->roads().at(index);
    if (mSelectedRoad && road == mSelectedRoad) {
        if (mMoving) {
            mSelectedRoadItem = 0; // could have been deleted by WorldScene already
            cancelMoving();
            mMoving = false;
        }
        updateHandles(0);
        mSelectedRoadItem = 0;
        mSelectedRoad = 0;
    }
}

void WorldEditRoadTool::roadCoordsChanged(int index)
{
    Road *road = mScene->world()->roads().at(index);
    if (mSelectedRoadItem && road == mSelectedRoadItem->road())
        updateHandles(road);
}

void WorldEditRoadTool::startMoving(const QPointF &scenePos)
{
    if (mSelectedRoadItem) {
        QPoint roadPos = mScene->pixelToRoadCoords(scenePos);
        if (mSelectedRoadItem->road()->startBounds().adjusted(0, 0, 1, 1).contains(roadPos)) {
            mMovingStart = true;
        } else if (mSelectedRoadItem->road()->endBounds().adjusted(0, 0, 1, 1).contains(roadPos)) {
            mMovingStart = false;
        } else {
            mSelectedRoadItem->setEditable(false);
            mSelectedRoadItem->setZValue(mSelectedRoadItem->isSelected() ?
                                             WorldScene::ZVALUE_ROADITEM_SELECTED :
                                             WorldScene::ZVALUE_ROADITEM_UNSELECTED);
            mSelectedRoadItem = 0;
            mSelectedRoad = 0;
        }
        if (mSelectedRoadItem) {
            mMoving = true;

            mRoad = new Road(mScene->world(),
                             mSelectedRoadItem->road()->x1(), mSelectedRoadItem->road()->y1(),
                             mSelectedRoadItem->road()->x2(), mSelectedRoadItem->road()->y2(),
                             mSelectedRoadItem->road()->width(), -1);
            mRoadItem = new WorldRoadItem(mScene, mRoad);
            mRoadItem->setEditable(true);
            mRoadItem->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING);
            mScene->addItem(mRoadItem);
            mSelectedRoadItem->setVisible(false);
            return;
        }
    }

    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (WorldRoadItem *roadItem = dynamic_cast<WorldRoadItem*>(item)) {
            mSelectedRoad = roadItem->road();
            mSelectedRoadItem = roadItem;
            mSelectedRoadItem->setEditable(true);
            mSelectedRoadItem->setZValue(WorldScene::ZVALUE_ROADITEM_CREATING);
            updateHandles(mSelectedRoadItem->road());
            break;
        }
    }
    updateHandles(mSelectedRoadItem ? mSelectedRoadItem->road() : 0);
}

void WorldEditRoadTool::finishMoving()
{
    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    undoStack->push(new ChangeRoadCoords(mScene->worldDocument(),
                                         mSelectedRoadItem->road(),
                                         mRoad->start(), mRoad->end()));
    cancelMoving();
}

void WorldEditRoadTool::cancelMoving()
{
    if (mSelectedRoadItem) {
        mSelectedRoadItem->setVisible(true);
        updateHandles(mSelectedRoadItem->road());
    } else
        updateHandles(0);

    mScene->removeItem(mRoadItem);
    delete mRoadItem;
    mRoadItem = 0;

    delete mRoad;
    mRoad = 0;
}

void WorldEditRoadTool::updateHandles(Road *road)
{
    if (road) {
        mStartHandle->setPolygon(mScene->roadRectToScenePolygon(road->startBounds()));
        mEndHandle->setPolygon(mScene->roadRectToScenePolygon(road->endBounds()));
        if (mHandlesVisible == false) {
            mScene->addItem(mStartHandle);
            mScene->addItem(mEndHandle);
            mHandlesVisible = true;
        }
    } else if (mHandlesVisible) {
        mScene->removeItem(mStartHandle);
        mScene->removeItem(mEndHandle);
        mHandlesVisible = false;
    }
}

/////

WorldSelectMoveRoadTool *WorldSelectMoveRoadTool::mInstance = 0;

WorldSelectMoveRoadTool *WorldSelectMoveRoadTool::instance()
{
    if (!mInstance)
        mInstance = new WorldSelectMoveRoadTool();
    return mInstance;
}

void WorldSelectMoveRoadTool::deleteInstance()
{
    delete mInstance;
}

WorldSelectMoveRoadTool::WorldSelectMoveRoadTool()
    : BaseWorldSceneTool(tr("Select and Move Roads"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-select.png")),
                         QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mSelectionRectItem(new QGraphicsPolygonItem)
{
    mSelectionRectItem->setZValue(1000);
    mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
    mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
}

WorldSelectMoveRoadTool::~WorldSelectMoveRoadTool()
{
    delete mSelectionRectItem;
}

void WorldSelectMoveRoadTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asWorldScene() : 0;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(roadAboutToBeRemoved(int)),
                SLOT(roadAboutToBeRemoved(int)));
    }
}

void WorldSelectMoveRoadTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        cancelMoving();
        event->accept();
    }
}

void WorldSelectMoveRoadTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mDropRoadPos = mScene->pixelToRoadCoords(mStartScenePos);
        mClickedItem = topmostItemAt(mStartScenePos);
        break;
    case Qt::RightButton:
        cancelMoving();
        break;
    default:
        break;
    }
}

void WorldSelectMoveRoadTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem &&
                    mScene->worldDocument()->selectedRoads().contains(mClickedItem->road()) &&
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

void WorldSelectMoveRoadTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
    {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        QList<Road*> newSelection;
        if (extend || toggle)
            newSelection = mScene->worldDocument()->selectedRoads();
        if (mClickedItem) {
            if (toggle && newSelection.contains(mClickedItem->road()))
                newSelection.removeOne(mClickedItem->road());
            else if (!newSelection.contains(mClickedItem->road()))
                newSelection += mClickedItem->road();
        }
        mScene->worldDocument()->setSelectedRoads(newSelection);
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

void WorldSelectMoveRoadTool::roadAboutToBeRemoved(int index)
{
    if (mMode == Moving) {
        Road *road = mScene->world()->roads().at(index);
        if (mMovingRoads.contains(road)) {
//            mScene->itemForRoad(road)->setDragging(false);
            mMovingRoads.removeAll(road);
            if (mMovingRoads.isEmpty())
                mMode = CancelMoving;
        }
    }
}

void WorldSelectMoveRoadTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void WorldSelectMoveRoadTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mScene->pixelToCellCoords(mStartScenePos);
    QPointF end = mScene->pixelToCellCoords(event->scenePos());
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    QList<Road*> selection;
    if (extend || toggle)
        selection = mScene->worldDocument()->selectedRoads();

    foreach (Road *road, mScene->roadsInRect(bounds)) {
        if (toggle && selection.contains(road))
            selection.removeOne(road);
        else if (!selection.contains(road))
            selection += road;
    }

    mScene->worldDocument()->setSelectedRoads(selection);
}

void WorldSelectMoveRoadTool::startMoving()
{
    mMovingRoads = mScene->worldDocument()->selectedRoads();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingRoads.contains(mClickedItem->road())) {
        mMovingRoads.clear();
        mMovingRoads += mClickedItem->road();
        mScene->worldDocument()->setSelectedRoads(mMovingRoads);
    }

    mMode = Moving;
}

void WorldSelectMoveRoadTool::updateMovingItems(const QPointF &pos,
                                      Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startPos = mScene->pixelToRoadCoords(mStartScenePos);
    mDropRoadPos = mScene->pixelToRoadCoords(pos);

    foreach (Road *road, mMovingRoads) {
        WorldRoadItem *item = mScene->itemForRoad(road);
        item->setDragging(true);
        item->setDragOffset(mDropRoadPos - startPos);
    }
}

void WorldSelectMoveRoadTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (Road *road, mMovingRoads)
        mScene->itemForRoad(road)->setDragging(false);

    QPoint startPos = mScene->pixelToRoadCoords(mStartScenePos);
    QPoint dropPos = mDropRoadPos;
    QPoint diff = dropPos - startPos;
    if (startPos != dropPos) {
        QUndoStack *undoStack = mScene->worldDocument()->undoStack();
        int count = mMovingRoads.size();
        undoStack->beginMacro(tr("Move %1 Road%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (Road *road, mMovingRoads) {
            mScene->worldDocument()->changeRoadCoords(road,
                                                      road->start() + diff,
                                                      road->end() + diff);
        }
        undoStack->endMacro();
    }

    mMovingRoads.clear();
}

void WorldSelectMoveRoadTool::cancelMoving()
{
    if (mMode == Moving) {
        mMode = CancelMoving;
        foreach (Road *road, mMovingRoads)
            mScene->itemForRoad(road)->setDragging(false);
        mMovingRoads.clear();
    }
}

WorldRoadItem *WorldSelectMoveRoadTool::topmostItemAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (WorldRoadItem *roadItem = dynamic_cast<WorldRoadItem*>(item))
            return roadItem;
    }
    return 0;
}

/////

WorldBMPTool *WorldBMPTool::mInstance = 0;

WorldBMPTool *WorldBMPTool::instance()
{
    if (!mInstance)
        mInstance = new WorldBMPTool();
    return mInstance;
}

void WorldBMPTool::deleteInstance()
{
    delete mInstance;
}

WorldBMPTool::WorldBMPTool()
    : BaseWorldSceneTool(tr("Select and Move BMP Images"),
                         QIcon(QLatin1String(":/images/22x22/bmp-tool-select.png")),
                         QKeySequence())
    , mMode(NoMode)
    , mMousePressed(false)
    , mSelectionRectItem(new QGraphicsPolygonItem)
{
    mSelectionRectItem->setZValue(1000);
    mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
    mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
}

WorldBMPTool::~WorldBMPTool()
{
}

void WorldBMPTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asWorldScene() : 0;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(bmpAboutToBeRemoved(int)),
                SLOT(bmpAboutToBeRemoved(int)));
    }
}

void WorldBMPTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        cancelMoving();
        event->accept();
    }
}

void WorldBMPTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mDragOffset = QPoint();
        mClickedItem = topmostItemAt(mStartScenePos);
        break;
    case Qt::RightButton:
        if (mMode == Moving)
            cancelMoving();
        break;
    default:
        break;
    }
}

void WorldBMPTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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

void WorldBMPTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
    {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        QList<WorldBMP*> newSelection;
        if (extend || toggle)
            newSelection = mScene->worldDocument()->selectedBMPs();
        if (mClickedItem) {
            WorldBMP *bmp = mClickedItem->bmp();
            if (toggle && newSelection.contains(bmp))
                newSelection.removeOne(bmp);
            else if (!newSelection.contains(bmp))
                newSelection += bmp;
        }
        mScene->worldDocument()->setSelectedBMPs(newSelection);
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

void WorldBMPTool::bmpAboutToBeRemoved(int index)
{
    if (mMode == Moving) {
        WorldBMP *bmp = mScene->world()->bmps().at(index);
        if (mMovingBMPs.contains(bmp)) {
//            mScene->itemForBMP(bmp)->setDragging(false);
            mMovingBMPs.removeAll(bmp);
            if (mMovingBMPs.isEmpty())
                mMode = CancelMoving;
        }
    }
}

void WorldBMPTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void WorldBMPTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mScene->pixelToCellCoords(mStartScenePos);
    QPointF end = mScene->pixelToCellCoords(event->scenePos());
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    QList<WorldBMP*> selection;
    if (extend || toggle)
        selection = mScene->worldDocument()->selectedBMPs();

    foreach (WorldBMP *bmp, mScene->bmpsInRect(bounds)) {
        if (toggle && selection.contains(bmp))
            selection.removeOne(bmp);
        else if (!selection.contains(bmp))
            selection += bmp;
    }

    mScene->worldDocument()->setSelectedBMPs(selection);
}

void WorldBMPTool::startMoving()
{
    mMovingBMPs = mScene->worldDocument()->selectedBMPs();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingBMPs.contains(mClickedItem->bmp())) {
        mMovingBMPs.clear();
        mMovingBMPs += mClickedItem->bmp();
        mScene->worldDocument()->setSelectedBMPs(mMovingBMPs);
    }

    mMode = Moving;
}

void WorldBMPTool::updateMovingItems(const QPointF &pos,
                                     Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
#if 0
    // Restrict the drop position so the images stay in bounds
    QVector<QPoint> cellPositions;
    cellPositions.append(mClickedItem->bmp()->pos());
    cellPositions.append(mClickedItem->bmp()->bounds().bottomRight());
    QPointF pt = restrictDragging(cellPositions, mStartScenePos, pos);
#endif
    QPoint startCellPos = mScene->pixelToCellCoordsInt(mStartScenePos);
    QPoint dropCellPos = mScene->pixelToCellCoordsInt(pos);
    mDragOffset = dropCellPos - startCellPos;

    foreach (WorldBMP *bmp, mMovingBMPs) {
        mScene->itemForBMP(bmp)->setDragging(true);
        mScene->itemForBMP(bmp)->setDragOffset(mDragOffset);
    }
}

void WorldBMPTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (WorldBMP *bmp, mMovingBMPs)
        mScene->itemForBMP(bmp)->setDragging(false);

    int count = mMovingBMPs.size();
    if (count && !mDragOffset.isNull()) {
        QUndoStack *undoStack = mScene->worldDocument()->undoStack();
        if (count > 1)
            undoStack->beginMacro(tr("Move %1 BMP%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (WorldBMP *bmp, mMovingBMPs) {
            mScene->worldDocument()->moveBMP(bmp, bmp->pos() + mDragOffset);
        }
        if (count > 1)
            undoStack->endMacro();
    }

    mMovingBMPs.clear();
}

void WorldBMPTool::cancelMoving()
{
    if (mMode == Moving) {
        mMode = CancelMoving;
        foreach (WorldBMP *bmp, mMovingBMPs)
            mScene->itemForBMP(bmp)->setDragging(false);
        mMovingBMPs.clear();
        mClickedItem = 0;
    }
}

WorldBMPItem *WorldBMPTool::topmostItemAt(const QPointF &scenePos)
{
    WorldBMP *bmp = mScene->pointToBMP(scenePos);
    return bmp ? mScene->itemForBMP(bmp) : 0;
}

/////

CellObjectEdgeResizeHandle::CellObjectEdgeResizeHandle(CellScene *scene, WorldCellObject *object, Edge edge)
    : QGraphicsItem(nullptr)
    , mScene(scene)
    , mObject(object)
    , mEdge(edge)
    , mOffsetItemBG(nullptr)
    , mOffsetItem(nullptr)
{
    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
//             QGraphicsItem::ItemIgnoresTransformations |
             QGraphicsItem::ItemIgnoresParentOpacity);

    setCursor(Qt::SizeFDiagCursor);
}

void CellObjectEdgeResizeHandle::setObject(WorldCellObject *object)
{
    prepareGeometryChange();
    mObject = object;
}

void CellObjectEdgeResizeHandle::setEdge(Edge edge)
{
    prepareGeometryChange();
    mEdge = edge;
}

void CellObjectEdgeResizeHandle::synchWithObject()
{
    prepareGeometryChange();
    setPos(0.0, 0.0);
}

CellObjectEdgeResizeHandle::Edge CellObjectEdgeResizeHandle::pickEdge(ObjectItem *objectItem, const QPointF &scenePos)
{
    if (objectItem == nullptr) {
        return Edge::NONE;
    }
    Tiled::MapRenderer *renderer = objectItem->cellScene()->renderer();
    QPointF worldPos = renderer->pixelToTileCoords(scenePos);
    WorldCellObject *object = objectItem->object();
    qreal T = edgeThickness(objectItem->cellScene());
    qreal x = object->x(), y = object->y(), w = object->width(), h = object->height();
    if (worldPos.y() >= y - T && worldPos.y() <= y + T) {
        return Edge::North;
    }
    if (worldPos.y() >= y + h - T && worldPos.y() <= y + h + T) {
        return Edge::South;
    }
    if (worldPos.x() >= x - T && worldPos.x() <= x + T) {
        return Edge::West;
    }
    if (worldPos.x() >= x + w - T && worldPos.x() <= x + w + T) {
        return Edge::East;
    }
    return Edge::NONE;
}

QRectF CellObjectEdgeResizeHandle::boundingRect() const
{
    Tiled::MapRenderer *renderer = mScene->renderer();
    return renderer->tileToPixelCoords(edgeRect(), mObject->level()).boundingRect().adjusted(-20, -20, 20, 20);
}

void CellObjectEdgeResizeHandle::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPen pen(Qt::blue);
    pen.setWidth(3);
    pen.setCosmetic(true);
    painter->setPen(pen);
    Tiled::MapRenderer *renderer = mScene->renderer();
    QPolygonF scenePolygon = renderer->tileToPixelCoords(edgeRect(), mObject->level());
    painter->drawPolygon(scenePolygon);
}

void CellObjectEdgeResizeHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Remember the old size since we may resize the object
    if (event->button() == Qt::LeftButton) {
        Tiled::MapRenderer *renderer = mScene->renderer();
        mCancelResize = false;
        mClickObjectPos = renderer->pixelToTileCoords(event->scenePos(), mObject->level()) - mObject->pos();
        mOldSize = mObject->size();
        if (auto *objectItem = mScene->itemForObject(mObject)) {
            objectItem->labelItem()->setShowSize(true);
            objectItem->labelItem()->synch();
        }
    }

    QGraphicsItem::mousePressEvent(event);
}

void CellObjectEdgeResizeHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton) {
        WorldCellObject *obj = mObject;
        auto *objectItem = mScene->itemForObject(mObject);
        if (objectItem == nullptr)
            return;
        QPointF posDelta = objectItem->dragOffset();
        QSizeF resizeDelta = objectItem->resizeDelta();
        objectItem->setDragOffset(QPoint());
        objectItem->setResizeDelta(QSizeF(0, 0));
        updateOffsetLabel();
        if (mCancelResize == false) {
            if (!resizeDelta.isNull() || !posDelta.isNull()) {
                WorldDocument *document = mScene->document()->worldDocument();
                QUndoStack *undoStack = mScene->document()->undoStack();
                undoStack->beginMacro(QStringLiteral("Resize Object"));
                if (posDelta.isNull() == false)
                    document->moveCellObject(obj, obj->pos() + posDelta);
                if (resizeDelta.isNull() == false)
                    document->resizeCellObject(obj, mOldSize + resizeDelta);
                undoStack->endMacro();
            }
        }
        mCancelResize = false;
        objectItem->labelItem()->setShowSize(false);
        objectItem->labelItem()->synch();
    }

    if (event->button() == Qt::RightButton) {
        setPos(QPointF());
        mCancelResize = true;
    }

    // Stop the context-menu messing us up.
    event->accept();
}

QVariant CellObjectEdgeResizeHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    Tiled::MapRenderer *renderer = mScene->renderer();
    if (change == ItemPositionChange) {
        if (mCancelResize) {
            return QPointF();
        }
        int level = mObject->level();
        QPointF clickScenePos = renderer->tileToPixelCoords(mClickObjectPos, level);
        QPointF tileCoords = renderer->pixelToTileCoords(clickScenePos + value.toPointF(), level);
        QPointF delta = (tileCoords - mClickObjectPos).toPoint()/* - objectWorldPos*/;
        switch (mEdge) {
        case Edge::North:
            delta.setX(0);
            delta.setY(qMin(delta.y(), mObject->height() - MIN_OBJECT_SIZE));
            break;
        case Edge::South:
            delta.setX(0);
            delta.setY(qMax(delta.y(), -(mObject->height() - MIN_OBJECT_SIZE)));
            break;
        case Edge::West:
            delta.setX(qMin(delta.x(), mObject->width() - MIN_OBJECT_SIZE));
            delta.setY(0);
            break;
        case Edge::East:
            delta.setX(qMax(delta.x(), -(mObject->width() - MIN_OBJECT_SIZE)));
            delta.setY(0);
            break;
        default:
            delta = QPointF(0, 0);
            break;
        }
        return renderer->tileToPixelCoords(mClickObjectPos + delta, level) - clickScenePos;
    } else if (change == ItemPositionHasChanged) {
        if (mCancelResize)
            return QGraphicsItem::itemChange(change, value);;
        auto *objectItem = mScene->itemForObject(mObject);
        if (objectItem == nullptr)
            return QGraphicsItem::itemChange(change, value);
        int level = mObject->level();
        QPointF clickScenePos = renderer->tileToPixelCoords(mClickObjectPos, level);
        const QPointF newPos = value.toPointF() + clickScenePos;
        QPointF tileCoords = renderer->pixelToTileCoords(newPos, level);
        QPointF delta = (tileCoords - mClickObjectPos).toPoint();
        switch (mEdge) {
        case Edge::North:
            objectItem->setDragOffset(delta);
            objectItem->setResizeDelta(QSizeF(-delta.x(), -delta.y()));
            break;
        case Edge::South:
            objectItem->setResizeDelta(QSizeF(delta.x(), delta.y()));
            break;
        case Edge::West:
            objectItem->setDragOffset(delta);
            objectItem->setResizeDelta(QSizeF(-delta.x(), -delta.y()));
            break;
        case Edge::East:
            objectItem->setResizeDelta(QSizeF(delta.x(), delta.y()));
            break;
        default:
            break;
        }
        updateOffsetLabel();
    }
    return QGraphicsItem::itemChange(change, value);
}

qreal CellObjectEdgeResizeHandle::edgeThickness(CellScene *scene)
{
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    qreal T = 0.2 / qMin(zoom, 1.0);
    return T;
}

QRectF CellObjectEdgeResizeHandle::edgeRect() const
{
    qreal T = edgeThickness(mScene);
    int x = mObject->x(), y = mObject->y(), w = mObject->width(), h = mObject->height();
    switch (mEdge) {
    case Edge::NONE:
        return QRectF(x, y, T, T);
    case Edge::North:
        return QRectF(x, y, w, T);
    case Edge::East:
        return QRectF(x + w - T, y, T, h);
    case Edge::South:
        return QRectF(x, y + h - T, w, T);
    case Edge::West:
        return QRectF(x, y, T, h);
    }
}

void CellObjectEdgeResizeHandle::updateOffsetLabel()
{
    QSizeF offset;
    if (auto *objectItem = mScene->itemForObject(mObject)) {
        offset = objectItem->resizeDelta();
    }
    if (offset.isNull()) {
        if (mOffsetItem) {
            delete mOffsetItemBG;
            mOffsetItem = nullptr;
            mOffsetItemBG = nullptr;
        }
        return;
    }
    if (mOffsetItem == nullptr) {
        mOffsetItemBG = new QGraphicsRectItem(this);
        mOffsetItemBG->setBrush(Qt::lightGray);
        mOffsetItemBG->setPos(0.0, 0.0);
        mOffsetItem = new QGraphicsSimpleTextItem(mOffsetItemBG);
        mOffsetItem->setPos(4.0, 4.0);
        mOffsetItemBG->setFlags(QGraphicsItem::ItemIgnoresTransformations);
    }
    mOffsetItem->setText(QStringLiteral("offset %1").arg((mEdge == Edge::North || mEdge == Edge::South) ? offset.height() : offset.width()));
    mOffsetItemBG->setRect(QRectF(QPointF(), mOffsetItem->boundingRect().size() + QSizeF(8.0, 8.0)));
    mOffsetItemBG->setPos(mScene->renderer()->tileToPixelCoords(mObject->pos() + mClickObjectPos, mObject->level()));
}
