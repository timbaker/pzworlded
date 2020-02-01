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

#include "mapboxscene.h"

#include "mapboxundo.h"
#include "worldcellmapbox.h"

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

MapboxFeatureItem::MapboxFeatureItem(MapBoxFeature* feature, CellScene *scene, QGraphicsItem *parent)
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

QRectF MapboxFeatureItem::boundingRect() const
{
    return mBoundingRect;
}

void MapboxFeatureItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
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
        painter->drawPolygon(screenPolygon);

        pen.setColor(color);
        painter->setPen(pen);
        painter->setBrush(brush);
        screenPolygon.translate(0, -1);
        painter->drawPolygon(screenPolygon);
        break;
    case Type::Polyline:
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

void MapboxFeatureItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
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

void MapboxFeatureItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
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

void MapboxFeatureItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (--mHoverRefCount == 0) {
        mAddPointIndex = -1;
        update();
    }
}

QPainterPath MapboxFeatureItem::shape() const
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

    QPainterPathStroker stroker;
    stroker.setWidth(20 / zoom);
    return stroker.createStroke(path);
}

bool MapboxFeatureItem::contains(const QPointF &point) const
{
    return QGraphicsItem::contains(point);
}

void MapboxFeatureItem::synchWithFeature()
{
    mPolygon.clear();
    mAddPointIndex = -1;

    switch (geometryType()) {
    case Type::INVALID:
        break;
    case Type::Point: {
        MapBoxPoint center = mFeature->mGeometry.mCoordinates[0][0];
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
        for (auto& coords : mFeature->mGeometry.mCoordinates) {
            for (auto& point : coords) {
                mPolygon += QPointF(point.x, point.y);
            }
            break; // TODO: handle inner rings
        }
        break;
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

void MapboxFeatureItem::setSelected(bool selected)
{
    mIsSelected = selected;
    update();
}

void MapboxFeatureItem::movePoint(int pointIndex, const MapBoxPoint &point)
{
    auto& coords = mFeature->mGeometry.mCoordinates[0];
    coords[pointIndex] = point;
    synchWithFeature();
}

void MapboxFeatureItem::setEditable(bool editable)
{
    mIsEditable = editable;
    update();
}

MapboxFeatureItem::Type MapboxFeatureItem::geometryType() const
{
    if (isPoint()) return Type::Point;
    if (isPolygon()) return Type::Polygon;
    if (isPolyline()) return Type::Polyline;
    return Type::INVALID;
}

bool MapboxFeatureItem::isPoint() const
{
    return mFeature->mGeometry.isPoint();
}

bool MapboxFeatureItem::isPolygon() const
{
    return mFeature->mGeometry.isPolygon();
}

bool MapboxFeatureItem::isPolyline() const
{
    return mFeature->mGeometry.isLineString();
}

/////

class FeatureHandle : public QGraphicsItem
{
public:
    FeatureHandle(MapboxFeatureItem *featureItem, int pointIndex)
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

    MapBoxPoint geometryPoint() const {
        auto& coords = mFeatureItem->feature()->mGeometry.mCoordinates;
        if (coords.isEmpty() || mPointIndex < 0 || mPointIndex >= coords[0].size())
            return { -1, -1 };
        return coords[0].at(mPointIndex);
    }

    bool isSelected() const {
        CellScene *scene = static_cast<CellScene*>(this->scene());
        return scene->document()->selectedMapboxPoints().contains(mPointIndex);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

protected:
    MapboxFeatureItem *mFeatureItem;
    int mPointIndex;
    MapBoxPoint mOldPos;
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
        QList<int> selection = scene->document()->selectedMapboxPoints();
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
        scene->document()->setSelectedMapboxPoints(selection);
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
        MapBoxPoint newPos = geometryPoint();
        mFeatureItem->feature()->mGeometry.mCoordinates[0][mPointIndex] = mOldPos;
        if (mMoveAllPoints) {
            MapBoxCoordinates coords = mFeatureItem->feature()->mGeometry.mCoordinates[0];
            coords.translate(int(newPos.x - mOldPos.x), int(newPos.y - mOldPos.y));
            QUndoCommand *cmd = new SetMapboxCoordinates(document, mFeatureItem->feature()->cell(), featureIndex, 0, coords);
            document->undoStack()->push(cmd);
        } else {
            QUndoCommand *cmd = new MoveMapboxPoint(document, mFeatureItem->feature()->cell(), featureIndex, mPointIndex, newPos);
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
            QPointF tileCoords = renderer->pixelToTileCoords(pixelPos, 0);

            const QPointF objectPos = { 0, 0 };
            tileCoords -= objectPos;
            tileCoords.setX(qMax(tileCoords.x(), qreal(0)));
            tileCoords.setY(qMax(tileCoords.y(), qreal(0)));
            if (snapToGrid)
                tileCoords = tileCoords.toPoint();
            tileCoords += objectPos;

            return renderer->tileToPixelCoords(tileCoords, 0) - itemPos;
        }
        else if (change == ItemPositionHasChanged) {
            const QPointF newPos = value.toPointF();
            QPointF tileCoords = renderer->pixelToTileCoords(newPos, 0);
            MapBoxPoint point = { tileCoords.x(), tileCoords.y()};
            mFeatureItem->movePoint(mPointIndex, point);
        }
    }

    return QGraphicsItem::itemChange(change, value);
}

/////

SINGLETON_IMPL(CreateMapboxPointTool)
SINGLETON_IMPL(CreateMapboxPolygonTool)
SINGLETON_IMPL(CreateMapboxPolylineTool)

CreateMapboxFeatureTool::CreateMapboxFeatureTool(MapboxFeatureItem::Type type)
    : BaseCellSceneTool(QString(),
                        QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                        QKeySequence())
    , mFeatureType(type)
    , mPathItem(nullptr)
{
    switch (mFeatureType) {
    case MapboxFeatureItem::Type::INVALID:
        break;
    case MapboxFeatureItem::Type::Point:
        setName(tr("Create Mapbox Point"));
        break;
    case MapboxFeatureItem::Type::Polygon:
        setName(tr("Create Mapbox Polygon"));
        break;
    case MapboxFeatureItem::Type::Polyline:
        setName(tr("Create Mapbox LineString"));
        break;
    }
}

CreateMapboxFeatureTool::~CreateMapboxFeatureTool()
{
    delete mPathItem;
}

void CreateMapboxFeatureTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : nullptr;
}

void CreateMapboxFeatureTool::activate()
{
    BaseCellSceneTool::activate();
}

void CreateMapboxFeatureTool::deactivate()
{
    if (mPathItem != nullptr) {
        mScene->removeItem(mPathItem);
        delete mPathItem;
        mPathItem = nullptr;
    }
    mPolygon.clear();
    BaseCellSceneTool::deactivate();
}

void CreateMapboxFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mPathItem) {
        mPolygon.clear();
        event->accept();
    }
}

void CreateMapboxFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        addPoint(event->scenePos());
        updatePathItem();
    }
    if (event->button() == Qt::RightButton) {
        switch (mFeatureType) {
        case MapboxFeatureItem::Type::Polygon:
            if (mPolygon.size() > 2) {
                MapBoxFeature* feature = new MapBoxFeature(&mScene->cell()->mapBox());
                MapBoxCoordinates coords;
                for (QPointF& point : mPolygon)
                    coords += MapBoxPoint(point.x(), point.y());
                feature->mGeometry.mType = QLatin1Literal("Polygon");
                feature->mGeometry.mCoordinates += coords;
                mScene->worldDocument()->addMapboxFeature(mScene->cell(), mScene->cell()->mapBox().mFeatures.size(), feature);
            }
            break;
        case MapboxFeatureItem::Type::Polyline:
            if (mPolygon.size() > 1) {
                MapBoxFeature* feature = new MapBoxFeature(&mScene->cell()->mapBox());
                MapBoxCoordinates coords;
                for (QPointF& point : mPolygon)
                    coords += MapBoxPoint(point.x(), point.y());
                feature->mGeometry.mType = QLatin1Literal("LineString");
                feature->mGeometry.mCoordinates += coords;
                mScene->worldDocument()->addMapboxFeature(mScene->cell(), mScene->cell()->mapBox().mFeatures.size(), feature);
            }
            break;
        }
        mPolygon.clear();
        updatePathItem();
    }
}

void CreateMapboxFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mScenePos = event->scenePos();
    updatePathItem();
}

void CreateMapboxFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

void CreateMapboxFeatureTool::updatePathItem()
{
    QPainterPath path;

    if (mFeatureType == MapboxFeatureItem::Type::Polygon) {
        if (mPolygon.size() > 2) {
            path.addPolygon(mScene->renderer()->tileToPixelCoords(mPolygon));
        }
    }

    if (mPolygon.isEmpty()) {
        QPointF tilePos = mScene->renderer()->pixelToTileCoordsInt(mScenePos);
        QPointF scenePos = mScene->renderer()->tileToPixelCoords(tilePos);
        path.addRect(scenePos.x() - 5, scenePos.y() - 5, 10, 10);
    } else {
        for (QPointF& point : mPolygon) {
            QPointF p = mScene->renderer()->tileToPixelCoords(point);
            path.addRect(p.x() - 5, p.y() - 5, 10, 10);
        }
    }
    if (!mPolygon.isEmpty()) {
        QPointF p1 = mScene->renderer()->tileToPixelCoords(mPolygon[0]);
        path.moveTo(p1);
        for (int i = 1; i < mPolygon.size(); i++) {
            QPointF p2 = mScene->renderer()->tileToPixelCoords(mPolygon[i]);
            path.lineTo(p2);
        }

        // Line to mouse pointer
        QPointF p2 = mScene->renderer()->pixelToTileCoordsInt(mScenePos);
        p2 = mScene->renderer()->tileToPixelCoords(p2);
        path.lineTo(p2);
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

void CreateMapboxFeatureTool::addPoint(const QPointF &scenePos)
{
    if (mFeatureType == MapboxFeatureItem::Type::Point) {
        MapBoxFeature* feature = new MapBoxFeature(&mScene->cell()->mapBox());
        QPointF cellPos = mScene->renderer()->pixelToTileCoordsInt(scenePos);
        MapBoxCoordinates coords;
        coords += MapBoxPoint(cellPos.x(), cellPos.y());
        feature->mGeometry.mType = QLatin1Literal("Point");
        feature->mGeometry.mCoordinates += coords;
        mScene->worldDocument()->addMapboxFeature(mScene->cell(), mScene->cell()->mapBox().mFeatures.size(), feature);
        return;
    }

    mPolygon += mScene->renderer()->pixelToTileCoordsInt(scenePos);
}

/////

SINGLETON_IMPL(EditMapboxFeatureTool)

EditMapboxFeatureTool::EditMapboxFeatureTool()
    : BaseCellSceneTool(tr("Edit Mapbox Features"),
                         QIcon(QLatin1String(":/images/24x24/tool-edit-polygons.png")),
                         QKeySequence())
    , mSelectedFeatureItem(nullptr)
    , mSelectedFeature(nullptr)
{
}

EditMapboxFeatureTool::~EditMapboxFeatureTool()
{
}

void EditMapboxFeatureTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene) {
        mScene->worldDocument()->disconnect(this);
        mScene->document()->disconnect(this);
    }

    mScene = scene ? scene->asCellScene() : nullptr;

    if (mScene) {
        connect(mScene->worldDocument(), &WorldDocument::mapboxFeatureAboutToBeRemoved,
                this, &EditMapboxFeatureTool::featureAboutToBeRemoved);
        connect(mScene->worldDocument(), &WorldDocument::mapboxPointMoved,
                this, &EditMapboxFeatureTool::featurePointMoved);
        connect(mScene->worldDocument(), &WorldDocument::mapboxGeometryChanged,
                this, &EditMapboxFeatureTool::geometryChanged);

        connect(mScene->document(), &CellDocument::selectedMapboxFeaturesChanged,
                this, &EditMapboxFeatureTool::selectedFeaturesChanged);
    }
}

void EditMapboxFeatureTool::activate()
{
    BaseCellSceneTool::activate();
}

void EditMapboxFeatureTool::deactivate()
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
        mScene->document()->setSelectedMapboxPoints(QList<int>());
    }

    BaseCellSceneTool::deactivate();
}

void EditMapboxFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape)) {
        event->accept();
    }
}

void EditMapboxFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        MapboxFeatureItem* clickedItem = nullptr;
        for (QGraphicsItem *item : mScene->items(event->scenePos())) {
            if (MapboxFeatureItem *featureItem = dynamic_cast<MapboxFeatureItem*>(item)) {
                clickedItem = featureItem;
                break;
            }
        }
        if ((clickedItem != nullptr) && (clickedItem == mSelectedFeatureItem)) {
            if (mSelectedFeatureItem->mAddPointIndex != -1) {
                QPointF tilePos = mScene->renderer()->pixelToTileCoordsInt(mSelectedFeatureItem->mAddPointPos);
                MapBoxPoint point(tilePos.x(), tilePos.y());
                MapBoxCoordinates coords = mSelectedFeature->mGeometry.mCoordinates[0];
                coords.insert(mSelectedFeatureItem->mAddPointIndex + 1, point);
                mScene->worldDocument()->setMapboxCoordinates(mScene->cell(), mSelectedFeature->index(), 0, coords);
            }
            return;
        }
        if (clickedItem == nullptr)
            mScene->setSelectedMapboxFeatureItems(QSet<MapboxFeatureItem*>());
        else
            mScene->setSelectedMapboxFeatureItems(QSet<MapboxFeatureItem*>() << clickedItem);
        mScene->document()->setSelectedMapboxPoints(QList<int>());
    }
    if (event->button() == Qt::RightButton) {
    }
}

void EditMapboxFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
}

void EditMapboxFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

// The feature being edited could be deleted via undo/redo.
void EditMapboxFeatureTool::featureAboutToBeRemoved(WorldCell* cell, int featureIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
        setSelectedItem(nullptr);
        mScene->document()->setSelectedMapboxPoints(QList<int>());
    }
}

void EditMapboxFeatureTool::featurePointMoved(WorldCell* cell, int featureIndex, int pointIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
//        mSelectedFeatureItem->synchWithFeature();
        setSelectedItem(mSelectedFeatureItem);
    }
}

void EditMapboxFeatureTool::geometryChanged(WorldCell *cell, int featureIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(featureIndex);
    if (feature == mSelectedFeature) {
        setSelectedItem(mSelectedFeatureItem);
    }
}

void EditMapboxFeatureTool::selectedFeaturesChanged()
{
    auto& selected = mScene->document()->selectedMapboxFeatures();
    if (selected.size() == 1) {
        MapBoxFeature* feature = selected.first();
        if (MapboxFeatureItem* item = mScene->itemForMapboxFeature(feature)) {
            setSelectedItem(item);
        }
    } else {
        setSelectedItem(nullptr);
    }
}

void EditMapboxFeatureTool::setSelectedItem(MapboxFeatureItem *featureItem)
{
    if (mSelectedFeatureItem) {
        // The item may have been deleted if inside featureAboutToBeRemoved()
        if (mScene->itemForMapboxFeature(mSelectedFeature)) {
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
            MapBoxPoint point = handle->geometryPoint();
            handle->setPos(mScene->renderer()->tileToPixelCoords(point.x, point.y));
//            mScene->addItem(handle);
            mHandles += handle;
        };
        switch (featureItem->geometryType()) {
        case MapboxFeatureItem::Type::INVALID:
            break;
        case MapboxFeatureItem::Type::Point:
            for (auto& coords : featureItem->feature()->mGeometry.mCoordinates) {
                for (int i = 0; i < coords.size(); i++)
                    createHandle(i);
                break; // should be only one set of coordinates
            }
            break;
        case MapboxFeatureItem::Type::Polygon:
            for (auto& coords : featureItem->feature()->mGeometry.mCoordinates) {
                for (int i = 0; i < coords.size(); i++)
                    createHandle(i);
                break; // exterior loop only
            }
            break;
        case MapboxFeatureItem::Type::Polyline:
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
