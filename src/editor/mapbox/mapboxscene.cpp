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

#include "cellscene.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "maprenderer.h"

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
    QColor color = Qt::blue; //

    QColor brushColor = color;
    brushColor.setAlpha(50);
    QBrush brush(brushColor);

    QPen pen(Qt::black);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(2);

    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);

    QPolygonF screenPolygon = mRenderer->tileToPixelCoords(mPolygon);

    switch (geometryType()) {
    case Type::INVALID:
        break;
    case Type::Point:
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

}

void MapboxFeatureItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
}

void MapboxFeatureItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
}

QPainterPath MapboxFeatureItem::shape() const
{
    QPolygonF polygon = mRenderer->tileToPixelCoords(mPolygon, 0);

    QPainterPath path;
    // FIXME: this is a polygon for Polyline which messes with hit-testing
    path.addPolygon(polygon);
    return path;
}

void MapboxFeatureItem::synchWithFeature()
{
    mPolygon.clear();

    switch (geometryType()) {
    case Type::INVALID:
        break;
    case Type::Point:
        break;
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

    QRectF bounds = mRenderer->tileToPixelCoords(mPolygon).boundingRect().adjusted(-2, -3, 2, 2);
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
    return mFeature->mGeometry.mType == QLatin1Literal("Point");
}

bool MapboxFeatureItem::isPolygon() const
{
    return mFeature->mGeometry.mType == QLatin1Literal("Polygon");
}

bool MapboxFeatureItem::isPolyline() const
{
    return mFeature->mGeometry.mType == QLatin1Literal("LineString");
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
    }

    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr);

    MapBoxPoint geometryPoint() const {
        auto& coords = mFeatureItem->feature()->mGeometry.mCoordinates;
        if (coords.isEmpty() || mPointIndex < 0 || mPointIndex >= coords[0].size())
            return { -1, -1 };
        return coords[0].at(mPointIndex);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

protected:
    MapboxFeatureItem *mFeatureItem;
    int mPointIndex;
    MapBoxPoint mOldPos;
};

QRectF FeatureHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void FeatureHandle::paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *,
                   QWidget *)
{
    painter->setBrush(Qt::blue);
    painter->setPen(Qt::black);
    painter->drawRect(QRectF(-5, -5, 10, 10));
}

void FeatureHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);

    if (event->button() == Qt::LeftButton)
        mOldPos = geometryPoint();

    // Stop the object context menu messing us up.
    event->accept();
}

void FeatureHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    // If we resized the object, create an undo command
    if (event->button() == Qt::LeftButton && (mOldPos != geometryPoint())) {
        WorldDocument *document = mFeatureItem->mWorldDoc;
        int featureIndex = mFeatureItem->feature()->index();
        MapBoxPoint newPos = geometryPoint();
        mFeatureItem->feature()->mGeometry.mCoordinates[0][mPointIndex] = mOldPos;
        QUndoCommand *cmd = new MoveMapboxPoint(document, mFeatureItem->feature()->mOwner->mCell, featureIndex, mPointIndex, newPos);
        document->undoStack()->push(cmd);
    }

    // Stop the object context menu messing us up.
    event->accept();
}

QVariant FeatureHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (!mFeatureItem->mSyncing) {
        auto* renderer = mFeatureItem->mRenderer;

        if (change == ItemPositionChange) {
            bool snapToGrid = true;

            // Calculate the absolute pixel position
            const QPointF itemPos = pos();
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

SINGLETON_IMPL(CreateMapboxPolylineTool)

CreateMapboxFeatureTool::CreateMapboxFeatureTool(MapboxFeatureItem::Type type)
    : BaseCellSceneTool(tr("Create Mapbox Features"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                         QKeySequence())
    , mFeatureType(type)
    , mPathItem(nullptr)
{
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
    BaseCellSceneTool::deactivate();
}

void CreateMapboxFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mPathItem) {
        mScene->removeItem(mPathItem);
        delete mPathItem;
        mPathItem = nullptr;
        event->accept();
    }
}

void CreateMapboxFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        addPoint(event->scenePos());
    }
    if (event->button() == Qt::RightButton) {
        switch (mFeatureType) {
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
        if (mPathItem) {
            mScene->removeItem(mPathItem);
            delete mPathItem;
            mPathItem = nullptr;
        }
    }
}

void CreateMapboxFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
}

void CreateMapboxFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }
}

void CreateMapboxFeatureTool::addPoint(const QPointF &scenePos)
{
    if (mPathItem == nullptr) {
        mPathItem = new QGraphicsPathItem();
        QPen pen(Qt::blue);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);
        pen.setCosmetic(true);
        mPathItem->setPen(pen);
        mScene->addItem(mPathItem);
        mPolygon.clear();
    }

    mPolygon += mScene->renderer()->pixelToTileCoordsInt(scenePos);

    QPainterPath path;
    if (mPolygon.size() < 2)
        return;
    QPointF p1 = mScene->renderer()->tileToPixelCoords(mPolygon[0]);
    path.moveTo(p1);
    for (int i = 1; i < mPolygon.size(); i++) {
        QPointF p2 = mScene->renderer()->tileToPixelCoords(mPolygon[i]);
        path.lineTo(p2);
    }
    mPathItem->setPath(path);
}

/////

EditMapboxFeatureTool *EditMapboxFeatureTool::mInstance = nullptr;

EditMapboxFeatureTool *EditMapboxFeatureTool::instance()
{
    if (!mInstance)
        mInstance = new EditMapboxFeatureTool();
    return mInstance;
}

void EditMapboxFeatureTool::deleteInstance()
{
    delete mInstance;
}

EditMapboxFeatureTool::EditMapboxFeatureTool()
    : BaseCellSceneTool(tr("Edit Mapbox Features"),
                         QIcon(QLatin1String(":/images/22x22/road-tool-edit.png")),
                         QKeySequence())
    , mSelectedFeatureItem(nullptr)
    , mFeature(nullptr)
    , mFeatureItem(nullptr)
    , mMoving(false)
    , mHandlesVisible(false)
{
}

EditMapboxFeatureTool::~EditMapboxFeatureTool()
{
    delete mFeatureItem;
    delete mFeature;
}

void EditMapboxFeatureTool::setScene(BaseGraphicsScene *scene)
{
    if (mScene)
        mScene->worldDocument()->disconnect(this);

    mScene = scene ? scene->asCellScene() : nullptr;

    if (mScene) {
        connect(mScene->worldDocument(), SIGNAL(mapboxFeatureAboutToBeRemoved(WorldCell*,int)),
                SLOT(featureAboutToBeRemoved(WorldCell*,int)));
        connect(mScene->worldDocument(), SIGNAL(mapboxPointMoved(WorldCell*,int,int)),
                SLOT(featurePointMoved(WorldCell*,int,int)));
    }
}

void EditMapboxFeatureTool::activate()
{
    BaseCellSceneTool::activate();
}

void EditMapboxFeatureTool::deactivate()
{
    if (mHandlesVisible) {
        for (FeatureHandle* handle : mHandles) {
            mScene->removeItem(handle);
            delete handle;
        }
        mHandles.clear();
        mHandlesVisible = false;
    }
    mSelectedFeatureItem = nullptr;

    BaseCellSceneTool::deactivate();
}

void EditMapboxFeatureTool::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() == Qt::Key_Escape) && mMoving) {
        cancelMoving();
        mMoving = false;
        event->accept();
    }
}

void EditMapboxFeatureTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
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

void EditMapboxFeatureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!mMoving)
        return;
#if 0
    QPoint curPos = mScene->pixelToRoadCoords(event->scenePos());
    QPoint delta = curPos - (mMovingStart
            ? mSelectedFeatureItem->road()->start()
            : mSelectedFeatureItem->road()->end());
    if (mSelectedFeatureItem->road()->isVertical())
        delta.setX(0);
    else
        delta.setY(0);
    if (mMovingStart)
        mFeature->setCoords(mSelectedFeatureItem->road()->start() + delta, mFeature->end());
    else
        mFeature->setCoords(mFeature->start(), mSelectedFeatureItem->road()->end() + delta);
    mFeatureItem->synchWithFeature();
    updateHandles(mFeature);
#endif
}

void EditMapboxFeatureTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (!mMoving)
            return;
#if 0
        if (mFeature->start() == mSelectedFeatureItem->road()->start() &&
                mFeature->end() == mSelectedFeatureItem->road()->end()) {
            cancelMoving();
            mMoving = false;
            return;
        }
#endif
        finishMoving();
        mMoving = false;
    }
}

// The feature being edited could be deleted via undo/redo.
void EditMapboxFeatureTool::featureAboutToBeRemoved(WorldCell* cell, int featureIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(featureIndex);
    if (mSelectedFeatureItem && feature == mSelectedFeatureItem->feature()) {
        if (mMoving) {
            cancelMoving();
            mMoving = false;
        }
        updateHandles(nullptr);
        mSelectedFeatureItem = nullptr;
    }
}

void EditMapboxFeatureTool::featurePointMoved(WorldCell* cell, int featureIndex, int pointIndex)
{
    MapBoxFeature* feature = cell->mapBox().mFeatures.at(featureIndex);
    if (mSelectedFeatureItem && feature == mSelectedFeatureItem->feature()) {
        mSelectedFeatureItem->synchWithFeature();
        updateHandles(mSelectedFeatureItem);
    }
}

void EditMapboxFeatureTool::startMoving(const QPointF &scenePos)
{
    if (mSelectedFeatureItem) {
        QPoint roadPos = mScene->pixelToRoadCoords(scenePos);
        mSelectedFeatureItem->setEditable(false);
        mSelectedFeatureItem->setZValue(mSelectedFeatureItem->isSelected() ?
                                         CellScene::ZVALUE_ROADITEM_SELECTED :
                                         CellScene::ZVALUE_ROADITEM_UNSELECTED);
        mSelectedFeatureItem = nullptr;
        if (mSelectedFeatureItem) {
            mMoving = true;

            mFeature = new MapBoxFeature(&mScene->cell()->mapBox());
            *mFeature = *mSelectedFeatureItem->feature();

            mFeatureItem = new MapboxFeatureItem(mFeature, mScene);
            mFeatureItem->setEditable(true);
            mFeatureItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);
            mScene->addItem(mFeatureItem);
            mSelectedFeatureItem->setVisible(false);
            return;
        }
    }

    foreach (QGraphicsItem *item, mScene->items(scenePos)) {
        if (MapboxFeatureItem *featureItem = dynamic_cast<MapboxFeatureItem*>(item)) {
            mSelectedFeatureItem = featureItem;
            mSelectedFeatureItem->setEditable(true);
            mSelectedFeatureItem->setZValue(CellScene::ZVALUE_ROADITEM_CREATING);
            updateHandles(mSelectedFeatureItem);
            break;
        }
    }
    updateHandles(mSelectedFeatureItem ? mSelectedFeatureItem : nullptr);
}

void EditMapboxFeatureTool::finishMoving()
{
#if 0
    QUndoStack *undoStack = mScene->worldDocument()->undoStack();
    undoStack->push(new ChangeRoadCoords(mScene->worldDocument(),
                                         mSelectedFeatureItem->road(),
                                         mFeature->start(), mFeature->end()));
#endif
    cancelMoving();
}

void EditMapboxFeatureTool::cancelMoving()
{
    mSelectedFeatureItem->setVisible(true);
    updateHandles(mSelectedFeatureItem);

    mScene->removeItem(mFeatureItem);
    delete mFeatureItem;
    mFeatureItem = nullptr;

    delete mFeature;
    mFeature = nullptr;
}

void EditMapboxFeatureTool::updateHandles(MapboxFeatureItem *featureItem)
{
    if (mHandlesVisible) {
        for (auto* handle : mHandles) {
            mScene->removeItem(handle);
            delete handle;
        }
        mHandles.clear();
        mHandlesVisible = false;
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
        mHandlesVisible = true;

        featureItem->mSyncing = false;
    }
}
