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
#include "worldchunkmap.h"

#include "map.h"
#include "zlevelrenderer.h"

#include <QStyleOptionGraphicsItem>

using namespace Tiled;

#if 0
class TilePathSceneSink : public WorldTileLayer::TileSink
{
public:
    void putTile(int x, int y, WorldTile *tile)
    {
        int x1 = x - mScene->topLeftInWorld().x();
        int y1 = y - mScene->topLeftInWorld().y();

        if (mMapLayer->contains(x1, y1))
            mMapLayer->setCell(x1, y1, Cell(tile->mTiledTile));
    }

    WorldTile *getTile(int x, int y)
    {
        return 0; // FIXME
    }

    TilePathScene *mScene;
    WorldTileLayer *mWorldLayer;
    TileLayer *mMapLayer;
};
#endif

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
            QRect bounds(QPoint(), mScene->world()->size() * 300);
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

/////

class TSCompositeLayerGroup : public Tiled::ZTileLayerGroup
{
public:
    TSCompositeLayerGroup(TilePathScene *scene, Tiled::Map *map, int level);

    QRect bounds() const;
    QMargins drawMargins() const;

    bool orderedCellsAt(const QPoint &point, QVector<const Tiled::Cell*>& cells,
                        QVector<qreal> &opacities) const;

    void prepareDrawing(const Tiled::MapRenderer *renderer, const QRect &rect);

    TilePathScene *mScene;
    World *mWorld;
    QVector<Tiled::SparseTileGrid*> mGrids;
};

TSCompositeLayerGroup::TSCompositeLayerGroup(TilePathScene *scene, Map *map, int level) :
    ZTileLayerGroup(map, level),
    mScene(scene),
    mWorld(scene->world())
{
    mGrids.resize(20); // FIXME: max number of tiles in any cell on this level
    for (int x = 0; x < mGrids.size(); x++)
        mGrids[x] = new SparseTileGrid(mScene->chunkMap()->getWidthInTiles(),
                                       mScene->chunkMap()->getWidthInTiles());
}

QRect TSCompositeLayerGroup::bounds() const
{
    return QRect(0, 0, mWorld->width() * 300, mWorld->height() * 300);
}

QMargins TSCompositeLayerGroup::drawMargins() const
{
    return QMargins(0, 128 - 32, 64, 0);
}

bool TSCompositeLayerGroup::orderedCellsAt(const QPoint &point,
                                       QVector<const Cell *> &cells,
                                       QVector<qreal> &opacities) const
{
    cells.resize(0);
    opacities.resize(0);
    int x = point.x()  - mScene->chunkMap()->getWorldXMinTiles();
    int y = point.y() - mScene->chunkMap()->getWorldYMinTiles();
    if (WorldChunkSquare *sq = mScene->chunkMap()->getGridSquare(point.x(), point.y(), level())) {
        foreach (WorldTile *wtile, sq->tiles) {
            if (Tile *tile = wtile->mTiledTile) {
                mGrids[cells.size()]->replace(x, y, Cell(tile));
                const Cell *cell = &mGrids[cells.size()]->at(x, y);
                cells += cell;
                opacities += 1.0;
            }
        }
    }
    return !cells.isEmpty();
}

void TSCompositeLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    Q_UNUSED(renderer)
    Q_UNUSED(rect)
}

/////

TSCompositeLayerGroupItem::TSCompositeLayerGroupItem(ZTileLayerGroup *layerGroup,
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
//    if (mLayerGroup->needsSynch())
//        return; // needed, see MapComposite::mapAboutToChange
    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#ifdef _DEBUGxxx
    p->drawRect(mBoundingRect);
#endif
}

/////

TilePathScene::TilePathScene(PathDocument *doc, QObject *parent) :
    BasePathScene(doc, parent),
    mMap(new Map(Map::LevelIsometric, 1, 1, 64, 32)),
    mMapInfo(MapManager::instance()->newFromMap(mMap)),
    mMapComposite(0),
    mGridItem(new TSGridItem(this))
{
    setBackgroundBrush(Qt::darkGray);

    // The map is as big as the world, but I don't want MapBMP to allocate that
    // large an image!
    mMap->setWidth(world()->width() * 300);
    mMap->setHeight(world()->height() * 300);

    foreach (WorldTileLayer *wtl, world()->tileLayers()) {
        TileLayer *tl = new TileLayer(wtl->mName, 0, 0, mMap->width(), mMap->height());
        mMap->insertLayer(mMap->layerCount(), tl);
#if 0
        TilePathSceneSink *sink = new TilePathSceneSink;
        sink->mScene = this;
        sink->mWorldLayer = wtl;
        sink->mMapLayer = tl;
        wtl->mSinks += sink;
        mTileSinks += sink;
#endif
    }

    TilePathRenderer *renderer = new TilePathRenderer(this, world());
    setRenderer(renderer);
    setDocument(doc);

    mChunkMap = new WorldChunkMap(document());

    qDeleteAll(items());

//    mMapComposite = new MapComposite(mMapInfo);
    foreach (WorldTileLayer *wtl, world()->tileLayers()) {
        TSCompositeLayerGroup *lg = new TSCompositeLayerGroup(this, mMap, wtl->mLevel);
        TSCompositeLayerGroupItem *item = new TSCompositeLayerGroupItem(lg, renderer->mRenderer);
        mLayerGroupItems += item;
        addItem(item);
    }

    mGridItem->updateBoundingRect();
    addItem(mGridItem);

    setSceneRect(mGridItem->boundingRect());
}

TilePathScene::~TilePathScene()
{
    delete mChunkMap;
    delete mMapComposite;
    delete mMapInfo;
    delete mMap;
}

void TilePathScene::setTool(AbstractTool *tool)
{
}

void TilePathScene::scrollContentsBy(const QPointF &worldPos)
{
    int wx = qMax(0, int(worldPos.x()) - mMap->width() / 2);
    int wy = qMax(0, int(worldPos.y()) - mMap->height() / 2);
    mTopLeftInWorld = QPoint(wx, wy);

    mChunkMap->setCenter(worldPos.x(), worldPos.y());
}

void TilePathScene::centerOn(const QPointF &worldPos)
{
    int wx = qMax(0, int(worldPos.x()) - mMap->width() / 2);
    int wy = qMax(0, int(worldPos.y()) - mMap->height() / 2);
    mTopLeftInWorld = QPoint(wx, wy);

#if 0
    foreach (TileLayer *tl, mMap->tileLayers())
        tl->erase();

    PROGRESS progress(QLatin1String("Running scripts (run)"));

    foreach (WorldScript *ws, world()->scripts()) {
        QRect r = ws->mRegion.boundingRect();
        if (!r.intersects(QRect(mTopLeftInWorld, mMap->size()))) continue;
        Lua::LuaScript ls(world(), ws);
        ls.runFunction("run");
    }
#endif
    foreach (TSCompositeLayerGroupItem *item, mLayerGroupItems) {
//        item->layerGroup()->synch();
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
//    x -= mScene->topLeftInWorld().x();
//    y -= mScene->topLeftInWorld().y();
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
    return /*mScene->topLeftInWorld() +*/ mRenderer->pixelToTileCoords(x, y);
}

QRectF TilePathRenderer::sceneBounds(const QRectF &worldRect, int level)
{
    return toScene(worldRect, level).boundingRect();
}
