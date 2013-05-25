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

#include "basepathscene.h"

#include "basepathrenderer.h"
#include "global.h"
#include "path.h"
#include "pathdocument.h"
#include "pathtools.h"
#include "pathview.h"
#include "pathworld.h"
#include "worldchanger.h"
#include "worldlookup.h"
#include "zoomable.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

/////

BasePathScene::BasePathScene(PathDocument *doc, QObject *parent) :
    BaseGraphicsScene(PathSceneType, parent),
    mDocument(doc),
    mNodeItem(new NodesItem(this)),
    mActiveTool(0),
    mChanger(new WorldChanger(doc->world()))
{
}

BasePathScene::~BasePathScene()
{
    delete mRenderer;
}

void BasePathScene::setDocument(PathDocument *doc)
{
    mDocument = doc;

#if 0

    foreach (WorldScript *ws, world()->scripts()) {
        QRect r = ws->mRegion.boundingRect().translated(-cell()->pos() * 300);
        if (!r.intersects(QRect(QPoint(), QSize(300,300)))) continue;
        Lua::LuaScript ls(world(), ws);
        ls.runFunction("run");
    }
#endif

    int z = 1;
    foreach (WorldLevel *wlevel, world()->levels()) {
        WorldLevelItem *item = new WorldLevelItem(wlevel, this);
        item->setZValue(z++);
        mLevelItems += item;
        addItem(item);
    }

    mNodeItem->setZValue(z++);
    addItem(mNodeItem);

    setSceneRect(mRenderer->toScene(world()->bounds(), 16).boundingRect()
                 | mRenderer->toScene(world()->bounds(), 0).boundingRect());

    connect(mDocument->changer(), SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(nodeMoved(WorldNode*)));
    connect(mDocument->changer(), SIGNAL(afterAddNodeSignal(WorldNode*)),
            SLOT(update()));
    connect(mDocument->changer(), SIGNAL(afterAddNodeToPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(update()));
    connect(mDocument->changer(), SIGNAL(afterSetPathVisibleSignal(WorldPath*,bool)),
            SLOT(update()));
//    connect(mDocument->shadow(), SIGNAL(nodesMoved(QRectF)), SLOT(update())); // FIXME: only repaint what's needed
    connect(mDocument, SIGNAL(currentPathLayerChanged(WorldPathLayer*)),
            SLOT(currentPathLayerChanged(WorldPathLayer*)));

    connect(mDocument->changer(), SIGNAL(afterAddPathLayerSignal(WorldLevel*,int,WorldPathLayer*)),
            SLOT(afterAddPathLayer(WorldLevel*,int,WorldPathLayer*)));
    connect(mDocument->changer(), SIGNAL(beforeRemovePathLayerSignal(WorldLevel*,int,WorldPathLayer*)),
            SLOT(beforeRemovePathLayer(WorldLevel*,int,WorldPathLayer*)));
    connect(mDocument->changer(), SIGNAL(afterReorderPathLayerSignal(WorldLevel*,WorldPathLayer*,int)),
            SLOT(afterReorderPathLayer(WorldLevel*,WorldPathLayer*,int)));
    connect(mDocument->changer(), SIGNAL(afterSetPathLayerVisibleSignal(WorldPathLayer*,bool)),
            SLOT(afterSetPathLayerVisible(WorldPathLayer*,bool)));

    connect(mChanger, SIGNAL(afterAddNodeSignal(WorldNode*)),
            SLOT(afterAddNode(WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeSignal(WorldPathLayer*,int,WorldNode*)),
            SLOT(afterRemoveNode(WorldPathLayer*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterAddPathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterAddPath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterRemovePath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterAddNodeToPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterAddNodeToPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeFromPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterRemoveNodeFromPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(nodeMoved(WorldNode*)));
}

PathWorld *BasePathScene::world() const
{
    return mDocument ? mDocument->world() : 0;
}

WorldLookup *BasePathScene::lookup() const
{
    return currentPathLayer() ? currentPathLayer()->lookup() : 0;
}

void BasePathScene::setRenderer(BasePathRenderer *renderer)
{
    mRenderer = renderer;
}

void BasePathScene::setTool(AbstractTool *tool)
{
    BasePathTool *pathTool = tool ? tool->asPathTool() : 0;

    if (mActiveTool == pathTool)
        return;

    if (mActiveTool) {
        mActiveTool->deactivate();
    }

    mActiveTool = pathTool;

    if (mActiveTool) {
        mActiveTool->activate();
    }
}

void BasePathScene::keyPressEvent(QKeyEvent *event)
{
    if (mActiveTool) {
        mActiveTool->keyPressEvent(event);
        if (event->isAccepted())
            return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void BasePathScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mouseMoveEvent(event);
}

void BasePathScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mousePressEvent(event);
}

void BasePathScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mouseReleaseEvent(event);
}

WorldPathLayer *BasePathScene::currentPathLayer() const
{
    return document()->currentPathLayer();
}

void BasePathScene::setSelectedPaths(const PathSet &selection)
{
    mSelectedPaths = selection;
    update(); // FIXME: redraw only as needed
}

bool PolylineIntersectsPolygon(const QPolygonF &polyLine, const QPolygonF &polyTest)
{
    // Slowest algo evar
    for (int i = 0; i < polyTest.size() - 1; i++) {
        QLineF line1(polyTest[i], polyTest[i+1]);
        for (int j = 0; j < polyLine.size() - 1; j++) {
            QLineF line2(polyLine[j], polyLine[j+1]);
            if (line1.intersect(line2, 0) == QLineF::BoundedIntersection)
                return true;
        }
    }
    return false;
}

bool PolygonContainsPolyline(const QPolygonF &polygon, const QPolygonF &polyLine)
{
    foreach (QPointF p, polyLine)
        if (!polygon.containsPoint(p, Qt::OddEvenFill))
            return false;
    return true;
}

QList<WorldPath*> BasePathScene::lookupPaths(const QRectF &sceneRect)
{
    QPolygonF worldPoly = renderer()->toWorld(sceneRect);
    QList<WorldPath*> paths = lookup()->paths(worldPoly);
    foreach (WorldPath *path, paths) {
        if (PolygonContainsPolyline(worldPoly, path->polygon()))
            continue;
        if (!PolylineIntersectsPolygon(path->polygon(), worldPoly))
            paths.removeOne(path);
    }
    return paths;
}

#include <QVector2D>
float minimum_distance(QVector2D v, QVector2D w, QVector2D p) {
    // Return minimum distance between line segment vw and point p
    const qreal l2 = (v - w).lengthSquared();  // i.e. |w-v|^2 -  avoid a sqrt
    if (l2 == 0.0) return (p - v).length();   // v == w case
    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line.
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2
    const qreal t = QVector2D::dotProduct(p - v, w - v) / l2;
    if (t < 0.0) return (p - v).length();       // Beyond the 'v' end of the segment
    else if (t > 1.0) return (p - w).length();  // Beyond the 'w' end of the segment
    const QVector2D projection = v + t * (w - v);  // Projection falls on the segment
    return (p - projection).length();
}

WorldPath *BasePathScene::topmostPathAt(const QPointF &scenePos)
{
    qreal radius = 4 / document()->view()->zoomable()->scale();
    QRectF sceneRect(scenePos.x() - radius, scenePos.y() - radius, radius * 2, radius * 2);
    QList<WorldPath*> paths = lookup()->paths(renderer()->toWorld(sceneRect));

    // Do distance check in scene coords otherwise isometric testing is 1/2 height
    QVector2D p(scenePos);
    qreal min = 1000;
    WorldPath *closest = 0;
    foreach (WorldPath *path, paths) {
        for (int i = 0; i < path->nodes.size() - 1; i++) {
            QVector2D v(mRenderer->toScene(path->nodes[i]->p));
            QVector2D w(mRenderer->toScene(path->nodes[i+1]->p));
            qreal dist = minimum_distance(v, w, p);
            if (dist <= radius && dist <= min) {
                min = dist;
                closest = path; // get the topmost path
            }
        }
    }

    return closest;
}

void BasePathScene::afterAddNode(WorldNode *node)
{
    lookup()->nodeAdded(node);
}

void BasePathScene::afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    Q_UNUSED(index)
    nodeItem()->nodeRemoved(node);
    lookup()->nodeRemoved(node);
}

void BasePathScene::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    Q_UNUSED(node)
    Q_UNUSED(prev)
}

void BasePathScene::afterAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
    lookup()->pathAdded(path);
    nodeItem()->update();
}

void BasePathScene::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(index)
    lookup()->pathRemoved(path);
    nodeItem()->update();
}

void BasePathScene::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    lookup()->pathChanged(path);
    nodeItem()->update();
}

void BasePathScene::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
    lookup()->pathChanged(path);
}

void BasePathScene::afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    mLevelItems[wlevel->level()]->afterAddPathLayer(index, layer);
}

void BasePathScene::beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    mLevelItems[wlevel->level()]->beforeRemovePathLayer(index, layer);
}

void BasePathScene::afterReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex)
{
    mLevelItems[wlevel->level()]->afterReorderPathLayer(layer, oldIndex);
}

void BasePathScene::afterSetPathLayerVisible(WorldPathLayer *layer, bool visible)
{
    mLevelItems[layer->wlevel()->level()]->afterSetPathLayerVisible(layer, visible);
}

void BasePathScene::nodeMoved(WorldNode *node)
{
    Q_UNUSED(node)
    nodeItem()->update(); // FIXME: redraw only as needed
}

void BasePathScene::currentPathLayerChanged(WorldPathLayer *layer)
{
    nodeItem()->currentPathLayerChanged();
}

/////

PathLayerItem::PathLayerItem(WorldPathLayer *layer, BasePathScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mLayer(layer),
    mScene(scene)
{
    setFlag(ItemUsesExtendedStyleOption);
}

QRectF PathLayerItem::boundingRect() const
{
    return mScene->renderer()->sceneBounds(mScene->world()->bounds(),
                                           mLayer->wlevel()->level());
}

void PathLayerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
#if 0
    QRectF bounds = QRectF(0, 0, 300, 300).translated(mScene->cell()->x() * 300,
                                                      mScene->cell()->y() * 300);

    painter->setBrush(QColor(0,0,128,128));
    foreach (WorldScript *ws, mScene->document()->lookupScripts(bounds)) {
        QRect r = ws->mRegion.boundingRect().translated(-mScene->cell()->pos() * 300);
        if (!r.intersects(QRect(QPoint(), QSize(300,300)))) continue;
        QRegion rgn = ws->mRegion.translated(-mScene->cell()->pos() * 300);
        foreach (QRect r, rgn.rects()) {
            painter->drawPolygon(mScene->renderer()->tileToPixelCoords(r));
        }
    }

    return;
#endif
#if 1
    QPolygonF exposed = mScene->renderer()->toWorld(option->exposedRect);
        foreach (WorldPath *path, mLayer->lookup()->paths(exposed)) {
#if 1
            if (!path->isVisible()) continue;
            painter->setPen(QPen());
            QPolygonF pf = path->polygon();
            painter->drawPolyline(mScene->renderer()->toScene(pf, mLayer->level()));
#else
            painter->setBrush(Qt::NoBrush);
            QPolygonF pf = path->polygon();
            if (path->isClosed()) {
                if (path->tags.contains(QLatin1String("landuse"))) {
                    QString v = path->tags[QLatin1String("landuse")];
                    if (v == QLatin1String("forest") || v == QLatin1String("wood"))
                        painter->setBrush(Qt::darkGreen);
                    else if (v == QLatin1String("park"))
                        painter->setBrush(Qt::green);
                    else if (v == QLatin1String("grass"))
                        painter->setBrush(QColor(Qt::green).lighter());
                } else if (path->tags.contains(QLatin1String("natural"))) {
                    QString v = path->tags[QLatin1String("natural")];
                    if (v == QLatin1String("water"))
                        painter->setBrush(Qt::blue);

                } else if (path->tags.contains(QLatin1String("leisure"))) {
                    QString v = path->tags[QLatin1String("leisure")];
                    if (v == QLatin1String("park"))
                        painter->setBrush(Qt::green);
                }
                painter->drawPolygon(mScene->renderer()->toScene(pf, mLayer->level()));
            } else {
                painter->drawPolyline(mScene->renderer()->toScene(pf, mLayer->level()));
                if (path->tags.contains(QLatin1String("highway"))) {
                    qreal width = 6;
                    QColor color = QColor(0,0,0,128);
                    QString v = path->tags[QLatin1String("highway")];
                    qreal residentialWidth = 6;
                    if (v == QLatin1String("residential")) width = 6; /// #1
                    else if (v == QLatin1String("pedestrian")) width = 5, color = QColor(0,64,0,128);
                    else if (v == QLatin1String("secondary")) width = 5, color = QColor(0,0,128,128);
                    else if (v == QLatin1String("secondary_link")) width = 5, color = QColor(0,0,128,128);
                    else if (v == QLatin1String("tertiary")) width = 4, color = QColor(0,128,0,128); /// #3
                    else if (v == QLatin1String("tertiary_link")) width = 4, color = QColor(0,128,0,128);
                    else if (v == QLatin1String("bridleway")) width = 4, color = QColor(128,128,0,128);
                    else if (v == QLatin1String("private")) width = 4, color = QColor(128,0,0,128);
                    else if (v == QLatin1String("service")) width = residentialWidth/2, color = QColor(0,0,64,128); /// #2
                    else if (v == QLatin1String("path")) width = 3, color = QColor(128,64,64,128); /// #2
                    else continue;

                    pf = strokePath(path, width);
                    painter->setBrush(color);
                    painter->drawPolygon(mScene->renderer()->toScene(pf, mLayer->level()));
                }
            }

            if (path->tags.contains(QLatin1String("name"))
                    && path->tags[QLatin1String("name")] == QLatin1String("Plant Science Field Building")) {
                painter->setBrush(Qt::red);
                painter->drawPolygon(pf);
            }
#endif
            if (mScene->selectedPaths().contains(path)) {
                painter->setPen(QPen(Qt::blue, 4 / painter->worldMatrix().m22()));
                painter->drawPolyline(mScene->renderer()->toScene(pf, mLayer->level()));
            }

            if (mScene->mHighlightPath == path) {
                painter->setPen(QPen(Qt::green, 4 / painter->worldMatrix().m22()));

                QPolygonF fwd, bwd;
                offsetPath(path, 0.5, fwd, bwd);

                qreal radius = mScene->nodeItem()->nodeRadius();

                fwd = mScene->renderer()->toScene(fwd, mLayer->level());
                painter->drawPolyline(fwd);
                foreach (QPointF p, fwd)
                    painter->drawEllipse(p, radius, radius);
                bwd = mScene->renderer()->toScene(bwd, mLayer->level());
                painter->drawPolyline(bwd);
                foreach (QPointF p, bwd)
                    painter->drawEllipse(p, radius, radius);
            }
        }

    return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0,64,0,128));
    foreach (WorldScript *ws, mLayer->lookup()->scripts(exposed.boundingRect())) {
        if (!ws->mRegion.isEmpty()) {
            foreach (QRect rect, ws->mRegion.rects())
                painter->drawPolygon(mScene->renderer()->toScene(rect));
        }
    }

#endif
}

PathList PathLayerItem::lookupPaths(const QRectF &sceneRect)
{
    QPolygonF worldPoly = mScene->renderer()->toWorld(sceneRect);
    PathList paths = mLayer->lookup()->paths(worldPoly);
    foreach (WorldPath *path, paths) {
        if (PolygonContainsPolyline(worldPoly, path->polygon()))
            continue;
        if (!PolylineIntersectsPolygon(path->polygon(), worldPoly))
            paths.removeOne(path);
    }
    return paths;
}

/////

NodesItem::NodesItem(BasePathScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mHoverNode(InvalidId)
{
    setFlag(ItemUsesExtendedStyleOption);
//    setAcceptHoverEvents(true);
}

QRectF NodesItem::boundingRect() const
{
    return mScene->renderer()->sceneBounds(mScene->world()->bounds());
}

void NodesItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (!mScene->isTile() && painter->worldMatrix().m22() < 0.75) return;
    if (mScene->isTile() && painter->worldMatrix().m22() < 0.10) return;

    QRectF exposed = option->exposedRect.adjusted(-nodeRadius(), -nodeRadius(),
                                                  nodeRadius(), nodeRadius());
    painter->setPen(QColor(0x33,0x99,0xff));
    painter->setBrush(QColor(0x33,0x99,0xff,255/8));
    foreach (WorldNode *node, lookupNodes(exposed)) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos());
        painter->drawEllipse(scenePos, nodeRadius(), nodeRadius());
    }

    painter->setBrush(QColor(0x33,0x99,0xff,128));
    foreach (WorldNode *node, mSelectedNodes) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos());
        if (exposed.contains(scenePos))
            painter->drawEllipse(scenePos, nodeRadius(), nodeRadius());
    }

    if (mHoverNode != InvalidId) {
        if (WorldNode *node = mScene->currentPathLayer()->node(mHoverNode)) {
            QPointF scenePos = mScene->renderer()->toScene(node->pos());
            if (exposed.contains(scenePos)) {
                painter->setPen(QPen(QColor(0,128,0,200), 3 / mScene->document()->view()->zoomable()->scale()));
                painter->setBrush(Qt::NoBrush);
                painter->drawEllipse(scenePos, nodeRadius() * 1.25, nodeRadius() * 1.25);
            }
        }
    }
}

void NodesItem::trackMouse(QGraphicsSceneMouseEvent *event)
{
    WorldNode *node = topmostNodeAt(event->scenePos());
    id_t id = node ? node->id : InvalidId;
    if (id != mHoverNode) {
        redrawNode(mHoverNode);
        mHoverNode = id;
        redrawNode(mHoverNode);
    }
}

void NodesItem::setSelectedNodes(const NodeSet &selection)
{
    mSelectedNodes = selection;
    update(); // FIXME: don't redraw everything
}

NodeList NodesItem::lookupNodes(const QRectF &sceneRect)
{
    if (!mScene->currentPathLayer()) return NodeList();
    QRectF testRect = sceneRect.adjusted(-nodeRadius(), -nodeRadius(),
                                         nodeRadius(), nodeRadius());
    NodeList nodes =  mScene->lookup()->nodes(
                mScene->renderer()->toWorld(testRect));
    foreach(WorldNode *node, nodes) {
        if (!testRect.contains(mScene->renderer()->toScene(node->pos(),
                                                           mScene->currentPathLayer()->level())))
            nodes.removeOne(node);
    }
    return nodes;
}

WorldNode *NodesItem::topmostNodeAt(const QPointF &scenePos)
{
    if (!mScene->currentPathLayer()) return 0;
    QRectF sceneRect(scenePos.x() - nodeRadius(), scenePos.y() - nodeRadius(),
                     nodeRadius() * 2, nodeRadius() * 2);
    QList<WorldNode*> nodes = mScene->lookup()->nodes(
                mScene->renderer()->toWorld(sceneRect));
    return nodes.size() ? nodes.last() : 0;
}

void NodesItem::nodeRemoved(WorldNode *node)
{
    if (mSelectedNodes.contains(node)) {
        mSelectedNodes.remove(node);
        redrawNode(node->id);
    }

    if (mHoverNode == node->id) {
        mHoverNode = InvalidId;
        redrawNode(node->id);
    }
}

void NodesItem::redrawNode(id_t id)
{
    if (WorldNode *node = mScene->currentPathLayer()->node(id)) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos());
        update(scenePos.x() - nodeRadius() * 2, scenePos.y() - nodeRadius() * 2,
               nodeRadius() * 4, nodeRadius() * 4);
    }
}

qreal NodesItem::nodeRadius() const
{
    return 6 / mScene->document()->view()->zoomable()->scale();
}

void NodesItem::currentPathLayerChanged()
{
    mSelectedNodes.clear();
    mHoverNode = InvalidId;
    update();
}

/////

WorldLevelItem::WorldLevelItem(WorldLevel *wlevel, BasePathScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mLevel(wlevel),
    mScene(scene)
{
    setFlag(ItemHasNoContents);

    int z = 1;
    foreach (WorldPathLayer *layer, mLevel->pathLayers()) {
        PathLayerItem *item = new PathLayerItem(layer, scene, this);
        item->setZValue(z++);
        if (mScene->isTile())
            item->setOpacity(0.5);
        mPathLayerItems += item;
    }
}

void WorldLevelItem::afterAddPathLayer(int index, WorldPathLayer *layer)
{
    PathLayerItem *item = new PathLayerItem(layer, mScene, this);
    mPathLayerItems.insert(index, item);
    for (int z = index; z < mPathLayerItems.size(); z++)
        mPathLayerItems[z]->setZValue(z + 1);
}

void WorldLevelItem::beforeRemovePathLayer(int index, WorldPathLayer *layer)
{
    Q_UNUSED(layer)
    delete mPathLayerItems.takeAt(index);
    for (int z = index; z < mPathLayerItems.size(); z++)
        mPathLayerItems[z]->setZValue(z + 1);
}

void WorldLevelItem::afterReorderPathLayer(WorldPathLayer *layer, int oldIndex)
{
    mPathLayerItems.move(oldIndex, mLevel->indexOf(layer));
    for (int z = 0; z < mPathLayerItems.size(); z++)
        mPathLayerItems[z]->setZValue(z + 1);
}

void WorldLevelItem::afterSetPathLayerVisible(WorldPathLayer *layer, bool visible)
{
    mPathLayerItems[mLevel->indexOf(layer)]->setVisible(visible);
}
