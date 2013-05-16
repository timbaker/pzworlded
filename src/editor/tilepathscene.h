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

#ifndef TILEPATHSCENE_H
#define TILEPATHSCENE_H

#include "basepathscene.h"

#include "basepathrenderer.h"

namespace Tiled {
class Map;
class MapRenderer;
class ZLevelRenderer;
}

class CompositeLayerGroup;
class MapComposite;
class MapInfo;

class TilePathScene;
class TilePathSceneSink;
class TSGridItem;

class TilePathRenderer : public BasePathRenderer
{
public:
    TilePathRenderer(TilePathScene *scene, World *world);
    ~TilePathRenderer();

    using BasePathRenderer::toScene;
    QPointF toScene(qreal x, qreal y, int level = 0);
    QPolygonF toScene(const QRectF &worldRect, int level = 0);
    QPolygonF toScene(const QPolygonF &worldPoly, int level);

    QPointF toWorld(qreal x, qreal y, int level = 0);

    QRectF sceneBounds(const QRectF &worldRect, int level = 0);

    TilePathScene *mScene;
    Tiled::ZLevelRenderer *mRenderer;
};

/**
  * Item that draws all the TileLayers on a single level.
  */
class TSCompositeLayerGroupItem : public QGraphicsItem
{
public:
    TSCompositeLayerGroupItem(CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);

    void synchWithTileLayers();

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

    CompositeLayerGroup *layerGroup() const { return mLayerGroup; }

private:
    CompositeLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
};

class TilePathScene : public BasePathScene
{
public:
    TilePathScene(PathDocument *doc, QObject *parent = 0);

    void setTool(AbstractTool *tool);

    bool isTile() const { return true; }

    void centerOn(const QPointF &worldPos);

    QPoint topLeftInWorld() const
    { return mTopLeftInWorld; }

    Tiled::Map *map() const
    { return mMap; }
    Tiled::MapRenderer *mapRenderer() const;
    int currentLevel();

private:
    Tiled::Map *mMap;
    MapInfo *mMapInfo;
    MapComposite *mMapComposite;
    QList<TilePathSceneSink*> mTileSinks;
    QPoint mTopLeftInWorld;
    TSGridItem *mGridItem;
    QList<TSCompositeLayerGroupItem*> mLayerGroupItems;
};

#endif // TILEPATHSCENE_H
