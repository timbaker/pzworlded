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
class HeightMapScene;

class HeightMapItem : public QGraphicsItem
{
public:
    HeightMapItem(HeightMapScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    HeightMapScene *mScene;
};

/////

class HeightMapScene : public BaseGraphicsScene
{
public:
    HeightMapScene(HeightMapDocument *hmDoc, QObject *parent = 0);
    ~HeightMapScene();

    void setTool(AbstractTool *tool);

    QPointF toScene(const QPointF &p) const;
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

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

private:
    HeightMapDocument *mDocument;
    HeightMapItem *mHeightMapItem;
    HeightMap *mHeightMap;
    BaseHeightMapTool *mActiveTool;
};

/////

class HeightMapView : public BaseGraphicsView
{
    Q_OBJECT
public:
    explicit HeightMapView(QWidget *parent = 0);
    
    void scrollContentsBy(int dx, int dy);

private slots:
    void recenter();

private:
    HeightMapScene *mScene;
    bool mRecenterScheduled;
};

#endif // HEIGHTMAPVIEW_H
