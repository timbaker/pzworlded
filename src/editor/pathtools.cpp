/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "pathtools.h"

#include "basepathrenderer.h"
#include "basepathscene.h"
#include "path.h"
#include "pathdocument.h"
#include "pathundoredo.h"
#include "pathworld.h"
#include "preferences.h"

#include <QApplication>
#include <QKeyEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QUndoStack>

BasePathTool::BasePathTool(const QString &name, const QIcon &icon,
                           const QKeySequence &shortcut, QObject *parent) :
    AbstractTool(name, icon, shortcut, PathToolType, parent),
    mScene(0)
{
}

BasePathTool::~BasePathTool()
{
}

void BasePathTool::setScene(BaseGraphicsScene *scene)
{
    mScene = scene ? scene->asPathScene() : 0;
}

PathDocument *BasePathTool::document()
{
    return mScene ? mScene->document() : 0;
}

void BasePathTool::beginUndoMacro(const QString &s)
{
    document()->undoStack()->beginMacro(s);
}

void BasePathTool::endUndoMacro()
{
    document()->undoStack()->endMacro();
}

void BasePathTool::updateEnabledState()
{
    setEnabled(mScene != 0 && mScene->isPathScene());
}

/////

SelectMoveNodeTool *SelectMoveNodeTool::mInstance = 0;

SelectMoveNodeTool *SelectMoveNodeTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveNodeTool();
    return mInstance;
}

void SelectMoveNodeTool::deleteInstance()
{
    delete mInstance;
}

SelectMoveNodeTool::SelectMoveNodeTool()
    : BasePathTool(tr("Select and Move Nodes"),
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

SelectMoveNodeTool::~SelectMoveNodeTool()
{
    delete mSelectionRectItem;
}

void SelectMoveNodeTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->document()->disconnect(this);

    BasePathTool::setScene(scene);

    if (mScene) {
        connect(mScene->document(), SIGNAL(nodeAboutToBeRemoved(int)),
                SLOT(nodeAboutToBeRemoved(int)));
    }
}

void SelectMoveNodeTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;
        foreach (WorldNode *node, mMovingNodes)
            mScene->nodeItem()->setDragging(node, false);
        mMovingNodes.clear();
        event->accept();
    }
}

void SelectMoveNodeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mStartScreenPos = event->screenPos();
        mDropWorldPos = mScene->renderer()->toWorld(mStartScenePos);
        mClickedNode = topmostNodeAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            foreach (WorldNode *node, mMovingNodes)
                mScene->nodeItem()->setDragging(node, false);
            mMovingNodes.clear();
        }
        break;
    default:
        break;
    }
}

void SelectMoveNodeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScreenPos - event->screenPos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedNode &&
                    mScene->nodeItem()->selectedNodes().contains(mClickedNode) &&
                    !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting: {
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

void SelectMoveNodeTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode: {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        NodeSet newSelection;
        if (extend || toggle)
            newSelection = mScene->nodeItem()->selectedNodes();
        if (mClickedNode) {
            if (toggle && newSelection.contains(mClickedNode))
                newSelection.remove(mClickedNode);
            else if (!newSelection.contains(mClickedNode))
                newSelection += mClickedNode;
        }
        mScene->nodeItem()->setSelectedNodes(newSelection);
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
    mClickedNode = 0;
}

void SelectMoveNodeTool::nodeAboutToBeRemoved(WorldNode *node)
{
    if (mMode == Moving) {
        if (mMovingNodes.contains(node)) {
            mScene->nodeItem()->setDragging(node, false);
            mMovingNodes.remove(node);
            if (mMovingNodes.isEmpty())
                mMode = CancelMoving;
        }
    }
}

void SelectMoveNodeTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void SelectMoveNodeTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mStartScenePos;
    QPointF end = event->scenePos();
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    NodeSet selection;
    if (extend || toggle)
        selection = mScene->nodeItem()->selectedNodes();

    foreach (WorldNode *node, mScene->nodeItem()->lookupNodes(bounds)) {
        if (toggle && selection.contains(node))
            selection.remove(node);
        else if (!selection.contains(node))
            selection += node;
    }

    mScene->nodeItem()->setSelectedNodes(selection);
}

void SelectMoveNodeTool::startMoving()
{
    mMovingNodes = mScene->nodeItem()->selectedNodes();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingNodes.contains(mClickedNode)) {
        mMovingNodes.clear();
        mMovingNodes += mClickedNode;
        mScene->nodeItem()->setSelectedNodes(mMovingNodes);
    }

    mMode = Moving;
}

void SelectMoveNodeTool::updateMovingItems(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos);
    mDropWorldPos = mScene->renderer()->toWorld(pos);

#if 0
    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (modifiers & Qt::ControlModifier)
        snapToGrid = !snapToGrid;
    if (snapToGrid)
        mDropWorldPos = mDropWorldPos.toPoint();
#endif

    foreach (WorldNode *node, mMovingNodes) {
        mScene->nodeItem()->setDragging(node, true);
        mScene->nodeItem()->setDragOffset(node, mDropWorldPos - startPos);
    }
}

void SelectMoveNodeTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (WorldNode *node, mMovingNodes)
        mScene->nodeItem()->setDragging(node, false);

    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos);
    QPointF dropPos = mDropWorldPos;
    QPointF diff = dropPos - startPos;
    if (startPos != dropPos) {
        int count = mMovingNodes.size();
        beginUndoMacro(tr("Move %1 Node%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (WorldNode *node, mMovingNodes) {
            foreach (WorldPathLayer *wpl, document()->world()->pathLayers()) {
                if (WorldNode *origNode = wpl->node(node->id))
                    PathUndoRedo::MoveNode::push(document(), origNode, node->pos() + diff);
            }

        }
        endUndoMacro();
    }

    mMovingNodes.clear();
}

WorldNode *SelectMoveNodeTool::topmostNodeAt(const QPointF &scenePos)
{
    return mScene->nodeItem()->topmostNodeAt(scenePos);
}

/////

SelectMovePathTool *SelectMovePathTool::mInstance = 0;

SelectMovePathTool *SelectMovePathTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMovePathTool();
    return mInstance;
}

void SelectMovePathTool::deleteInstance()
{
    delete mInstance;
}

SelectMovePathTool::SelectMovePathTool()
    : BasePathTool(tr("Select and Move Paths"),
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

SelectMovePathTool::~SelectMovePathTool()
{
    delete mSelectionRectItem;
}

void SelectMovePathTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->document()->disconnect(this);

    BasePathTool::setScene(scene);

    if (mScene) {
        connect(mScene->document(), SIGNAL(nodeAboutToBeRemoved(int)),
                SLOT(nodeAboutToBeRemoved(int)));
    }
}

void SelectMovePathTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;
        foreach (WorldNode *node, movingNodes())
            mScene->nodeItem()->setDragging(node, false);
        mMovingPaths.clear();
        event->accept();
    }
}

void SelectMovePathTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            break;
        mMousePressed = true;
        mStartScenePos = event->scenePos();
        mStartScreenPos = event->screenPos();
        mDropWorldPos = mScene->renderer()->toWorld(mStartScenePos);
        mClickedPath = topmostPathAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            foreach (WorldNode *node, movingNodes())
                mScene->nodeItem()->setDragging(node, false);
            mMovingPaths.clear();
        }
        break;
    default:
        break;
    }
}

void SelectMovePathTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScreenPos - event->screenPos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedPath &&
                    mScene->selectedPaths().contains(mClickedPath) &&
                    !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting: {
        QPointF start = mStartScenePos;
        QPointF end = event->scenePos();
        QRectF bounds = QRectF(start, end).normalized();
        mSelectionRectItem->setRect(bounds);
        break;
    }
    case Moving:
        updateMovingItems(event->scenePos(), event->modifiers());
        break;
    case NoMode: {
        WorldPath *closest = topmostPathAt(event->scenePos());
        if (closest != mScene->mHighlightPath) {
            mScene->mHighlightPath = closest;
            mScene->update();
        }
        break;
    }
    case CancelMoving:
        break;
    }
}

void SelectMovePathTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode: {
        bool toggle = event->modifiers() & Qt::ControlModifier;
        bool extend = event->modifiers() & Qt::ShiftModifier;
        PathSet newSelection;
        if (extend || toggle)
            newSelection = mScene->selectedPaths();
        if (mClickedPath) {
            if (toggle && newSelection.contains(mClickedPath))
                newSelection.remove(mClickedPath);
            else if (!newSelection.contains(mClickedPath))
                newSelection += mClickedPath;
        }
        mScene->setSelectedPaths(newSelection);
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
    mClickedPath = 0;
}

void SelectMovePathTool::pathAboutToBeRemoved(WorldPath *path)
{
    if (mMode == Moving) {
        if (mMovingPaths.contains(path)) {
            NodeSet moving = movingNodes();
            mMovingPaths.remove(path);
            if (mMovingPaths.isEmpty())
                mMode = CancelMoving;
            foreach (WorldNode *node, moving - movingNodes())
                mScene->nodeItem()->setDragging(node, false);
        }
    }
}

void SelectMovePathTool::startSelecting()
{
    mMode = Selecting;
    mScene->addItem(mSelectionRectItem);
}

void SelectMovePathTool::updateSelection(QGraphicsSceneMouseEvent *event)
{
    QPointF start = mStartScenePos;
    QPointF end = event->scenePos();
    QRectF bounds = QRectF(start, end).normalized();

    bool toggle = event->modifiers() & Qt::ControlModifier;
    bool extend = event->modifiers() & Qt::ShiftModifier;

    PathSet selection;
    if (extend || toggle)
        selection = mScene->selectedPaths();

    foreach (WorldPath *path, mScene->lookupPaths(bounds)) {
        if (toggle && selection.contains(path))
            selection.remove(path);
        else if (!selection.contains(path))
            selection += path;
    }

    mScene->setSelectedPaths(selection);
}

void SelectMovePathTool::startMoving()
{
    mMovingPaths = mScene->selectedPaths();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingPaths.contains(mClickedPath)) {
        mMovingPaths.clear();
        mMovingPaths += mClickedPath;
        mScene->nodeItem()->setSelectedNodes(movingNodes());
    }

    mMode = Moving;
}

void SelectMovePathTool::updateMovingItems(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos);
    mDropWorldPos = mScene->renderer()->toWorld(pos);

#if 0
    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (modifiers & Qt::ControlModifier)
        snapToGrid = !snapToGrid;
    if (snapToGrid)
        mDropWorldPos = mDropWorldPos.toPoint();
#endif

    foreach (WorldNode *node, movingNodes()) {
        mScene->nodeItem()->setDragging(node, true);
        mScene->nodeItem()->setDragOffset(node, mDropWorldPos - startPos);
    }
}

void SelectMovePathTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (WorldNode *node, movingNodes())
        mScene->nodeItem()->setDragging(node, false);

    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos);
    QPointF dropPos = mDropWorldPos;
    QPointF diff = dropPos - startPos;
    if (startPos != dropPos) {
        int count = movingNodes().size();
        beginUndoMacro(tr("Move %1 Node%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (WorldNode *node, movingNodes()) {
            foreach (WorldPathLayer *wpl, document()->world()->pathLayers()) {
                if (WorldNode *origNode = wpl->node(node->id))
                    PathUndoRedo::MoveNode::push(document(), origNode, node->pos() + diff);
            }

        }
        endUndoMacro();
    }

    mMovingPaths.clear();
}

WorldPath *SelectMovePathTool::topmostPathAt(const QPointF &scenePos)
{
    return mScene->topmostPathAt(scenePos);
}

NodeSet SelectMovePathTool::movingNodes()
{
    NodeSet nodes;
    foreach (WorldPath *path, mMovingPaths)
        nodes += path->nodes.toSet();
    return nodes;
}
