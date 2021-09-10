/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "ingamemapscene.h"

#include "ingamemapundo.h"
#include "worldcellingamemap.h"

#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "maprenderer.h"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

InGameMapFeatureItem::InGameMapFeatureItem(InGameMapFeature* feature, CellScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mWorldDoc(scene->worldDocument())
    , mFeature(feature)
    , mRenderer(scene->renderer())
    , mSyncing(false)
    , mIsEditable(false)
    , mIsSelected(false)
    , mHoverRefCount(0)
{
    setAcceptHoverEvents(true);
    synchWithFeature();
}

QRectF InGameMapFeatureItem::boundingRect() const
{
    return mBoundingRect;
}

void InGameMapFeatureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QColor color = mHoverRefCount > 0 ? Qt::blue : Qt::darkBlue;
    if (isSelected())
        color = mHoverRefCount ? Qt::green : Qt::darkGreen;
    if (isEditable())
        color = mHoverRefCount ? Qt::red : Qt::darkRed;

//    if (mHoverRefCount)
//        painter->drawPath(shape());

    QColor brushColor = color;
    brushColor.setAlpha(50);
    QBrush brush(brushColor);

    QPen pen(Qt::black);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(2);
    pen.setCosmetic(true);

    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);

    QPolygonF screenPolygon = mRenderer->tileToPixelCoords(mPolygon.translated(mDragOffset));

    switch (geometryType()) {
    case Type::INVALID:
        break;
    case Type::Point:
        pen.setColor(color);
        painter->setPen(pen);
        painter->setBrush(brush);
        painter->drawEllipse(screenPolygon[0], 10, 10);
        break;
    case Type::Polygon:
    {
        QPainterPath path;
        if (screenPolygon.isClosed() == false) {
            screenPolygon += screenPolygon.first();
        }
        path.addPolygon(screenPolygon);
        QPainterPath path2;
        for (auto& hole : mHoles) {
            QPolygonF screenPolygon2 = mRenderer->tileToPixelCoords(hole.translated(mDragOffset));
            if (screenPolygon2.isClosed() == false) {
                screenPolygon2 += screenPolygon2.first();
            }
            path2.addPolygon(screenPolygon2);
        }
        path = path.subtracted(path2);
        painter->drawPath(path);

        pen.setColor(color);
        painter->setPen(pen);
        painter->setBrush(brush);
        path.translate(0, -1);
        painter->drawPath(path);
        break;
    }
    case Type::Polyline:
#if 0
    {
        int width = feature()->properties().getInt(QStringLiteral("width"), -1);
        if (width > 0) {
            QPainterPathStroker stroker;
            stroker.setWidth(width * 64);
            QPainterPath path1;
            path1.addPolygon(screenPolygon);
            QPainterPath path = stroker.createStroke(path1);
            painter->drawPath(path);
            break;
        }
    }
#endif
        painter->drawPolyline(screenPolygon);

        pen.setColor(color);
        painter->setPen(pen);
        painter->setBrush(brush);
        screenPolygon.translate(0, -1);
        painter->drawPolyline(screenPolygon);
        break;
    }

    if (mAddPointIndex != -1) {
        auto scene = static_cast<CellScene*>(this->scene());
        auto view = static_cast<CellView*>(scene->views().first());
        qreal zoom = view->zoomable()->scale();
        zoom = qMin(zoom, 1.0);

        painter->drawRect(mAddPointPos.x() - 5/zoom, mAddPointPos.y() -5/zoom, (int)(10 / zoom), (int)(10 / zoom));
    }
}

void InGameMapFeatureItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (++mHoverRefCount == 1) {
        update();
    }
}

// http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
static float distanceOfPointToLineSegment(QVector2D p1, QVector2D p2, QVector2D p)
{
    QVector2D n = p2 - p1;
    QVector2D pa =  p1 - p;

    float c = QVector2D::dotProduct(n, pa);

    if (c > 0.0f)
        return QVector2D::dotProduct(pa, pa);

    QVector2D bp = p - p2;

    if (QVector2D::dotProduct(n, bp) > 0.0f)
        return QVector2D::dotProduct(bp, bp);

    QVector2D e = pa - n * (c / QVector2D::dotProduct(n, n));

    return QVector2D::dotProduct(e, e);
}

#include <QtMath>

void InGameMapFeatureItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isEditable()) {
        if (mAddPointIndex != -1) {
            mAddPointIndex = -1;
            update();
        }
        return;
    }

    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    zoom = qMin(zoom, 1.0);

    QPolygonF poly = mRenderer->tileToPixelCoords(mPolygon, 0);

    // Don't add points near other points
    for (int i = 0; i < poly.size(); i++) {
        float d = QVector2D(event->scenePos()).distanceToPoint(QVector2D(poly[i]));
        if (d < 20 / (float) zoom) {
            if (mAddPointIndex != -1) {
                mAddPointIndex = -1;
                update();
            }
            return;
        }
    }

    // Find the line segment the mouse pointer is over
    int closestIndex = -1;
    float closestDist = 10000;
    for (int i = 0; i < poly.size() - 1; i++) {
        QVector2D p1(poly[i]), p2(poly[i+1]);
//        QVector2D dir = (p2 - p1).normalized();
//        float d = QVector2D(event->scenePos()).distanceToLine(p1, dir);
        float d = distanceOfPointToLineSegment(p1, p2, QVector2D(event->scenePos()));
        d = qSqrt(qAbs(d));
        if (d < 10 / (float)zoom && d < closestDist) {
            closestIndex = i;
            closestDist = d;
        }
    }
    if (closestIndex != -1) {
        mAddPointIndex = closestIndex;
        mAddPointPos = event->scenePos();
//        qDebug() << "mAddPointIndex " << mAddPointIndex << " dist " << closestDist;
        update();
    } else if (mAddPointIndex != -1) {
        mAddPointIndex = -1;
        update();
    }

}

void InGameMapFeatureItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (--mHoverRefCount == 0) {
        mAddPointIndex = -1;
        update();
    }
}

QPainterPath InGameMapFeatureItem::shape() const
{
    QPolygonF polygon = mRenderer->tileToPixelCoords(mPolygon, 0);

    if (isPolygon())
        polygon += polygon[0];

    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    zoom = qMin(zoom, 1.0);

    QPainterPath path;
    if (isPoint()) {
//        path.addEllipse(polygon[0], 10, 10);
        path.addRect(polygon[0].x() - 10, polygon[0].y() - 10, 20, 20);
        return path;
    }
    path.addPolygon(polygon);

    if (isPolygon() && (mHoles.isEmpty() == false)) {
        // Holes
        QPainterPath path2;
        for (int i = 0; i < mHoles.size(); i++) {
            path2.addPolygon(mRenderer->tileToPixelCoords(mHoles[i]));
        }
        path = path.subtracted(path2);
    }

    QPainterPathStroker stroker;
    stroker.setWidth(20 / zoom);
    return stroker.createStroke(path);
}

bool InGameMapFeatureItem::contains(const QPointF &point) const
{
    return QGraphicsItem::contains(point);
}

void InGameMapFeatureItem::synchWithFeature()
{
    mPolygon.clear();
    mHoles.clear();
    mAddPointIndex = -1;

    switch (geometryType()) {
    case Type::INVALID:
        break;
    case Type::Point: {
        InGameMapPoint center = mFeature->mGeometry.mCoordinates[0][0];
        mPolygon += { center.x , center.y };
        QPointF scenePos = mRenderer->tileToPixelCoords(mPolygon[0] + mDragOffset);
        QRectF bounds(scenePos.x() - 10, scenePos.y() - 10, 20, 20);
        if (bounds != mBoundingRect) {
            prepareGeometryChange();
            mBoundingRect = bounds;
        }
        return;
    }
    case Type::Polygon:
    {
        if (mFeature->mGeometry.mCoordinates.isEmpty()) {
            break;
        }
        const InGameMapCoordinates& outer = mFeature->mGeometry.mCoordinates.first();
        for (auto& point : outer) {
            mPolygon += QPointF(point.x, point.y);
        }
        bool bClockwise = outer.isClockwise();
        for (int i = 1; i < mFeature->mGeometry.mCoordinates.size(); i++) {
            const auto& inner = mFeature->mGeometry.mCoordinates[i];
            QPolygonF hole;
            if (bClockwise == inner.isClockwise()) {
                for (int j = inner.size() - 1; j >= 0; j--) {
                    auto& point = inner[j];
                    hole += QPointF(point.x, point.y);
                }
            } else {
                for (auto& point : inner) {
                    hole += QPointF(point.x, point.y);
                }
            }
            mHoles += hole;
        }
        break;
    }
    case Type::Polyline:
        for (auto& coords : mFeature->mGeometry.mCoordinates) {
            for (auto& point : coords) {
                mPolygon += QPointF(point.x, point.y);
            }
            break; // should only be one array of coordinates
        }
        break;
    }

    QRectF bounds = mRenderer->tileToPixelCoords(mPolygon.translated(mDragOffset)).boundingRect().adjusted(-2, -3, 2, 2);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void InGameMapFeatureItem::setSelected(bool selected)
{
    mIsSelected = selected;
    update();
}

void InGameMapFeatureItem::movePoint(int pointIndex, const InGameMapPoint &point)
{
    auto& coords = mFeature->mGeometry.mCoordinates[0];
    coords[pointIndex] = point;
    synchWithFeature();
}

void InGameMapFeatureItem::setEditable(bool editable)
{
    mIsEditable = editable;
    update();
}

InGameMapFeatureItem::Type InGameMapFeatureItem::geometryType() const
{
    if (isPoint()) return Type::Point;
    if (isPolygon()) return Type::Polygon;
    if (isPolyline()) return Type::Polyline;
    return Type::INVALID;
}

bool InGameMapFeatureItem::isPoint() const
{
    return mFeature->mGeometry.isPoint();
}

bool InGameMapFeatureItem::isPolygon() const
{
    return mFeature->mGeometry.isPolygon();
}

bool InGameMapFeatureItem::isPolyline() const
{
    return mFeature->mGeometry.isLineString();
}

/////

class FeatureHandle : public QGraphicsItem
{
public:
    FeatureHandle(InGameMapFeatureItem *featureItem, int pointIndex)
        : QGraphicsItem(featureItem)
        , mFeatureItem(featureItem)
        , mPointIndex(pointIndex)
    {
        setFlags(QGraphicsItem::ItemIsMovable |
                 QGraphicsItem::ItemSendsGeometryChanges |
                 QGraphicsItem::ItemIgnoresTransformations |
                 QGraphicsItem::ItemIgnoresParentOpacity);
        setAcceptHoverEvents(true);
    }

    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
        if (++mHoverRefCount == 1) {
            update();
        }
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
        if (--mHoverRefCount == 0) {
            update();
        }
    }

    InGameMapPoint geometryPoint() const {
        auto& coords = mFeatureItem->feature()->mGeometry.mCoordinates;
        if (coords.isEmpty() || mPointIndex < 0 || mPointIndex >= coords[0].size())
            return { -1, -1 };
        return coords[0].at(mPointIndex);
    }

    bool isSelected() const {
        CellScene *scene = static_cast<CellScene*>(this->scene());
        return scene->document()->selectedInGameMapPoints().contains(mPointIndex);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

protected:
    InGameMapFeatureItem *mFeatureItem;
    int mPointIndex;
    InGameMapPoint mOldPos;
    bool mMoveAllPoints = false;
    int mHoverRefCount = 0;
};

QRectF FeatureHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void FeatureHandle::paint(QPainter *painter,
                   const QStyleOptionGraphicsItem*,
                   QWidget *)
{
    painter->setBrush(mHoverRefCount ? Qt::red : Qt::blue);
    painter->setPen(Qt::black);
    painter->drawRect(QRectF(-5, -5, 10, 10));

    if (isSelected()) {
        painter->setPen(Qt::white);
        painter->drawRect(QRectF(-5, -5, 10, 10));
    }
}

void FeatureHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        mOldPos = geometryPoint();
        mMoveAllPoints = (event->modifiers() & Qt::ShiftModifier) != 0;

        CellScene *scene = static_cast<CellScene*>(this->scene());
        QList<int> selection = scene->document()->selectedInGameMapPoints();
        if (event->modifiers() & Qt::ControlModifier) {
            if (isSelected()) {
                selection.removeOne(mPointIndex);
            } else {
                selection += mPointIndex;
            }
        } else {
            if (isSelected() == false) {
                selection.clear();
                selection += mPointIndex;
            }
        }
        scene->document()->setSelectedInGameMapPoints(selection);
    }

    // Stop the object context menu messing us up.
    event->accept();
}

void FeatureHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton && (mOldPos != geometryPoint())) {
        WorldDocument *document = mFeatureItem->mWorldDoc;
        int featureIndex = mFeatureItem->feature()->index();
        InGameMapPoint newPos = geometryPoint();
        mFeatureItem->feature()->mGeometry.mCoordinates[0][mPointIndex] = mOldPos;
        if (mMoveAllPoints) {
            InGameMapCoordinates coords = mFeatureItem->feature()->mGeometry.mCoordinates[0];
            coords.translate(int(newPos.x - mOldPos.x), int(newPos.y - mOldPos.y));
            QUndoCommand *cmd = new SetInGameMapCoordinates(document, mFeatureItem->feature()->cell(), featureIndex, 0, coords);
            document->undoStack()->push(cmd);
        } else {
            QUndoCommand *cmd = new MoveInGameMapPoint(document, mFeatureItem->feature()->cell(), featureIndex, mPointIndex, newPos);
            document->undoStack()->push(cmd);
        }
    }

    // Stop the context-menu messing us up.
    event->accept();
}

QVariant FeatureHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (!mFeatureItem->mSyncing) {
        auto* renderer = mFeatureItem->mRenderer;

        if (change == ItemPositionChange) {
            bool snapToGrid = true;

            // Calculate the absolute pixel position
            const QPointF itemPos = mFeatureItem->pos();
            QPointF pixelPos = value.toPointF() + itemPos;

            // Calculate the new coordinates in tiles
            QPointF tileCoords = renderer->pixelToTileCoordsNearest(pixelPos, 0);

            const QPointF objectPos = { 0, 0 };
            tileCoords -= objectPos;
            tileCoords.setX(qMax(tileCoords.x(), qreal(-1)));
            tileCoords.setY(qMax(tileCoords.y(), qreal(-1)));
            if (snapToGrid)
                tileCoords = tileCoords.toPoint();
            tileCoords += objectPos;

            return renderer->tileToPixelCoords(tileCoords, 0) - itemPos;
        }
        else if (change == ItemPositionHasChanged) {
            const QPointF newPos = value.toPointF();
            QPointF tileCoords = renderer->pixelToTileCoordsNearest(newPos, 0);
            InGameMapPoint point = { tileCoords.x(), tileCoords.y()};
            mFeatureItem->movePoint(mPointIndex, point);
        }
    }

    return QGraphicsItem::itemChange(change, value);
}

/////

SINGLETON_IMPL(CreateInGameMapPointTool)
SINGLETON_IMPL(CreateInGameMapPolygonTool)
SINGLETON_IMPL(CreateInGameMapPolylineTool)
SINGLETON_IMPL(CreateInGameMapRectangleTool)

CreateInGameMapFeatureTool::CreateInGameMapFeatureTool(Type type)
    : BaseCellSceneTool(QString(),
                        QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                        QKeySequence())
    , mFeatureType(type)
    , mPathItem(nullptr)
{
    switch (mFeatureType) {
    case Type::INVALID:
        break;
    case Type::Point:
        setName(tr("Create InGameMap Point"));
        break;
    case Type::Polygon:
        setName(tr("Create InGameMap Polygon"));
        break;
    case Type::Polyline:
        setName(tr("Create InGameMap LineString"));
        break;
    case Type::Rectangle:
        setName(tr("Create InGameMap Rectangle"));
        break;
    }
}

CreateInGameMapFeatureTool::~CreateInGameMapFeatureTool()
{
    delete mPathItem;
}

void CreateInGameMapFeatureTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : nullptr;
}

void CreateInGameMapFeatureTool::activate()
{
    BaseCellSceneTool::activate();
}

void CreateInGameMapFeatureTool::deactivate()
{
    if (mPathItem != nullptr) {
        mScene->removeItem(mPathItem);
        delete mPathItem;
        mPathItem = nullptr;
    }
    mPolygon.clear();
    BaseCellSceneTool::deactivate();
}

void CreateInGameMapFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mPathItem) {
        mPolygon.clear();
        event->accept();
    }
}

void CreateInGameMapFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        addPoint(event->scenePos());
        updatePathItem();
    }
    if (event->button() == Qt::RightButton) {
        switch (mFeatureType) {
        case Type::Polygon:
            if (mPolygon.size() > 2) {
                InGameMapFeature* feature = new InGameMapFeature(&mScene->cell()->inGameMap());
                InGameMapCoordinates coords;
                for (QPointF& point : mPolygon)
                    coords += InGameMapPoint(point.x(), point.y());
                feature->mGeometry.mType = QLatin1Literal("Polygon");
                feature->mGeometry.mCoordinates += coords;
                mScene->worldDocument()->addInGameMapFeature(mScene->cell(), mScene->cell()->inGameMap().mFeatures.size(), feature);
            }
            break;
        case Type::Polyline:
            if (mPolygon.size() > 1) {
                InGameMapFeature* feature = new InGameMapFeature(&mScene->cell()->inGameMap());
                InGameMapCoordinates coords;
                for (QPointF& point : mPolygon)
                    coords += InGameMapPoint(point.x(), point.y());
                feature->mGeometry.mType = QLatin1Literal("LineString");
                feature->mGeometry.mCoordinates += coords;
                mScene->worldDocument()->addInGameMapFeature(mScene->cell(), mScene->cell()->inGameMap().mFeatures.size(), feature);
            }
            break;
        default:
            break;
        }
        mPolygon.clear();
        updatePathItem();
    }
}

void CreateInGameMapFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScenePos = event->scenePos();
    updatePathItem();
}

void CreateInGameMapFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        switch (mFeatureType) {
        case Type::Rectangle:
        {
            if (mPolygon.isEmpty())
                break;
            QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(event->scenePos());
            if (tilePos != mPolygon.first()) {
                // Click-drag-release
                addPoint(event->scenePos());
            }
            break;
        }
        default:
            break;
        }
    }
}

void CreateInGameMapFeatureTool::updatePathItem()
{
    QPainterPath path;

    if (mFeatureType == Type::Polygon) {
        if (mPolygon.size() > 2) {
            path.addPolygon(mScene->renderer()->tileToPixelCoords(mPolygon));
        }
    }

    if (mPolygon.isEmpty()) {
        QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mScenePos);
        QPointF scenePos = mScene->renderer()->tileToPixelCoords(tilePos);
        path.addRect(scenePos.x() - 5, scenePos.y() - 5, 10, 10);
    } else {
        for (QPointF& point : mPolygon) {
            QPointF p = mScene->renderer()->tileToPixelCoords(point);
            path.addRect(p.x() - 5, p.y() - 5, 10, 10);
        }

        if (mFeatureType == Type::Rectangle) {
            QPointF p2 = mScene->renderer()->pixelToTileCoordsNearest(mScenePos);
            int minX = std::min(mPolygon[0].x(), p2.x());
            int minY = std::min(mPolygon[0].y(), p2.y());
            int maxX = std::max(mPolygon[0].x(), p2.x());
            int maxY = std::max(mPolygon[0].y(), p2.y());
            QPolygonF screenPoly = mScene->renderer()->tileToPixelCoords(QRect(minX, minY, maxX - minX, maxY - minY));
            screenPoly += screenPoly.first();
            path.addPolygon(screenPoly);
        } else {
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
    }

    if (mPathItem == nullptr) {
        mPathItem = new QGraphicsPathItem();
        QPen pen(Qt::blue);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);
        pen.setCosmetic(true);
        mPathItem->setPen(pen);
        mScene->addItem(mPathItem);
    }

    mPathItem->setPath(path);
}

void CreateInGameMapFeatureTool::addPoint(const QPointF &scenePos)
{
    if (mFeatureType == Type::Point) {
        InGameMapFeature* feature = new InGameMapFeature(&mScene->cell()->inGameMap());
        QPointF cellPos = mScene->renderer()->pixelToTileCoordsNearest(scenePos);
        InGameMapCoordinates coords;
        coords += InGameMapPoint(cellPos.x(), cellPos.y());
        feature->mGeometry.mType = QLatin1Literal("Point");
        feature->mGeometry.mCoordinates += coords;
        mScene->worldDocument()->addInGameMapFeature(mScene->cell(), mScene->cell()->inGameMap().mFeatures.size(), feature);
        return;
    }
    if (mFeatureType == Type::Rectangle) {
        if (mPolygon.size() == 1) {
            InGameMapFeature* feature = new InGameMapFeature(&mScene->cell()->inGameMap());
            QPointF cellPos = mScene->renderer()->pixelToTileCoordsNearest(scenePos);
            InGameMapCoordinates coords;
            int minX = std::min(mPolygon[0].x(), cellPos.x());
            int minY = std::min(mPolygon[0].y(), cellPos.y());
            int maxX = std::max(mPolygon[0].x(), cellPos.x());
            int maxY = std::max(mPolygon[0].y(), cellPos.y());
            coords += InGameMapPoint(minX, minY);
            coords += InGameMapPoint(maxX, minY);
            coords += InGameMapPoint(maxX, maxY);
            coords += InGameMapPoint(minX, maxY);
            feature->mGeometry.mType = QLatin1Literal("Polygon");
            feature->mGeometry.mCoordinates += coords;
            mScene->worldDocument()->addInGameMapFeature(mScene->cell(), mScene->cell()->inGameMap().mFeatures.size(), feature);
            mPolygon.clear();
            updatePathItem();
            return;
        }
    }

    mPolygon += mScene->renderer()->pixelToTileCoordsNearest(scenePos);
}

/////

SINGLETON_IMPL(EditInGameMapFeatureTool)

EditInGameMapFeatureTool::EditInGameMapFeatureTool()
    : BaseCellSceneTool(tr("Edit InGameMap Features"),
                         QIcon(QLatin1String(":/images/24x24/tool-edit-polygons.png")),
                         QKeySequence())
    , mSelectedFeatureItem(nullptr)
    , mSelectedFeature(nullptr)
{
}

EditInGameMapFeatureTool::~EditInGameMapFeatureTool()
{
}

void EditInGameMapFeatureTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene) {
        mScene->worldDocument()->disconnect(this);
        mScene->document()->disconnect(this);
    }

    mScene = scene ? scene->asCellScene() : nullptr;

    if (mScene) {
        connect(mScene->worldDocument(), &WorldDocument::inGameMapFeatureAboutToBeRemoved,
                this, &EditInGameMapFeatureTool::featureAboutToBeRemoved);
        connect(mScene->worldDocument(), &WorldDocument::inGameMapPointMoved,
                this, &EditInGameMapFeatureTool::featurePointMoved);
        connect(mScene->worldDocument(), &WorldDocument::inGameMapGeometryChanged,
                this, &EditInGameMapFeatureTool::geometryChanged);

        connect(mScene->document(), &CellDocument::selectedInGameMapFeaturesChanged,
                this, &EditInGameMapFeatureTool::selectedFeaturesChanged);
    }
}

void EditInGameMapFeatureTool::activate()
{
    BaseCellSceneTool::activate();
}

void EditInGameMapFeatureTool::deactivate()
{
    if (mSelectedFeatureItem) {
        mSelectedFeatureItem->setEditable(false);
        for (FeatureHandle* handle : mHandles) {
            mScene->removeItem(handle);
            delete handle;
        }
        mHandles.clear();
        mSelectedFeatureItem = nullptr;
        mSelectedFeature = nullptr;
        mScene->document()->setSelectedInGameMapPoints(QList<int>());
    }

    BaseCellSceneTool::deactivate();
}

void EditInGameMapFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape)) {
        event->accept();
    }
}

void EditInGameMapFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        InGameMapFeatureItem* clickedItem = nullptr;
        for (QGraphicsItem *item : mScene->items(event->scenePos())) {
            if (InGameMapFeatureItem *featureItem = dynamic_cast<InGameMapFeatureItem*>(item)) {
                clickedItem = featureItem;
                break;
            }
        }
        if ((clickedItem != nullptr) && (clickedItem == mSelectedFeatureItem)) {
            if (mSelectedFeatureItem->mAddPointIndex != -1) {
                QPointF tilePos = mScene->renderer()->pixelToTileCoordsNearest(mSelectedFeatureItem->mAddPointPos);
                InGameMapPoint point(tilePos.x(), tilePos.y());
                InGameMapCoordinates coords = mSelectedFeature->mGeometry.mCoordinates[0];
                coords.insert(mSelectedFeatureItem->mAddPointIndex + 1, point);
                mScene->worldDocument()->setInGameMapCoordinates(mScene->cell(), mSelectedFeature->index(), 0, coords);
            }
            return;
        }
        if (clickedItem == nullptr)
            mScene->setSelectedInGameMapFeatureItems(QSet<InGameMapFeatureItem*>());
        else
            mScene->setSelectedInGameMapFeatureItems(QSet<InGameMapFeatureItem*>() << clickedItem);
        mScene->document()->setSelectedInGameMapPoints(QList<int>());
    }
    if (event->button() == Qt::RightButton) {
    }
}

void EditInGameMapFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
}

void EditInGameMapFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

// The feature being edited could be deleted via undo/redo.
void EditInGameMapFeatureTool::featureAboutToBeRemoved(WorldCell* cell, int featureIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
        setSelectedItem(nullptr);
        mScene->document()->setSelectedInGameMapPoints(QList<int>());
    }
}

void EditInGameMapFeatureTool::featurePointMoved(WorldCell* cell, int featureIndex, int pointIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
//        mSelectedFeatureItem->synchWithFeature();
        setSelectedItem(mSelectedFeatureItem);
    }
}

void EditInGameMapFeatureTool::geometryChanged(WorldCell *cell, int featureIndex)
{
    InGameMapFeature* feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
        setSelectedItem(mSelectedFeatureItem);
    }
}

void EditInGameMapFeatureTool::selectedFeaturesChanged()
{
    auto& selected = mScene->document()->selectedInGameMapFeatures();
    if (selected.size() == 1) {
        InGameMapFeature* feature = selected.first();
        if (InGameMapFeatureItem* item = mScene->itemForInGameMapFeature(feature)) {
            setSelectedItem(item);
        }
    } else {
        setSelectedItem(nullptr);
    }
}

void EditInGameMapFeatureTool::setSelectedItem(InGameMapFeatureItem *featureItem)
{
    if (mSelectedFeatureItem) {
        // The item may have been deleted if inside featureAboutToBeRemoved()
        if (mScene->itemForInGameMapFeature(mSelectedFeature)) {
            for (auto* handle : mHandles) {
                mScene->removeItem(handle);
                delete handle;
            }
            mSelectedFeatureItem->setEditable(false);
            mSelectedFeatureItem->setZValue(CellScene::ZVALUE_ROADITEM_UNSELECTED);
        }
        mHandles.clear();
        mSelectedFeatureItem = nullptr;
        mSelectedFeature = nullptr;
    }

    if (featureItem) {
        featureItem->mSyncing = true;

        auto createHandle = [&](int pointIndex) {
            FeatureHandle* handle = new FeatureHandle(featureItem, pointIndex);
            InGameMapPoint point = handle->geometryPoint();
            handle->setPos(mScene->renderer()->tileToPixelCoords(point.x, point.y));
//            mScene->addItem(handle);
            mHandles += handle;
        };
        switch (featureItem->geometryType()) {
        case InGameMapFeatureItem::Type::INVALID:
            break;
        case InGameMapFeatureItem::Type::Point:
            for (auto& coords : featureItem->feature()->mGeometry.mCoordinates) {
                for (int i = 0; i < coords.size(); i++)
                    createHandle(i);
                break; // should be only one set of coordinates
            }
            break;
        case InGameMapFeatureItem::Type::Polygon:
            for (auto& coords : featureItem->feature()->mGeometry.mCoordinates) {
                for (int i = 0; i < coords.size(); i++)
                    createHandle(i);
                break; // TODO: handle holes
            }
            break;
        case InGameMapFeatureItem::Type::Polyline:
            for (auto& coords : featureItem->feature()->mGeometry.mCoordinates) {
                for (int i = 0; i < coords.size(); i++)
                    createHandle(i);
                break; // should be only one set of coordinates
            }
            break;
        }

        mSelectedFeatureItem = featureItem;
        mSelectedFeature = featureItem->feature();
        mSelectedFeatureItem->setEditable(true);
        mSelectedFeatureItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);

        featureItem->mSyncing = false;
    }
}
