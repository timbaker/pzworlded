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

#include "tilepathscene.h"

#include "luaworlded.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "progress.h"
#include "world.h"

#include "map.h"
#include "zlevelrenderer.h"

#include <QStyleOptionGraphicsItem>

using namespace Tiled;

class TilePathSceneSink : public WorldTileLayer::TileSink
{
public:
    virtual void putTile(int x, int y, WorldTile *tile)
    {
        int x1 = x - mScene->topLeftInWorld().x();
        int y1 = y - mScene->topLeftInWorld().y();

        if (mMapLayer->contains(x1, y1))
            mMapLayer->setCell(x1, y1, Cell(tile->mTiledTile));
    }

    virtual WorldTile *getTile(int x, int y)
    {
        return 0; // FIXME
    }

    TilePathScene *mScene;
    WorldTileLayer *mWorldLayer;
    TileLayer *mMapLayer;
};

/////

class TSGridItem : public QGraphicsItem
{
public:
    TSGridItem(TilePathScene *scene, QGraphicsItem *parent = 0)
        : QGraphicsItem(parent)
        , mScene(scene)
    {
        setAcceptedMouseButtons(0);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    }

    QRectF boundingRect() const
    {
        return mBoundingRect;
    }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *)
    {
        QColor gridColor = Preferences::instance()->gridColor();
        mScene->mapRenderer()->drawGrid(painter, option->exposedRect, gridColor,
                                        mScene->currentLevel());
    }

    void updateBoundingRect()
    {
        QRectF boundsF;
        if (mScene->renderer()) {
            QRect bounds(QPoint(), mScene->map()->size());
            boundsF = mScene->renderer()->sceneBounds(bounds, mScene->currentLevel());
        }
        if (boundsF != mBoundingRect) {
            prepareGeometryChange();
            mBoundingRect = boundsF;
        }
    }

private:
    TilePathScene *mScene;
    QRectF mBoundingRect;
};

/////

TSCompositeLayerGroupItem::TSCompositeLayerGroupItem(CompositeLayerGroup *layerGroup,
                                                     Tiled::MapRenderer *renderer,
                                                     QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mLayerGroup(layerGroup)
    , mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = layerGroup->boundingRect(mRenderer);
}

void TSCompositeLayerGroupItem::synchWithTileLayers()
{
//    mLayerGroup->synch();

    QRectF bounds = mLayerGroup->boundingRect(mRenderer);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF TSCompositeLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void TSCompositeLayerGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (mLayerGroup->needsSynch())
        return; // needed, see MapComposite::mapAboutToChange
    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#ifdef _DEBUGxxx
    p->drawRect(mBoundingRect);
#endif
}

/////

TilePathScene::TilePathScene(PathDocument *doc, QObject *parent) :
    BasePathScene(doc, parent),
    mMap(new Map(Map::LevelIsometric, 300, 300, 64, 32)),
    mMapInfo(MapManager::instance()->newFromMap(mMap)),
    mGridItem(new TSGridItem(this))
{
    setBackgroundBrush(Qt::darkGray);

    foreach (WorldTileLayer *wtl, world()->tileLayers()) {
        TileLayer *tl = new TileLayer(wtl->mName, 0, 0, mMap->width(), mMap->height());
        mMap->insertLayer(mMap->layerCount(), tl);

        TilePathSceneSink *sink = new TilePathSceneSink;
        sink->mScene = this;
        sink->mWorldLayer = wtl;
        sink->mMapLayer = tl;
        wtl->mSinks += sink;
        mTileSinks += sink;
    }

    TilePathRenderer *renderer = new TilePathRenderer(this, world());
    setRenderer(renderer);
    setDocument(doc);

    qDeleteAll(items());

    mMapComposite = new MapComposite(mMapInfo);
    foreach (CompositeLayerGroup *lg, mMapComposite->layerGroups()) {
        TSCompositeLayerGroupItem *item = new TSCompositeLayerGroupItem(lg, renderer->mRenderer);
        mLayerGroupItems += item;
        addItem(item);
    }

    mGridItem->updateBoundingRect();
    addItem(mGridItem);

    setSceneRect(mGridItem->boundingRect());
}

void TilePathScene::setTool(AbstractTool *tool)
{
}

void TilePathScene::centerOn(const QPointF &worldPos)
{
    int wx = qMax(0, int(worldPos.x()) - mMap->width() / 2);
    int wy = qMax(0, int(worldPos.y()) - mMap->height() / 2);
    mTopLeftInWorld = QPoint(wx, wy);

    foreach (TileLayer *tl, mMap->tileLayers())
        tl->erase();

    PROGRESS progress(QLatin1String("Running scripts (run)"));

    foreach (WorldScript *ws, world()->scripts()) {
        QRect r = ws->mRegion.boundingRect();
        if (!r.intersects(QRect(mTopLeftInWorld, mMap->size()))) continue;
        Lua::LuaScript ls(world(), ws);
        ls.runFunction("run");
    }

    foreach (TSCompositeLayerGroupItem *item, mLayerGroupItems) {
        item->layerGroup()->synch();
        item->synchWithTileLayers();
    }
}

MapRenderer *TilePathScene::mapRenderer() const
{
    return ((TilePathRenderer*)renderer())->mRenderer;
}

int TilePathScene::currentLevel()
{
    return 0; // FIXME
}

/////

TilePathRenderer::TilePathRenderer(TilePathScene *scene, World *world) :
    BasePathRenderer(world),
    mScene(scene),
    mRenderer(new ZLevelRenderer(scene->map()))
{
}

TilePathRenderer::~TilePathRenderer()
{
    delete mRenderer;
}

QPointF TilePathRenderer::toScene(qreal x, qreal y, int level)
{
    x -= mScene->topLeftInWorld().x();
    y -= mScene->topLeftInWorld().y();
    return mRenderer->tileToPixelCoords(x, y, level);
}

QPolygonF TilePathRenderer::toScene(const QRectF &worldRect, int level)
{
    QPolygonF pf;
    pf << toScene(worldRect.topLeft());
    pf << toScene(worldRect.topRight());
    pf << toScene(worldRect.bottomRight());
    pf << toScene(worldRect.bottomLeft());
    pf += pf.first();
    return pf;
}

QPolygonF TilePathRenderer::toScene(const QPolygonF &worldPoly, int level)
{
    QPolygonF pf;
    foreach (QPointF p, worldPoly)
        pf += toScene(p);
    return pf;
}

QPointF TilePathRenderer::toWorld(qreal x, qreal y, int level)
{
    return mScene->topLeftInWorld() + mRenderer->pixelToTileCoords(x, y);
}

QRectF TilePathRenderer::sceneBounds(const QRectF &worldRect, int level)
{
    return toScene(worldRect, level).boundingRect();
}
