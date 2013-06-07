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

#ifndef HEIGHTMAPVIEW_H
#define HEIGHTMAPVIEW_H

#include "basegraphicsscene.h"
#include "basegraphicsview.h"

#include <QGraphicsItem>

class BaseHeightMapTool;
class HeightMap;
class HeightMapDocument;
class MapComposite;
class MapImage;
class WorldCell;
class WorldCellLot;

namespace Tiled {
class Tileset;
}

class HMMiniMapItem;
class HeightMapScene;

class HeightMapItem : public QGraphicsItem
{
public:
    HeightMapItem(HeightMapScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void paint2(QPainter *painter, const QStyleOptionGraphicsItem *option);

    enum DisplayStyle {
        MeshStyle,
        FlatStyle
    };
    void setDisplayStyle(DisplayStyle style)
    {
        mDisplayStyle = style;
        update();
    }

private:
    void loadGLTextures();
    QVector3D vertexAt(int x, int y);
    void draw_triangle(const QVector3D &v0, const QVector3D &v1, const QVector3D &v2);

private:
    HeightMapScene *mScene;
    unsigned int mTextureID;
    Tiled::Tileset *mTileset;
    DisplayStyle mDisplayStyle;
};

/////

// FIXME: nearly identical to CellMiniMapItem
class HMMiniMapItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    HMMiniMapItem(HeightMapScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateCellImage();
    void updateLotImage(int index);
    void updateBoundingRect();

private slots:
    void lotAdded(WorldCell *cell, int index);
    void lotRemoved(WorldCell *cell, int index);
    void lotMoved(WorldCellLot *lot);

    void cellContentsAboutToChange(WorldCell *cell);
    void cellContentsChanged(WorldCell *cell);

    void sceneRectChanged(const QRectF &sceneRect);
    void mapImageChanged(MapImage *mapImage);

private:
    struct LotImage {
        LotImage()
            : mMapImage(0)
        {
        }

        LotImage(const QRectF &bounds, MapImage *mapImage)
            : mBounds(bounds)
            , mMapImage(mapImage)
        {
        }

        QRectF mBounds;
        MapImage *mMapImage;
    };

    HeightMapScene *mScene;
    WorldCell *mCell;
    QRectF mBoundingRect;
    MapImage *mMapImage;
    QRectF mMapImageBounds;
    QVector<LotImage> mLotImages;
};

/////

class HeightMapScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    HeightMapScene(HeightMapDocument *hmDoc, QObject *parent = 0);
    ~HeightMapScene();

    void setTool(AbstractTool *tool);

    QPointF toScene(const QPointF &worldPos) const;
    QPointF toScene(qreal x, qreal y) const;
    QPolygonF toScene(const QRect &rect) const;
    QRect boundingRect(const QRect &rect) const;

    QPointF toWorld(const QPointF &scenePos) const;
    QPointF toWorld(qreal x, qreal y) const;
    QPoint toWorldInt(const QPointF &scenePos) const;

    HeightMapDocument *document() const
    { return mDocument; }

    int tileWidth() const { return 64; }
    int tileHeight() const { return 32; }
    int maxLevel() const { return 1; }

    HeightMap *hm() const
    { return mHeightMap; }

    void setCenter(int x, int y);

    QPoint worldOrigin() const;
    QRect worldBounds() const;

    MapComposite *mapComposite() const
    { return mMapComposite; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

private slots:
    void heightMapPainted(const QRegion &region);
    void heightMapDisplayStyleChanged(int style);

private:
    HeightMapDocument *mDocument;
    HeightMapItem *mHeightMapItem;
    HeightMap *mHeightMap;
    BaseHeightMapTool *mActiveTool;
    MapComposite *mMapComposite;
};

/////

class HeightMapView : public BaseGraphicsView
{
    Q_OBJECT
public:
    explicit HeightMapView(QWidget *parent = 0);
    
    void scrollContentsBy(int dx, int dy);

    void mouseMoveEvent(QMouseEvent *event);

    QRectF sceneRectForMiniMap() const;

    void setScene(HeightMapScene *scene);

    HeightMapScene *scene() const
    { return (HeightMapScene*) BaseGraphicsView::scene(); }

private slots:
    void recenter();

private:
    bool mRecenterScheduled;
};

#endif // HEIGHTMAPVIEW_H
