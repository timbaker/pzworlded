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

#ifndef MAPBOXSCENE_H
#define MAPBOXSCENE_H

#include "scenetools.h"

class CellScene;
class WorldDocument;

class MapBoxFeature;
class MapBoxPoint;

namespace Tiled {
class MapRenderer;
}

class MapboxFeatureItem : public QGraphicsItem
{
public:
    enum class Type {
        INVALID,
        Point,
        Polygon,
        Polyline
    };

    MapboxFeatureItem(MapBoxFeature* feature, CellScene *scene, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    QPainterPath shape() const;

    void setEditable(bool editable);
    bool isEditable() const { return mIsEditable; }

    /**
      * Note: QGraphicsItem already defines these, but I'm avoiding any
      * QGraphicsScene() selection behavior.
      */
    void setSelected(bool selected);
    bool isSelected() const { return mIsSelected; }

    void movePoint(int pointIndex, const MapBoxPoint& point);

    void synchWithFeature();

    MapBoxFeature* feature() const { return mFeature; }

    Type geometryType() const;
    bool isPoint() const;
    bool isPolygon() const;
    bool isPolyline() const;

protected:
    friend class FeatureHandle;
    friend class EditMapboxFeatureTool;

    WorldDocument* mWorldDoc;
    MapBoxFeature* mFeature;
    QPolygonF mPolygon;
    Tiled::MapRenderer *mRenderer;
    bool mSyncing;
    bool mIsEditable;
    bool mIsSelected;
    QRectF mBoundingRect;
    int mHoverRefCount;
    QPointF mDragOffset;
    int mAddPointIndex = -1;
    QPointF mAddPointPos;
};

class FeatureHandle;

class CreateMapboxFeatureTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    explicit CreateMapboxFeatureTool(MapboxFeatureItem::Type type);
    ~CreateMapboxFeatureTool();

    void setScene(BaseGraphicsScene *scene);

    void activate();
    void deactivate();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    bool affectsLots() const { return false; }
    bool affectsObjects() const { return false; }

    void languageChanged() {
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
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void updatePathItem();
    void addPoint(const QPointF& scenePos);

    MapboxFeatureItem::Type mFeatureType;
    QGraphicsPathItem* mPathItem;
    QPolygonF mPolygon;
    QPointF mScenePos;
};

class CreateMapboxPolygonTool : public CreateMapboxFeatureTool, public Singleton<CreateMapboxPolygonTool>
{
    Q_OBJECT

public:
    CreateMapboxPolygonTool()
        : CreateMapboxFeatureTool(MapboxFeatureItem::Type::Polygon)
    {
    }
};

class CreateMapboxPolylineTool : public CreateMapboxFeatureTool, public Singleton<CreateMapboxPolylineTool>
{
    Q_OBJECT

public:
    CreateMapboxPolylineTool()
        : CreateMapboxFeatureTool(MapboxFeatureItem::Type::Polyline)
    {
    }
};

class EditMapboxFeatureTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    static EditMapboxFeatureTool *instance();
    static void deleteInstance();

    explicit EditMapboxFeatureTool();
    ~EditMapboxFeatureTool();

    void setScene(BaseGraphicsScene *scene);

    void activate();
    void deactivate();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    bool affectsLots() const { return false; }
    bool affectsObjects() const { return false; }

    void languageChanged()
    {
        setName(tr("Edit Mapbox Features"));
        //setShortcut(QKeySequence(tr("S")));
    }

private slots:
    void featureAboutToBeRemoved(WorldCell* cell, int featureIndex);
    void featurePointMoved(WorldCell* cell, int featureIndex, int pointIndex);
    void geometryChanged(WorldCell* cell, int featureIndex);
    void selectedFeaturesChanged();

private:
    Q_DISABLE_COPY(EditMapboxFeatureTool)

    void setSelectedItem(MapboxFeatureItem* feature);

    MapboxFeatureItem *mSelectedFeatureItem;
    MapBoxFeature *mSelectedFeature;
    QList<FeatureHandle*> mHandles;

    static EditMapboxFeatureTool *mInstance;
};

#endif // MAPBOXSCENE_H
