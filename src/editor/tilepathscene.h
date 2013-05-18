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

class WorldChunkMap;

class TilePathScene;
class TilePathSceneSink;
class TSGridItem;

class TilePathRenderer : public BasePathRenderer
{
public:
    TilePathRenderer(TilePathScene *scene, PathWorld *world);
    ~TilePathRenderer();

    using BasePathRenderer::toScene;
    QPointF toScene(qreal x, qreal y, int level = 0);
    QPolygonF toScene(const QRectF &worldRect, int level = 0);
    QPolygonF toScene(const QPolygonF &worldPoly, int level);

    using BasePathRenderer::toWorld;
    QPointF toWorld(qreal x, qreal y, int level = 0);

    QRectF sceneBounds(const QRectF &worldRect, int level = 0);

    void drawLevel(QPainter *painter, int level, const QRectF &exposed);
    void drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor, int level);

    TilePathScene *mScene;
};

/**
  * Item that draws all the TileLayers on a single level.
  */
class TSLevelItem : public QGraphicsItem
{
public:
    TSLevelItem(int level, TilePathScene *scene, QGraphicsItem *parent = 0);

    void synchWithTileLayers();

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *);

private:
    int mLevel;
    TilePathScene *mScene;
    QRectF mBoundingRect;
};

class TilePathScene : public BasePathScene
{
public:
    TilePathScene(PathDocument *doc, QObject *parent = 0);
    ~TilePathScene();

    bool isTile() const { return true; }

    void scrollContentsBy(const QPointF &worldPos);

    TilePathRenderer *renderer() const
    { return (TilePathRenderer*)BasePathScene::renderer(); }

    void centerOn(const QPointF &worldPos);

    int currentLevel();

    WorldChunkMap *chunkMap() const
    { return mChunkMap; }

private:
    TSGridItem *mGridItem;
    QList<TSLevelItem*> mLayerGroupItems;
    WorldChunkMap *mChunkMap;
};

#endif // TILEPATHSCENE_H
