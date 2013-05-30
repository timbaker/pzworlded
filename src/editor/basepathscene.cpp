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
#include "preferences.h"
#include "textureeditdialog.h"
#include "worldchanger.h"
#include "worldlookup.h"
#include "zoomable.h"

#include <QGLWidget>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QtOpenGL>
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
    mChanger(doc->changer()/*new WorldChanger(doc->world())*/),
    mGridItem(new PathGridItem(this))
{
    mGridItem->setZValue(9999);
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

    mGridItem->updateBoundingRect();
    addItem(mGridItem);

    setSceneRect(mRenderer->toScene(world()->bounds(), 16).boundingRect()
                 | mRenderer->toScene(world()->bounds(), 0).boundingRect());

    connect(mDocument->changer(), SIGNAL(afterMoveNodeSignal(WorldNode*,QPointF)),
            SLOT(afterMoveNode(WorldNode*,QPointF)));
    connect(mDocument->changer(), SIGNAL(afterReorderPathSignal(WorldPath*,int)),
            SLOT(update()));
    connect(mDocument->changer(), SIGNAL(afterSetPathVisibleSignal(WorldPath*,bool)),
            SLOT(update()));
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

    connect(mChanger, SIGNAL(afterAddPathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterAddPath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterRemovePathSignal(WorldPathLayer*,int,WorldPath*)),
            SLOT(afterRemovePath(WorldPathLayer*,int,WorldPath*)));
    connect(mChanger, SIGNAL(afterAddNodeToPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterAddNodeToPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterRemoveNodeFromPathSignal(WorldPath*,int,WorldNode*)),
            SLOT(afterRemoveNodeFromPath(WorldPath*,int,WorldNode*)));
    connect(mChanger, SIGNAL(afterSetPathClosedSignal(WorldPath*,bool)),
            SLOT(afterSetPathClosed(WorldPath*,bool)));
    connect(mChanger, SIGNAL(afterSetPathTextureParamsSignal(WorldPath*,PathTexture)),
            SLOT(update()));
    connect(mChanger, SIGNAL(afterSetPathStrokeSignal(WorldPath*,qreal)),
            SLOT(update()));
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

    if (mSelectedPaths.size() == 1) {
        TextureEditDialog::instance()->setPath(mSelectedPaths.values().first());
        TextureEditDialog::instance()->show();
    } else {
        TextureEditDialog::instance()->setPath(0);
        TextureEditDialog::instance()->hide();
    }
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
    QPolygonF worldPoly = renderer()->toWorld(sceneRect, currentPathLayer()->level());
    QList<WorldPath*> paths = lookup()->paths(worldPoly);
    foreach (WorldPath *path, paths) {
        QPolygonF pathPoly = path->polygon(false);
        if (PolygonContainsPolyline(worldPoly, pathPoly))
            continue;
        if (!PolylineIntersectsPolygon(pathPoly, worldPoly))
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

WorldPath *BasePathScene::topmostPathAt(const QPointF &scenePos, int *indexPtr)
{
    qreal radius = 8 / document()->view()->scale();
    QRectF sceneRect(scenePos.x() - radius, scenePos.y() - radius, radius * 2, radius * 2);
    QList<WorldPath*> paths = lookup()->paths(renderer()->toWorld(sceneRect, currentPathLayer()->level()));

    // Do distance check in scene coords otherwise isometric testing is 1/2 height
    QVector2D p(scenePos);
    qreal min = 1000;
    WorldPath *closest = 0;
    foreach (WorldPath *path, paths) {
        QPolygonF scenePoly = mRenderer->toScene(path->polygon(false), currentPathLayer()->level());
        for (int i = 0; i < scenePoly.size() - 1; i++) {
            QVector2D v(scenePoly[i]);
            QVector2D w(scenePoly[i+1]);
            qreal dist = minimum_distance(v, w, p);
            if (dist <= radius && dist <= min) {
                min = dist;
                closest = path; // get the topmost path
                if (indexPtr) *indexPtr = i;
            }
        }
    }

    return closest;
}

void BasePathScene::setSelectedSegments(const SegmentList &segments)
{
    mSelectedSegments = segments;
    update();
}

void BasePathScene::setHighlightSegment(WorldPath *path, int nodeIndex)
{
    PathSegment seg(path, nodeIndex);
    if (seg != mHighlightSegment) {
        mHighlightSegment = seg;
        update();
    }
}

void BasePathScene::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    Q_UNUSED(node)
    Q_UNUSED(prev)
    nodeItem()->update(); // FIXME: redraw only as needed
}

void BasePathScene::afterAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(layer)
    Q_UNUSED(index)
//    lookup()->pathAdded(path);
    nodeItem()->update();
}

void BasePathScene::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    Q_UNUSED(index)
    if (mHighlightSegment.path() == path)
        mHighlightSegment = PathSegment();
    foreach (PathSegment segment, mSelectedSegments) {
        if (segment.path() == path)
            mSelectedSegments.removeOne(segment);
    }
//    lookup()->pathRemoved(path);
    nodeItem()->update();
}

void BasePathScene::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    Q_UNUSED(index)
    Q_UNUSED(node)
//    lookup()->pathChanged(path);
    nodeItem()->update();
}

void BasePathScene::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    nodeItem()->nodeRemoved(node);
    if (mHighlightSegment.path() == path && mHighlightSegment.nodeIndex() == index)
        mHighlightSegment = PathSegment();
    foreach (PathSegment segment, mSelectedSegments) {
        if (segment.path() == path && segment.nodeIndex() == index)
            mSelectedSegments.removeOne(segment);
    }

//    lookup()->pathChanged(path);
    update();
}

void BasePathScene::afterSetPathClosed(WorldPath *path, bool wasClosed)
{
    update();
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

void BasePathScene::currentPathLayerChanged(WorldPathLayer *layer)
{
    nodeItem()->currentPathLayerChanged();
}

void BasePathScene::pathTextureChanged(WorldPath *path)
{
    update();
}

unsigned int BasePathScene::loadGLTexture(const QString &fileName)
{
    QImage t;
    QImage b;

    if (b.load(fileName)) {
        QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
        QPainter painter(&fixedImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(fixedImage.rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage( 0, 0, b);
        painter.end();
        b = fixedImage;

        t = QGLWidget::convertToGLFormat( b );
        GLuint texture;
        glGenTextures( 1, &texture );
        glBindTexture( GL_TEXTURE_2D, texture );
        glTexImage2D( GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits() );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        return texture;
    }

    return 0;
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

#include "poly2tri/poly2tri.h"
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
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLushort> indices;
    int numcdt = 0;
    QPolygonF exposed = mScene->renderer()->toWorld(option->exposedRect, mLayer->level());
    foreach (WorldPath *path, mLayer->lookup()->paths(exposed)) {
#if 1
        if (!path->isVisible()) continue;
        painter->setPen(QPen());
        QPolygonF worldPoly = path->polygon();
        QPolygonF scenePoly = mScene->renderer()->toScene(worldPoly, mLayer->level());

        if (numcdt < 50 && worldPoly.isClosed() && (path->texture().mTexture != 0)) {
            std::vector<p2t::Point*> polyline;
            qreal factor = 1;
            foreach (QPointF p, worldPoly.mid(0,worldPoly.size()-1)) {
                if (polyline.size() && p.x() == polyline.back()->x && p.y() == polyline.back()->y) continue; // forbidden
                polyline.push_back(new p2t::Point(p.x() * factor, p.y() * factor));
            }
            p2t::CDT cdt(polyline);
            cdt.Triangulate();
            std::vector<p2t::Triangle*> triangles = cdt.GetTriangles();
            foreach (p2t::Triangle *tri, triangles) {
                QPointF p0 = QPointF(tri->GetPoint(0)->x, tri->GetPoint(0)->y);
                QPointF p1 = QPointF(tri->GetPoint(1)->x, tri->GetPoint(1)->y);
                QPointF p2 = QPointF(tri->GetPoint(2)->x, tri->GetPoint(2)->y);
                indices << indices.size() << indices.size() + 1 << indices.size() + 2;
                QPointF sp0 = mScene->renderer()->toScene(p0, mLayer->level());
                QPointF sp1 = mScene->renderer()->toScene(p1, mLayer->level());
                QPointF sp2 = mScene->renderer()->toScene(p2, mLayer->level());
                vertices << QVector3D(sp0.x(), sp0.y(), 0)
                         << QVector3D(sp1.x(), sp1.y(), 0)
                         << QVector3D(sp2.x(), sp2.y(), 0);

                // 1 world "unit" == 1 tile
                // 1 tile == 64/64 pixels
                qreal texw = path->texture().mTexture->mSize.width()/*/64.0f*/;
                qreal texh = path->texture().mTexture->mSize.height()/*/64.0f*/;

                QTransform xform;
                xform.scale(64/texw, -64/texh); // inverting the Y-axis as well
                xform.translate(-path->texture().mTranslation.x()*(1.0f/64), -path->texture().mTranslation.y()*(1.0f/64));
                xform.scale(1/path->texture().mScale.width(), 1/path->texture().mScale.height());
                xform.rotate(path->texture().mRotation);

                if (path->texture().mAlignWorld) {
                    texcoords << QVector2D(xform.map(p0));
                    texcoords << QVector2D(xform.map(p1));
                    texcoords << QVector2D(xform.map(p2));
                } else {
                    QRectF bounds = path->bounds();
                    texcoords << QVector2D(xform.map(p0-bounds.topLeft()));
                    texcoords << QVector2D(xform.map(p1-bounds.topLeft()));
                    texcoords << QVector2D(xform.map(p2-bounds.topLeft()));
                }
            }
            qDeleteAll(polyline);
            //                qDeleteAll(triangles);
            numcdt++;

            if (path->texture().mTexture != 0 && path->texture().mTexture->mSize.isValid()) {
                painter->beginNativePainting();

                if (path->texture().mTexture->mGLid == 0) {
                    path->texture().mTexture->mGLid = mScene->loadGLTexture(path->texture().mTexture->mFileName);
                }

                if (path->texture().mTexture->mGLid != 0) {
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
                    glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
                    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glBindTexture(GL_TEXTURE_2D, path->texture().mTexture->mGLid);
                    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, indices.constData());
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
                    glDisableClientState(GL_VERTEX_ARRAY);
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                }

                painter->endNativePainting();

                indices.resize(0);
                texcoords.resize(0);
                vertices.resize(0);
            }
        }

        painter->setPen(path->strokeWidth() ? Qt::gray : Qt::black);
        painter->drawPolyline(scenePoly);
        if (path->strokeWidth()) {
            painter->setPen(Qt::black);
            painter->drawPolyline(mScene->renderer()->toScene(path->polygon(false), mLayer->level()));
        }

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
            painter->drawPolyline(mScene->renderer()->toScene(worldPoly, mLayer->level()));
        }

        if (mScene->mHighlightPath == path) {
            painter->setPen(QPen(Qt::green, 4 / painter->worldMatrix().m22()));
#if 1
            painter->drawPolyline(mScene->renderer()->toScene(worldPoly, mLayer->level()));
#else
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
#endif
        }
    }

    foreach (PathSegment segment, mScene->selectedSegments()) {
        QPolygonF worldPoly = segment.path()->polygon();
        QPolygonF scenePoly = mScene->renderer()->toScene(worldPoly, mLayer->level());
        painter->setPen(QPen(Qt::blue, 4 / painter->worldMatrix().m22()));
        painter->drawLine(scenePoly[segment.nodeIndex()], scenePoly[segment.nodeIndex() + 1]);
    }

    const PathSegment &seg = mScene->highlightSegment();
    if (seg.path() != 0 && seg.path()->layer() == mLayer) {
        painter->setPen(QPen(Qt::green, 4 / painter->worldMatrix().m22()));
        QPointF p0 = seg.path()->nodePos(seg.nodeIndex());
        int index = (seg.nodeIndex() == seg.path()->nodeCount() - 1) ? 0 : (seg.nodeIndex() + 1);
        QPointF p1 = seg.path()->nodePos(index);
        painter->drawLine(mScene->renderer()->toScene(p0, mLayer->level()),
                          mScene->renderer()->toScene(p1, mLayer->level()));
    }

#if 0
    if (indices.size()) {
        painter->beginNativePainting();

        if (mScene->mTextureId == 0)
            mScene->mTextureId = mScene->loadGLTexture(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/francegrassfull.jpg"));

#if 0
        for (int i = 0; i < texcoords.size(); i++) {
            texcoords[i].setX(qBound(qreal(0), texcoords[i].x(), qreal(1)));
            texcoords[i].setY(qBound(qreal(0), texcoords[i].y(), qreal(1)));
        }
#endif
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
        glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(GL_TEXTURE_2D, mScene->mTextureId);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, indices.constData());
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        painter->endNativePainting();
    }
#endif
    QVector<QPointF> pts;
    for (int i = 0; i < vertices.size() - 2; i += 3) {
        pts << vertices[i].toPointF() << vertices[i + 1].toPointF();
        pts << vertices[i+1].toPointF() << vertices[i + 2].toPointF();
        pts << vertices[i+2].toPointF() << vertices[i].toPointF();
    }
    painter->drawLines(pts.constData(), pts.size() / 2);
#if 0
    for (int i = 0; i < texcoords.size(); i++) {
        painter->drawText(vertices[i].toPointF(), QString::fromLatin1("%1,%2").arg(texcoords[i].x()).arg(texcoords[i].y()));
    }
#endif
    return; ////////////////////////////////////////

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0,64,0,128));
    foreach (WorldScript *ws, mLayer->lookup()->scripts(exposed.boundingRect())) {
        if (!ws->mRegion.isEmpty()) {
            foreach (QRect rect, ws->mRegion.rects())
                painter->drawPolygon(mScene->renderer()->toScene(rect, mLayer->level()));
        }
    }
#endif
}

PathList PathLayerItem::lookupPaths(const QRectF &sceneRect)
{
    QPolygonF worldPoly = mScene->renderer()->toWorld(sceneRect, mLayer->level());
    PathList paths = mLayer->lookup()->paths(worldPoly);
    foreach (WorldPath *path, paths) {
        if (PolygonContainsPolyline(worldPoly, path->polygon(false)))
            continue;
        if (!PolylineIntersectsPolygon(path->polygon(false), worldPoly))
            paths.removeOne(path);
    }
    return paths;
}

/////

NodesItem::NodesItem(BasePathScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mHoverNode(0)
{
    setFlag(ItemUsesExtendedStyleOption);
//    setAcceptHoverEvents(true);
}

QRectF NodesItem::boundingRect() const
{
    return mScene->renderer()->sceneBounds(mScene->world()->bounds(),
                                           mScene->currentPathLayer()->level());
}

void NodesItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    qreal scale = painter->worldMatrix().m22();
    qreal nonTileScaleFactor = mScene->isTile() ? 1 : 32;
    if (mScene->isTile() && scale < 0.125) return;
    if (!mScene->isTile() && (scale*nonTileScaleFactor < 2.0)) return;

    QRectF exposed = option->exposedRect.adjusted(-nodeRadius(), -nodeRadius(),
                                                  nodeRadius(), nodeRadius());
    if (mScene->isTile()) {
        painter->setPen(Qt::black);
        painter->setBrush(Qt::white);
    } else {
        painter->setPen(QColor(0x33,0x99,0xff));
        painter->setBrush(QColor(0x33,0x99,0xff,255/8));
    }
    foreach (WorldNode *node, lookupNodes(exposed)) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos(),
                                                       mScene->currentPathLayer()->level());
        drawNodeSquare(painter, scenePos, nodeRadius());
    }

    // Fake node used by AddRemoveNodeTool
    if (!mFakeNodePos.isNull()) {
        QPointF scenePos = mScene->renderer()->toScene(mFakeNodePos,
                                                       mScene->currentPathLayer()->level());
        drawNodeSquare(painter, scenePos, nodeRadius());
    }

    painter->setBrush(QColor(0x33,0x99,0xff,128));
    foreach (WorldNode *node, mSelectedNodes) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos(),
                                                       mScene->currentPathLayer()->level());
        if (exposed.contains(scenePos))
            drawNodeSquare(painter, scenePos, nodeRadius());
    }

    if (mHoverNode) {
        QPointF scenePos = mScene->renderer()->toScene(mHoverNode->pos(),
                                                       mScene->currentPathLayer()->level());
        if (exposed.contains(scenePos)) {
            painter->setPen(QPen(Qt::green, 3.0 / mScene->document()->view()->scale()));
            painter->setBrush(Qt::NoBrush);
            drawNodeSquare(painter, scenePos, nodeRadius() * 1.25);
        }
    }
}

void NodesItem::trackMouse(QGraphicsSceneMouseEvent *event)
{
    WorldNode *node = topmostNodeAt(event->scenePos());
    if (node != mHoverNode) {
        redrawNode(mHoverNode);
        mHoverNode = node;
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
                mScene->renderer()->toWorld(testRect,
                                            mScene->currentPathLayer()->level()));
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
                mScene->renderer()->toWorld(sceneRect,
                                            mScene->currentPathLayer()->level()));
    return nodes.size() ? nodes.last() : 0;
}

void NodesItem::nodeRemoved(WorldNode *node)
{
    if (mSelectedNodes.contains(node)) {
        mSelectedNodes.remove(node);
        redrawNode(node);
    }

    if (mHoverNode == node) {
        mHoverNode = 0;
        redrawNode(node);
    }
}

void NodesItem::redrawNode(WorldNode *node)
{
    if (node) {
        QPointF scenePos = mScene->renderer()->toScene(node->pos(),
                                                       mScene->currentPathLayer()->level());
        update(scenePos.x() - nodeRadius() * 2, scenePos.y() - nodeRadius() * 2,
               nodeRadius() * 4, nodeRadius() * 4);
    }
}

qreal NodesItem::nodeRadius() const
{
    return 4 / mScene->document()->view()->scale();
}

void NodesItem::currentPathLayerChanged()
{
    mSelectedNodes.clear();
    mHoverNode = InvalidId;
    update();
}

void NodesItem::drawNodeElipse(QPainter *painter, const QPointF &scenePos, qreal radius)
{
    painter->drawEllipse(scenePos, radius, radius);
}

void NodesItem::drawNodeSquare(QPainter *painter, const QPointF &scenePos, qreal radius)
{
    painter->drawRect(QRectF(scenePos.x() - radius, scenePos.y() - radius, radius * 2, radius * 2));
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
#if 0
        if (mScene->isTile())
            item->setOpacity(0.5);
#endif
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

/////

PathGridItem::PathGridItem(BasePathScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(scene)
{
    setAcceptedMouseButtons(0);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    connect(Preferences::instance(), SIGNAL(showCellGridChanged(bool)),
            SLOT(showGridChanged(bool)));
}

QRectF PathGridItem::boundingRect() const
{
    return mBoundingRect;
}

void PathGridItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *)
{
    QColor gridColor = Preferences::instance()->gridColor();
    mScene->renderer()->drawGrid(painter, option->exposedRect, gridColor,
                                 mScene->document()->currentLevel()->level());
}

void PathGridItem::updateBoundingRect()
{
    QRectF boundsF;
    if (mScene->renderer())
        boundsF = mScene->renderer()->sceneBounds(mScene->world()->bounds(),
                                                  mScene->document()->currentLevel()->level());
    if (boundsF != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = boundsF;
    }
}
