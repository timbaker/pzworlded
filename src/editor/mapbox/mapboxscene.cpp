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
    if (++mHoverRefCount == 1) {
        update();
    }
}

void MapboxFeatureItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (--mHoverRefCount == 0) {
        update();
    }
}

QPainterPath MapboxFeatureItem::shape() const
{
    QPolygonF polygon = mRenderer->tileToPixelCoords(mPolygon, 0);

    if (isPolygon())
        polygon += polygon[0];

    QPainterPath path;
    path.addPolygon(polygon);
    QPainterPathStroker stroker;
    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    stroker.setWidth(20 / zoom); // TODO: adjust for zoom
    return stroker.createStroke(path);
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

    if (event->button() == Qt::LeftButton && (mOldPos != geometryPoint())) {
        WorldDocument *document = mFeatureItem->mWorldDoc;
        int featureIndex = mFeatureItem->feature()->index();
        MapBoxPoint newPos = geometryPoint();
        mFeatureItem->feature()->mGeometry.mCoordinates[0][mPointIndex] = mOldPos;
        QUndoCommand *cmd = new MoveMapboxPoint(document, mFeatureItem->feature()->cell(), featureIndex, mPointIndex, newPos);
        document->undoStack()->push(cmd);
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
    mPolygon += mScene->renderer()->pixelToTileCoordsInt(scenePos);

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
        foreach (QGraphicsItem *item, mScene->items(event->scenePos())) {
            if (MapboxFeatureItem *featureItem = dynamic_cast<MapboxFeatureItem*>(item)) {
                clickedItem = featureItem;
                break;
            }
        }
        if (clickedItem == mSelectedFeatureItem)
            return;
        if (clickedItem == nullptr)
            mScene->setSelectedMapboxFeatureItems(QSet<MapboxFeatureItem*>());
        else
            mScene->setSelectedMapboxFeatureItems(QSet<MapboxFeatureItem*>() << clickedItem);
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
