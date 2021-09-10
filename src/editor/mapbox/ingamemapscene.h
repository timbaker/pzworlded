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

#ifndef INGAMEMAPSCENE_H
#define INGAMEMAPSCENE_H

#include "scenetools.h"

class CellScene;
class WorldDocument;

class InGameMapFeature;
class InGameMapPoint;

namespace Tiled {
class MapRenderer;
}

class InGameMapFeatureItem : public QGraphicsItem
{
public:
    enum class Type {
        INVALID,
        Point,
        Polygon,
        Polyline
    };

    InGameMapFeatureItem(InGameMapFeature* feature, CellScene *scene, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    QPainterPath shape() const;
    bool contains(const QPointF &point) const;

    void setEditable(bool editable);
    bool isEditable() const { return mIsEditable; }

    /**
      * Note: QGraphicsItem already defines these, but I'm avoiding any
      * QGraphicsScene() selection behavior.
      */
    void setSelected(bool selected);
    bool isSelected() const { return mIsSelected; }

    void movePoint(int pointIndex, const InGameMapPoint& point);

    void synchWithFeature();

    InGameMapFeature* feature() const { return mFeature; }

    Type geometryType() const;
    bool isPoint() const;
    bool isPolygon() const;
    bool isPolyline() const;

protected:
    friend class FeatureHandle;
    friend class EditInGameMapFeatureTool;

    WorldDocument* mWorldDoc;
    InGameMapFeature* mFeature;
    QPolygonF mPolygon;
    QList<QPolygonF> mHoles;
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

class CreateInGameMapFeatureTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    enum class Type
    {
        INVALID,
        Point,
        Polygon,
        Polyline,
        Rectangle,
    };

    explicit CreateInGameMapFeatureTool(Type type);
    ~CreateInGameMapFeatureTool();

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
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void updatePathItem();
    void addPoint(const QPointF& scenePos);

    Type mFeatureType;
    QGraphicsPathItem* mPathItem;
    QPolygonF mPolygon;
    QPointF mScenePos;
};

class CreateInGameMapPointTool : public CreateInGameMapFeatureTool, public Singleton<CreateInGameMapPointTool>
{
    Q_OBJECT

public:
    CreateInGameMapPointTool()
        : CreateInGameMapFeatureTool(Type::Point)
    {
        setIcon(QIcon(QLatin1Literal(":/images/22x22/road-tool-edit.png")));
    }
};

class CreateInGameMapPolygonTool : public CreateInGameMapFeatureTool, public Singleton<CreateInGameMapPolygonTool>
{
    Q_OBJECT

public:
    CreateInGameMapPolygonTool()
        : CreateInGameMapFeatureTool(Type::Polygon)
    {
        setIcon(QIcon(QLatin1Literal(":/images/24x24/insert-polygon.png")));
    }
};

class CreateInGameMapPolylineTool : public CreateInGameMapFeatureTool, public Singleton<CreateInGameMapPolylineTool>
{
    Q_OBJECT

public:
    CreateInGameMapPolylineTool()
        : CreateInGameMapFeatureTool(Type::Polyline)
    {
        setIcon(QIcon(QLatin1Literal(":/images/24x24/insert-polyline.png")));
    }
};

class CreateInGameMapRectangleTool : public CreateInGameMapFeatureTool, public Singleton<CreateInGameMapRectangleTool>
{
    Q_OBJECT

public:
    CreateInGameMapRectangleTool()
        : CreateInGameMapFeatureTool(Type::Rectangle)
    {
        setIcon(QIcon(QLatin1Literal(":/images/24x24/insert-polygon.png")));
    }
};

class EditInGameMapFeatureTool : public BaseCellSceneTool, public Singleton<EditInGameMapFeatureTool>
{
    Q_OBJECT

public:
    explicit EditInGameMapFeatureTool();
    ~EditInGameMapFeatureTool();

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
        setName(tr("Edit InGameMap Features"));
        //setShortcut(QKeySequence(tr("S")));
    }

private slots:
    void featureAboutToBeRemoved(WorldCell* cell, int featureIndex);
    void featurePointMoved(WorldCell* cell, int featureIndex, int pointIndex);
    void geometryChanged(WorldCell* cell, int featureIndex);
    void selectedFeaturesChanged();

private:
    void setSelectedItem(InGameMapFeatureItem* feature);

    InGameMapFeatureItem *mSelectedFeatureItem;
    InGameMapFeature *mSelectedFeature;
    QList<FeatureHandle*> mHandles;
};

#endif // INGAMEMAPSCENE_H
