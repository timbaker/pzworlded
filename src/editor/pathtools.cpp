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
#include "worldchanger.h"

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

int BasePathTool::level()
{
    return (mScene && mScene->currentPathLayer()) ? mScene->currentPathLayer()->level() : 0;
}

#include <QVector2D>
static qreal pos_on_line(QVector2D v, QVector2D w, QVector2D p) {
    const qreal l2 = (v - w).lengthSquared();  // i.e. |w-v|^2 -  avoid a sqrt
    if (l2 == 0.0) return 0.0;   // v == w case
    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line.
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2
    return QVector2D::dotProduct(p - v, w - v) / l2;
}

WorldPath *BasePathTool::pointOnSegment(const QPointF &scenePos, QPointF &ptOnLine)
{
    int nodeIndex;
    if (WorldPath *path = mScene->topmostPathAt(scenePos, &nodeIndex)) {
        QVector2D p0(mScene->renderer()->toScene(path->nodePos(nodeIndex), level()));
        QVector2D p1(mScene->renderer()->toScene(path->nodePos((nodeIndex == path->nodeCount() - 1) ? 0 : nodeIndex + 1), level()));
        // Put the node exactly on the line segment
        ptOnLine = QLineF(p0.toPointF(), p1.toPointF()).pointAt(
                    pos_on_line(p0, p1, QVector2D(scenePos)));
        return path;
    }
    return 0;
}

QPointF BasePathTool::nextNodePos(QGraphicsSceneMouseEvent *event)
{
    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (event->modifiers() & Qt::ControlModifier)
        snapToGrid = !snapToGrid;

    QPointF pos = event->scenePos();
    if (snapToGrid)
        pos = mScene->renderer()->toScene(mScene->renderer()->toWorld(pos, level()).toPoint(), level());

    bool snapToSegment = true;
    if (snapToSegment)
        pointOnSegment(event->scenePos(), pos);

    bool snapToNode = true;
    if (snapToNode) {
        if (WorldNode *node = mScene->nodeItem()->topmostNodeAt(event->scenePos()))
            pos = mScene->renderer()->toScene(node->pos(), level());
    }

    return pos;
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
        connect(mScene->document()->changer(), SIGNAL(beforeRemoveNodeFromPathSignal(WorldPath*,int,WorldNode*)),
                SLOT(beforeRemoveNode(WorldPath*,int,WorldNode*)));
    }
}

void SelectMoveNodeTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;
        mScene->changer()->undoAndForget();
#if 0
        foreach (WorldNode *node, mMovingNodes)
            mScene->nodeItem()->setDragging(node, false);
#endif
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
        mDropWorldPos = mScene->renderer()->toWorld(mStartScenePos, level());
        mClickedNode = topmostNodeAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            mScene->changer()->undoAndForget();
            mMovingNodes.clear();
        }
        break;
    default:
        break;
    }
}

void SelectMoveNodeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScene->nodeItem()->trackMouse(event);

    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartScreenPos - event->screenPos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedNode /*&&
                    mScene->nodeItem()->selectedNodes().contains(mClickedNode)*/ &&
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
        updateMovingItems(event);
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

void SelectMoveNodeTool::beforeRemoveNode(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(path)
    Q_UNUSED(index)
    if (mMode == Moving) {
        if (mMovingNodes.contains(node)) {
            mMovingNodes.remove(node);
            if (mMovingNodes.isEmpty()) {
                mScene->changer()->undoAndForget();
                mMode = CancelMoving;
            }
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

void SelectMoveNodeTool::updateMovingItems(QGraphicsSceneMouseEvent *event)
{
    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos, level());
    foreach (WorldNode *node, mMovingNodes) node->path()->setVisible(false);
    QPointF pos = nextNodePos(event);
    foreach (WorldNode *node, mMovingNodes) node->path()->setVisible(true);
    mDropWorldPos = mScene->renderer()->toWorld(pos, level());

    mScene->changer()->undoAndForget();
    foreach (WorldNode *node, mMovingNodes) {
        mScene->changer()->doMoveNode(node, /*snapToGrid ? (node->pos() + mDropWorldPos - startPos).toPoint() :*/ node->pos() + mDropWorldPos - startPos);
    }
}

void SelectMoveNodeTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    if (int count = mScene->changer()->changeCount()) {
        beginUndoMacro(tr("Move %1 Node%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (WorldChange *c, mScene->changer()->takeChanges()) {
            c->setChanger(document()->changer());
            document()->undoStack()->push(new WorldChangeUndoCommand(c));
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

AddRemoveNodeTool *AddRemoveNodeTool::mInstance = 0;

AddRemoveNodeTool *AddRemoveNodeTool::instance()
{
    if (!mInstance)
        mInstance = new AddRemoveNodeTool();
    return mInstance;
}

void AddRemoveNodeTool::deleteInstance()
{
    delete mInstance;
}

AddRemoveNodeTool::AddRemoveNodeTool()
    : BasePathTool(tr("Add and Remove Nodes"),
                   QIcon(QLatin1String(":/images/22x22/road-tool-select.png")),
                   QKeySequence())
{
}

AddRemoveNodeTool::~AddRemoveNodeTool()
{
}

void AddRemoveNodeTool::updateFakeNode(const QPointF &scenePos)
{
    int nodeIndex;
    if (WorldPath *path = mScene->topmostPathAt(scenePos, &nodeIndex)) {
        QVector2D p0(mScene->renderer()->toScene(path->nodePos(nodeIndex), level()));
        QVector2D p1(mScene->renderer()->toScene(path->nodePos((nodeIndex == path->nodeCount() - 1) ? 0 : nodeIndex + 1), level()));
        // Put the node exactly on the line segment
        QPointF ptOnLine = QLineF(p0.toPointF(), p1.toPointF()).pointAt(
                    pos_on_line(p0, p1, QVector2D(scenePos)));
        QPointF worldPos = mScene->renderer()->toWorld(ptOnLine, level());
        mScene->nodeItem()->setFakeNodePos(worldPos);
    } else
        mScene->nodeItem()->setFakeNodePos(QPointF());
}

void AddRemoveNodeTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    int nodeIndex;
    if (WorldNode *node = mScene->nodeItem()->topmostNodeAt(event->scenePos())) {
        document()->changer()->beginUndoCommand(document()->undoStack());
        document()->changer()->doRemoveNodeFromPath(node->path(), node->index(), node);
        document()->changer()->endUndoCommand();
    } else if (WorldPath *path = mScene->topmostPathAt(event->scenePos(), &nodeIndex)) {
        QVector2D p0(mScene->renderer()->toScene(path->nodePos(nodeIndex), level()));
        QVector2D p1(mScene->renderer()->toScene(path->nodePos((nodeIndex == path->nodeCount() - 1) ? 0 : nodeIndex + 1), level()));
        // Put the node exactly on the line segment
        QPointF scenePos = QLineF(p0.toPointF(), p1.toPointF()).pointAt(pos_on_line(p0, p1, QVector2D(event->scenePos())));
        QPointF worldPos = mScene->renderer()->toWorld(scenePos, level());
        WorldNode *node = document()->world()->allocNode(worldPos);
        document()->changer()->beginUndoCommand(document()->undoStack());
        document()->changer()->doAddNodeToPath(path, nodeIndex + 1, node);
        document()->changer()->endUndoCommand();
    }
}

void AddRemoveNodeTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScene->nodeItem()->trackMouse(event);

    int nodeIndex;
    if (WorldPath *path = mScene->topmostPathAt(event->scenePos(), &nodeIndex)) {
        mScene->setHighlightSegment(path, nodeIndex);
    } else
        mScene->setHighlightSegment(0, -1);
    updateFakeNode(event->scenePos());
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
        connect(mScene->document()->changer(), SIGNAL(beforeRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
                SLOT(beforeRemovePath(WorldPathLayer*,int,WorldPath*)));
    }
}

void SelectMovePathTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && (mMode == Moving)) {
        mMode = CancelMoving;
        mScene->changer()->undoAndForget();
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
        mDropWorldPos = mScene->renderer()->toWorld(mStartScenePos, level());
        mClickedPath = topmostPathAt(mStartScenePos);
        break;
    case Qt::RightButton:
        // Right-clicks exits moving, same as the Escape key.
        if (mMode == Moving) {
            mMode = CancelMoving;
            mScene->changer()->undoAndForget();
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

void SelectMovePathTool::beforeRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    if (mMode == Moving) {
        if (mMovingPaths.contains(path)) {
            mMovingPaths.remove(path);
            if (mMovingPaths.isEmpty()) {
                mScene->changer()->undoAndForget();
                mMode = CancelMoving;
            }
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

    QPointF startPos = mScene->renderer()->toWorld(mStartScenePos, level());
    mDropWorldPos = mScene->renderer()->toWorld(pos, level());

#if 0
    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (modifiers & Qt::ControlModifier)
        snapToGrid = !snapToGrid;
//    if (snapToGrid)
//        mDropWorldPos = mDropWorldPos.toPoint();
#endif
    mScene->changer()->undoAndForget();
    foreach (WorldNode *node, movingNodes()) {
        mScene->changer()->doMoveNode(node, node->pos() + mDropWorldPos - startPos);
    }
}

void SelectMovePathTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)
    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    if (int count = mScene->changer()->changeCount()) {
        beginUndoMacro(tr("Move %1 Node%2").arg(count).arg(QLatin1String((count > 1) ? "s" : "")));
        foreach (WorldChange *c, mScene->changer()->takeChanges()) {
            c->setChanger(document()->changer());
            document()->undoStack()->push(new WorldChangeUndoCommand(c));
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
        nodes += path->nodes().toSet();
    return nodes;
}

/////

CreatePathTool *CreatePathTool::mInstance = 0;

CreatePathTool *CreatePathTool::instance()
{
    if (!mInstance)
        mInstance = new CreatePathTool();
    return mInstance;
}

void CreatePathTool::deleteInstance()
{
    delete mInstance;
}

CreatePathTool::CreatePathTool() :
    BasePathTool(tr("Create Path"),
                 QIcon(QLatin1String(":/images/22x22/road-tool-select.png")),
                 QKeySequence()),
#if 1
    mNewPath(0),
    mSegmentItem(new QGraphicsLineItem)
#else
    mPolyItem(new QGraphicsPathItem),
    mFirstNodeItem(new QGraphicsEllipseItem)
#endif
{
#if 1
    mSegmentItem->setZValue(10000);
#else
    mPolyItem->setPen(QPen(QColor(0,255,0), 2));
    mFirstNodeItem->setPen(QPen(QColor(0,255,0), 2));
#endif
}

CreatePathTool::~CreatePathTool()
{
}

void CreatePathTool::activate()
{
}

void CreatePathTool::deactivate()
{
    clearNewPath();
}

void CreatePathTool::setScene(BaseGraphicsScene *scene)
{
    BasePathTool::setScene(scene);
}

void CreatePathTool::keyPressEvent(QKeyEvent *event)
{
#if 0
    if ((event->key() == Qt::Key_Escape) && mPoly.size()) {
        clearNewPath();
        event->accept();
    }
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && mPoly.size()) {
        finishNewPath(false);
        event->accept();
    }
#else
    if ((event->key() == Qt::Key_Escape) && mNewPath) {
        mScene->changer()->undoAndForget();
        // FIXME: delete path and nodes
        mNewPath = 0;
        event->accept();
    }
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && mNewPath) {
        finishNewPath(false);
        event->accept();
    }
#endif
}

void CreatePathTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {

        WorldNode *node = mScene->nodeItem()->topmostNodeAt(event->scenePos());
#if 0
        if (mPoly.size() > 1 && QLineF(mPoly.first(), event->scenePos()).length() < mScene->nodeItem()->nodeRadius()) {
            finishNewPath(true);
            return;
        }
        mNodes += node;
        if (node) {
            mPoly += mScene->renderer()->toScene(node->pos());
        } else {
            mPoly += event->scenePos();
        }
        QPainterPath path;
        path.moveTo(mPoly.first());
        foreach (QPointF p, mPoly.mid(1))
            path.lineTo(p);
        mPolyItem->setPath(path);
        if (mPoly.size() == 1) {
            if (!node) {
                mFirstNodeItem->setRect(mPoly.first().x() - mScene->nodeItem()->nodeRadius(),
                                        mPoly.first().y() - mScene->nodeItem()->nodeRadius(),
                                        mScene->nodeItem()->nodeRadius() * 2,
                                        mScene->nodeItem()->nodeRadius() * 2);
            }
            mScene->addItem(mFirstNodeItem);
            mScene->addItem(mPolyItem);
        }
#else
        if (!mNewPath) {
            Q_ASSERT(mScene->changer()->changeCount() == 0);
            mNewPath = mScene->world()->allocPath();
            mScene->changer()->doAddPath(mScene->currentPathLayer(),
                                         mScene->currentPathLayer()->pathCount(),
                                         mNewPath);
            mSegmentItem->setLine(QLineF());
            mScene->addItem(mSegmentItem);
        }

        if (node == mNewPath->first() && mNewPath->nodeCount() > 1) {
            finishNewPath(true);
        } else {
            QPointF pos = nextNodePos(event);
            WorldNode *node = mScene->world()->allocNode(
                        mScene->renderer()->toWorld(pos, level()));
            mScene->changer()->doAddNodeToPath(mNewPath, mNewPath->nodeCount(), node);
        }
#endif
    }
    if (event->button() == Qt::RightButton) {
        clearNewPath();
    }
}

void CreatePathTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScene->nodeItem()->trackMouse(event);

#if 1
    QPointF pos = nextNodePos(event);
    if (mNewPath)
        mSegmentItem->setLine(QLineF(mScene->renderer()->toScene(
                                         mNewPath->last()->pos(), level()), pos));
    mScene->nodeItem()->setFakeNodePos(mScene->renderer()->toWorld(pos, level()));
#else
    if (mPoly.size()) {
        QPainterPath path;
        path.moveTo(mPoly.first());
        foreach (QPointF p, mPoly.mid(1))
            path.lineTo(p);
        path.lineTo(event->scenePos());
        mPolyItem->setPath(path);
    }
#endif
}

void CreatePathTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}

void CreatePathTool::finishNewPath(bool close)
{
#if 0
    if (mPoly.size()) {
        document()->changer()->beginUndoMacro(document()->undoStack(), tr("Create Path"));
        WorldPath *path = mScene->world()->allocPath();
        document()->changer()->doAddPath(mScene->currentPathLayer(),
                                         mScene->currentPathLayer()->pathCount(),
                                         path);
        for (int i = 0; i < mPoly.size(); i++) {
            if (mNodes[i])
                document()->changer()->doAddNodeToPath(path, path->nodeCount(), mNodes[i]);
            else {
                WorldNode *node = mScene->world()->allocNode(mScene->renderer()->toWorld(mPoly[i]));
                document()->changer()->doAddNode(mScene->currentPathLayer(),
                                             mScene->currentPathLayer()->nodeCount(),
                                             node);
                document()->changer()->doAddNodeToPath(path, path->nodeCount(), node);
            }
        }
        if (close)
            document()->changer()->doAddNodeToPath(path, path->nodeCount(), path->first());
        document()->changer()->endUndoMacro();
        clearNewPath();
    }
#else
    if (mNewPath) {
        mNewPath->setClosed(close);
        beginUndoMacro(tr("Create Path"));
        foreach (WorldChange *c, mScene->changer()->takeChanges()) {
            c->setChanger(document()->changer());
            document()->undoStack()->push(new WorldChangeUndoCommand(c));
        }
        endUndoMacro();
        mScene->setSelectedPaths(PathSet() << mNewPath);
        clearNewPath();
    }
#endif
}

void CreatePathTool::clearNewPath()
{
#if 1
    if (mNewPath) {
        mScene->changer()->undoAndForget();
        mScene->removeItem(mSegmentItem);
        mNewPath = 0;
    }
    mScene->nodeItem()->setFakeNodePos(QPointF());
#else
    if (mPoly.size()) {
        mPoly.clear();
        mNodes.clear();
        mScene->removeItem(mFirstNodeItem);
        mScene->removeItem(mPolyItem);
    }
#endif
}

/////

AddPathSegmentsTool *AddPathSegmentsTool::mInstance = 0;

AddPathSegmentsTool *AddPathSegmentsTool::instance()
{
    if (!mInstance)
        mInstance = new AddPathSegmentsTool();
    return mInstance;
}

void AddPathSegmentsTool::deleteInstance()
{
    delete mInstance;
}

AddPathSegmentsTool::AddPathSegmentsTool() :
    BasePathTool(tr("Add Sements To Path"),
                 QIcon(QLatin1String(":/images/22x22/road-tool-select.png")),
                 QKeySequence()),
    mPath(0),
    mSegmentItem(new QGraphicsLineItem)
{
}

AddPathSegmentsTool::~AddPathSegmentsTool()
{
}

void AddPathSegmentsTool::activate()
{
}

void AddPathSegmentsTool::deactivate()
{
    clearNewPath();
}

void AddPathSegmentsTool::setScene(BaseGraphicsScene *scene)
{
    BasePathTool::setScene(scene);
}

void AddPathSegmentsTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mPath) {
        mScene->changer()->undoAndForget();
        // FIXME: delete path and nodes
        mPath = 0;
        event->accept();
    }
    if ((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) && mPath) {
        finishNewPath(false);
        event->accept();
    }
}

void AddPathSegmentsTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        WorldNode *node = mScene->nodeItem()->topmostNodeAt(event->scenePos());
        WorldPath *current = currentPath();
        if (!mPath && !current) {
            if (node)
                current = node->path();
            else
                return;
        }
        if (!mPath) {
            if (current->isClosed())
                return;
            if (node != current->first() && node != current->last())
                return;
            mPath = current;
            mAppend = node == mPath->last();
            mSegmentItem->setLine(QLineF());
            mScene->addItem(mSegmentItem);
            Q_ASSERT(mScene->changer()->changeCount() == 0);
            return;
        }
        if (node == (mAppend ? mPath->first() : mPath->last()) && mPath->nodeCount() > 1) {
            finishNewPath(true);
        } else {
            QPointF pos = nextNodePos(event);
            WorldNode *node = mScene->world()->allocNode(mScene->renderer()->toWorld(
                                                             pos, level()));
            mScene->changer()->doAddNodeToPath(mPath, mAppend ? mPath->nodeCount() : 0, node);
        }
    }
    if (event->button() == Qt::RightButton) {
        clearNewPath();
    }
}

void AddPathSegmentsTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScene->nodeItem()->trackMouse(event);

    if (mPath) {
        WorldNode *node = mAppend ? mPath->last() : mPath->first();
        QPointF pos = nextNodePos(event);
        mSegmentItem->setLine(QLineF(mScene->renderer()->toScene(node->pos(), level()),
                                     pos));
    }
}

void AddPathSegmentsTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}

WorldPath *AddPathSegmentsTool::currentPath()
{
    if (mScene->selectedPaths().size() == 1)
        return mScene->selectedPaths().values().first();
    return 0;
}

void AddPathSegmentsTool::finishNewPath(bool close)
{
    if (mPath) {
        beginUndoMacro(tr("Add Segments To Path"));
        if (close != mPath->isClosed())
            mScene->changer()->doSetPathClosed(mPath, close);
        foreach (WorldChange *c, mScene->changer()->takeChanges()) {
            c->setChanger(document()->changer());
            document()->undoStack()->push(new WorldChangeUndoCommand(c));
        }
        endUndoMacro();
        clearNewPath();
    }
}

void AddPathSegmentsTool::clearNewPath()
{
    if (mPath) {
        mScene->changer()->undoAndForget();
        mScene->removeItem(mSegmentItem);
        mPath = 0;
    }
}
