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

#include "lotpackwindow.h"
#include "ui_lotpackwindow.h"

#include "chunkmap.h"
#include "tilemetainfomgr.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "tilelayer.h"
#include "zlevelrenderer.h"

#include <QDebug>
#include <QFileDialog>

using namespace Tiled;

/////

LotPackLayerGroup::LotPackLayerGroup(IsoWorld *world, Map *map, int level) :
    ZTileLayerGroup(map, level),
    mWorld(world)
{
    mGrids.resize(20); // FIXME: max number of tiles in any cell on this level
    for (int x = 0; x < mGrids.size(); x++)
        mGrids[x] = new SparseTileGrid(mWorld->CurrentCell->ChunkMap->getWidthInTiles(),
                                       mWorld->CurrentCell->ChunkMap->getWidthInTiles());
}

QRect LotPackLayerGroup::bounds() const
{
    return QRect(0, 0, mWorld->getWidthInTiles(), mWorld->getHeightInTiles());
}

QMargins LotPackLayerGroup::drawMargins() const
{
    return QMargins(0, 128 - 32, 64, 0);
}

bool LotPackLayerGroup::orderedCellsAt(const QPoint &point,
                                       QVector<const Cell *> &cells,
                                       QVector<qreal> &opacities) const
{
    cells.resize(0);
    opacities.resize(0);
    int x = point.x() - mWorld->CurrentCell->ChunkMap->getWorldXMinTiles();
    int y = point.y() - mWorld->CurrentCell->ChunkMap->getWorldYMinTiles();
    if (IsoGridSquare *sq = mWorld->CurrentCell->getGridSquare(point.x(), point.y(), level())) {
        foreach (QString tileName, sq->tiles) {
            if (Tile *tile = mScene->mTileByName[tileName]/*BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName)*/) { // FIXME: way too slow
                mGrids[cells.size()]->replace(x, y, Cell(tile));
                const Cell *cell = &mGrids[cells.size()]->at(x, y);
                cells += cell;
                opacities += 1.0;
            }
        }
    }
    return !cells.isEmpty();
}

void LotPackLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
}

/////

LotPackLayerGroupItem::LotPackLayerGroupItem(LotPackLayerGroup *lg, MapRenderer *renderer) :
    QGraphicsItem(),
    mLayerGroup(lg),
    mRenderer(renderer)
{
    setFlag(ItemUsesExtendedStyleOption);
    mBoundingRect = mRenderer->boundingRect(lg->bounds());
}

QRectF LotPackLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void LotPackLayerGroupItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget)
{
    mRenderer->drawTileLayerGroup(painter, mLayerGroup, option->exposedRect);
}

/////

LotPackMiniMapItem::LotPackMiniMapItem(IsoWorld *world, MapRenderer *renderer) :
    mWorld(world),
    mRenderer(renderer)
{
    setFlag(ItemHasNoContents);
    mBoundingRect = mRenderer->boundingRect(QRect(world->MetaGrid->minx * world->CurrentCell->width,
                                                  world->MetaGrid->miny * world->CurrentCell->height,
                                                  world->getWidthInTiles(),
                                                  world->getHeightInTiles()));
    foreach (LotHeader *h, IsoLot::InfoHeaders) {
        foreach (BuildingDef *bdef, h->Buildings) {
            foreach (RoomDef *rdef, bdef->rooms) {
                if (rdef->level) continue;
                foreach (RoomRect *rr, rdef->rects) {
                    QPolygonF p;
                    p += mRenderer->tileToPixelCoords(rr->x, rr->y, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x + rr->w, rr->y, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x + rr->w, rr->y + rr->h, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x, rr->y + rr->h, rdef->level);
                    QGraphicsPolygonItem *item = new QGraphicsPolygonItem(this);
                    item->setPolygon(p);
                }
            }
        }
    }
}

QRectF LotPackMiniMapItem::boundingRect() const
{
    return mBoundingRect;
}

void LotPackMiniMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
}

///// ///// ///// ///// /////

/**
  * Item that draws the grid-lines in a LotPackScene.
  */
class IsoWorldGridItem : public QGraphicsItem
{
public:
    IsoWorldGridItem(IsoWorld *world, Tiled::MapRenderer *renderer);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    IsoWorld *mWorld;
    MapRenderer *mRenderer;
    QRectF mBoundingRect;
};


IsoWorldGridItem::IsoWorldGridItem(IsoWorld *world, Tiled::MapRenderer *renderer) :
    QGraphicsItem(),
    mWorld(world),
    mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    mBoundingRect = mRenderer->boundingRect(QRect(0, 0,
                                                  mWorld->getWidthInTiles(),
                                                  mWorld->getHeightInTiles()));
}

QRectF IsoWorldGridItem::boundingRect() const
{
    return mBoundingRect;
}

void IsoWorldGridItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *option,
                             QWidget *)
{
#if 1
    QColor gridColor(Qt::black);
//    gridColor.setAlpha(128);

    QPen gridPen(gridColor);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

    int startX = mWorld->MetaGrid->minx;
    int endX = mWorld->MetaGrid->maxx + 1;
    int startY = mWorld->MetaGrid->miny;
    int endY = mWorld->MetaGrid->maxy + 1;


    for (int y = startY; y <= endY; ++y) {
        const QPointF start = mRenderer->tileToPixelCoords(startX * 300, y * 300, 0);
        const QPointF end = mRenderer->tileToPixelCoords(endX * 300, y * 300, 0);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = mRenderer->tileToPixelCoords(x * 300, startY * 300, 0);
        const QPointF end = mRenderer->tileToPixelCoords(x * 300, endY * 300, 0);
        painter->drawLine(start, end);
    }
#else
    const int tileWidth = 64;
    const int tileHeight = 32;

    QRectF r = option->exposedRect;
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), mRenderer->pixelToTileCoords(r.topLeft()).x());
    const int startY = qMax(qreal(0), mRenderer->pixelToTileCoords(r.topRight()).y());
    const int endX = qMin(qreal(mWorld->getWidthInTiles()),
                          mRenderer->pixelToTileCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(mWorld->getHeightInTiles()),
                          mRenderer->pixelToTileCoords(r.bottomLeft()).y());

    QColor gridColor(Qt::black);
//    gridColor.setAlpha(128);

    QPen gridPen(gridColor);
    painter->setPen(gridPen);

    for (int y = startY; y <= endY; ++y) {
        if (y % 300) continue;
        const QPointF start = mRenderer->pixelToTileCoords(startX, (qreal)y);
        const QPointF end = mRenderer->pixelToTileCoords(endX, (qreal)y);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        if (x % 300) continue;
        const QPointF start = mRenderer->pixelToTileCoords(x, (qreal)startY);
        const QPointF end = mRenderer->pixelToTileCoords(x, (qreal)endY);
        painter->drawLine(start, end);
    }
#endif
}

/////

LotPackScene::LotPackScene(QWidget *parent) :
    BaseGraphicsScene(LotPackSceneType, parent)
{
    setBackgroundBrush(Qt::lightGray);
}

void LotPackScene::setWorld(IsoWorld *world)
{
    mWorld = world;

    mMap = new Map(Map::LevelIsometric,
                   mWorld->getWidthInTiles(),
                   mWorld->getHeightInTiles(),
                   64, 32);

    mRenderer = new ZLevelRenderer(mMap);

    for (int z = 0; z < IsoChunkMap::MaxLevels/*mWorld->CurrentCell->MaxHeight*/; z++) {
        LotPackLayerGroup *lg = new LotPackLayerGroup(mWorld, mMap, z);
        lg->mScene = this;
        LotPackLayerGroupItem *item = new LotPackLayerGroupItem(lg, mRenderer);
        addItem(item);
        mItems += item;
    }

    addItem(new IsoWorldGridItem(mWorld, mRenderer));
}

void LotPackScene::setMaxLevel(int max)
{
    for (int z = 0; z <= max; z++) {
        mItems[z]->setVisible(z <= max);
    }
}

/////

LotPackView::LotPackView(QWidget *parent) :
    BaseGraphicsView(true, parent),
    mScene(new LotPackScene(this)),
    mWorld(0)
{
    setScene(mScene);

    QVector<qreal> factors;
    factors << 0.25 << 0.33 << 0.5 << 0.75 << 1.0 << 1.5 << 2.0;
    zoomable()->setZoomFactors(factors);
    zoomable()->setScale(zoomable()->zoomFactors().first());

}

LotPackView::~LotPackView()
{
    delete mWorld;
}

void LotPackView::setWorld(IsoWorld *world)
{
    if (mWorld)
        delete mWorld;

    mWorld = world;

    mScene->setWorld(mWorld);

    LotPackMiniMapItem *item = new LotPackMiniMapItem(mWorld, mScene->renderer());
    (new IsoWorldGridItem(mWorld, mScene->renderer()))->setParentItem(item);

    addMiniMapItem(item);
}

void LotPackView::scrollContentsBy(int dx, int dy)
{
    BaseGraphicsView::scrollContentsBy(dx, dy);

    QPointF p = mapToScene(viewport()->rect().center());
    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(p);
    int wx = tilePos.x() / IsoChunkMap::ChunksPerWidth;
    int wy = tilePos.y() / IsoChunkMap::ChunksPerWidth;
    IsoChunkMap *cm = mWorld->CurrentCell->ChunkMap;
    wx = qBound(cm->ChunkGridWidth / 2, wx, mWorld->getWidthInTiles() / cm->ChunksPerWidth - cm->ChunkGridWidth / 2);
    wy = qBound(cm->ChunkGridWidth / 2, wy, mWorld->getHeightInTiles() / cm->ChunksPerWidth - cm->ChunkGridWidth / 2);
//    qDebug() << "LotPackView::scrollContentsBy" << wx << wy;
    if (wx != cm->WorldX || wy != cm->WorldY) {
        QRegion current = QRect(cm->getWorldXMin(), cm->getWorldYMin(),
                                cm->ChunkGridWidth, cm->ChunkGridWidth);
        QRegion updated = QRect(wx - cm->ChunkGridWidth / 2, wy - cm->ChunkGridWidth / 2,
                                cm->ChunkGridWidth, cm->ChunkGridWidth);

        // Discard old chunks.
        foreach (QRect r, (current - updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (IsoChunk *c = cm->getChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin())) {
                        c->Save(false);
                        cm->setChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin(), 0);
                        delete c;
                    }
                }
            }
        }

        // Shift preserved chunks.
        QVector<IsoChunk*> preserved;
        foreach (QRect r, (current & updated).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (IsoChunk *c = cm->getChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin())) {
//                        c->wx -= (wx - cm->WorldX);
//                        c->wy -= (wy - cm->WorldY);
                        cm->setChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin(), 0);
                        preserved += c;
                    }
                }
            }
        }

        cm->WorldX = wx;
        cm->WorldY = wy;
        cm->XMinTiles = cm->YMinTiles = -1;

        foreach (IsoChunk *c, preserved) {
            cm->setChunk(c->wx - cm->getWorldXMin(), c->wy - cm->getWorldYMin(), c);
        }

        // Load new chunks;
        foreach (QRect r, (updated - current).rects()) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
//                    if (x < mWorld->MetaGrid->minx || x > mWorld->MetaGrid->maxx) continue;
//                    if (y < mWorld->MetaGrid->miny || y > mWorld->MetaGrid->maxy) continue;
                    cm->LoadChunkForLater(x, y, x - cm->getWorldXMin(), y - cm->getWorldYMin());
                }
            }
        }

        cm->UpdateCellCache();

        for (int x = 0; x < cm->Chunks.size(); x++) {
            for (int y = 0; y < cm->Chunks[x].size(); y++) {
                if (IsoChunk *chunk = cm->Chunks[x][y]) {
                    if (!mScene->mHeadersExamined.contains(chunk->lotheader)) {
                        mScene->mHeadersExamined += chunk->lotheader;
                        foreach (QString tileName, chunk->lotheader->tilesUsed) {
                            if (!mScene->mTileByName.contains(tileName)) {
                                if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName)) {
                                    mScene->mTileByName[tileName] = tile;
                                }
                            }
                        }
                    }
                }
            }
        }

        mScene->setMaxLevel(mWorld->CurrentCell->MaxHeight);
    }
}

/////

LotPackWindow::LotPackWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LotPackWindow)
{
    ui->setupUi(this);

    mView = ui->view;

    connect(ui->actionOpen_World, SIGNAL(triggered()), SLOT(open()));

    TileMetaInfoMgr::instance()->loadTilesets();

    open();
}

LotPackWindow::~LotPackWindow()
{
    delete ui;
}

void LotPackWindow::open()
{
#ifdef QT_NO_DEBUGxxx
    QString f = QFileDialog::getOpenFileName(this, tr("Open LotPack"));
    if (f.isEmpty())
        return;

    IsoWorld *world = new IsoWorld(QFileInfo(f).absolutePath());
#else
    IsoWorld *world = new IsoWorld(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/Project Zomboid Latest/media/maps/Muldraugh, KY"));
#endif
    world->init();
    mView->setWorld(world);
}

