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
#include "path.h"
#include "pathdocument.h"
#include "pathworld.h"
#include "progress.h"
#include "worldchanger.h"
#include "worldchunkmap.h"

#include "tile.h"

#include <qmath.h>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;

/////

TSLevelItem::TSLevelItem(WorldLevel *wlevel, TilePathScene *scene,
                         QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mLevel(wlevel)
    , mScene(scene)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = mScene->renderer()->sceneBounds(mScene->world()->bounds(),
                                                    mLevel->level());
}

void TSLevelItem::synchWithTileLayers()
{
    QRectF bounds = mScene->renderer()->sceneBounds(mScene->world()->bounds(),
                                                    mLevel->level());
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF TSLevelItem::boundingRect() const
{
    return mBoundingRect;
}

void TSLevelItem::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *option,
                        QWidget *)
{
    ((TilePathRenderer*)mScene->renderer())->drawLevel(
                painter, mLevel, option->exposedRect);
}

/////

TilePathScene::TilePathScene(PathDocument *doc, QObject *parent) :
    BasePathScene(doc, parent)
{
//    setBackgroundBrush(Qt::darkGray);

    TilePathRenderer *renderer = new TilePathRenderer(this, world());
    setRenderer(renderer);
    setDocument(doc);

    mChunkMap = new WorldChunkMap(document());
    connect(mChunkMap, SIGNAL(chunksUpdated()), SLOT(update()));
    connect(doc->changer(), SIGNAL(afterAddScriptToPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterAddScriptToPath(WorldPath*,int,WorldScript*)));
    connect(doc->changer(), SIGNAL(afterRemoveScriptFromPathSignal(WorldPath*,int,WorldScript*)),
            SLOT(afterAddScriptToPath(WorldPath*,int,WorldScript*)));

//    qDeleteAll(items());

    foreach (WorldLevel *wlevel, world()->levels()) {
        TSLevelItem *item = new TSLevelItem(wlevel, this);
        mLayerGroupItems += item;
        addItem(item);
    }

//    setSceneRect(mGridItem->boundingRect());
}

TilePathScene::~TilePathScene()
{
    delete mChunkMap;
}

void TilePathScene::scrollContentsBy(const QPointF &worldPos)
{
    mChunkMap->setCenter(worldPos.x(), worldPos.y());
}

void TilePathScene::centerOn(const QPointF &worldPos)
{
    Q_UNUSED(worldPos)
}

void TilePathScene::afterAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    Q_UNUSED(index)
    Q_UNUSED(script)
#if 0
    if (path->nodeCount())
        mChunkMap->nodeMoved(path->first(), path->first()->pos() + QPoint(1,1));
#endif
}

/////

TilePathRenderer::TilePathRenderer(TilePathScene *scene, PathWorld *world) :
    BasePathRenderer(world),
    mScene(scene)
{
}

TilePathRenderer::~TilePathRenderer()
{
}

QPointF TilePathRenderer::toScene(qreal x, qreal y, int level)
{
    const int tileWidth = mScene->world()->tileWidth();
    const int tileHeight = mScene->world()->tileHeight();
    const int mapHeight = mScene->world()->height();
    const int tilesPerLevel = 3;
    const int maxLevel = mScene->world()->maxLevel();

    const int originX = mapHeight * tileWidth / 2; // top-left corner
    const int originY = tilesPerLevel * (maxLevel - level) * tileHeight;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY);
}

QPolygonF TilePathRenderer::toScene(const QRectF &worldRect, int level)
{
    QPolygonF pf;
    pf << toScene(worldRect.topLeft(), level);
    pf << toScene(worldRect.topRight(), level);
    pf << toScene(worldRect.bottomRight(), level);
    pf << toScene(worldRect.bottomLeft(), level);
    pf += pf.first();
    return pf;
}

QPolygonF TilePathRenderer::toScene(const QPolygonF &worldPoly, int level)
{
    QPolygonF pf;
    foreach (QPointF p, worldPoly)
        pf += toScene(p, level);
    return pf;
}

QPointF TilePathRenderer::toWorld(qreal x, qreal y, int level)
{
    const int tileWidth = mScene->world()->tileWidth();
    const int tileHeight = mScene->world()->tileHeight();
    const qreal ratio = (qreal) tileWidth / tileHeight;

    const int mapHeight = mScene->world()->height();
    const int tilesPerLevel = 3;
    const int maxLevel = mScene->world()->maxLevel();

    x -= mapHeight * tileWidth / 2;
    y -= tilesPerLevel * (maxLevel - level) * tileHeight;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QRectF TilePathRenderer::sceneBounds(const QRectF &worldRect, int level)
{
    return toScene(worldRect, level).boundingRect();
}

void TilePathRenderer::drawLevel(QPainter *painter, WorldLevel *wlevel, const QRectF &exposed)
{
    int level = wlevel->level();

    const int tileWidth = mScene->world()->tileWidth();
    const int tileHeight = mScene->world()->tileHeight();

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull()) {
        Q_ASSERT(false);
        return;
    }

    QMargins drawMargins(0, 128 - 32, 0, 0); // ???

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = toWorld(rect.x(), rect.y(), level);
    QPoint rowItr = QPoint(qFloor(tilePos.x()), qFloor(tilePos.y()));
    QPointF startPos = toScene(rowItr, level);
    startPos.rx() -= tileWidth / 2;
    startPos.ry() += tileHeight;

    /* Determine in which half of the tile the top-left corner of the area we
     * need to draw is. If we're in the upper half, we need to start one row
     * up due to those tiles being visible as well. How we go up one row
     * depends on whether we're in the left or right half of the tile.
     */
    const bool inUpperHalf = startPos.y() - rect.y() > tileHeight / 2;
    const bool inLeftHalf = rect.x() - startPos.x() < tileWidth / 2;

    if (inUpperHalf) {
        if (inLeftHalf) {
            --rowItr.rx();
            startPos.rx() -= tileWidth / 2;
        } else {
            --rowItr.ry();
            startPos.rx() += tileWidth / 2;
        }
        startPos.ry() -= tileHeight / 2;
    }

    // Determine whether the current row is shifted half a tile to the right
    bool shifted = inUpperHalf ^ inLeftHalf;

//    qreal opacity = painter->opacity();

    for (int y = startPos.y(); y - tileHeight < rect.bottom();
         y += tileHeight / 2) {
        QPoint columnItr = rowItr;

        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
#if 1
            if (WorldChunkSquare *sq = mScene->chunkMap()->getGridSquare(columnItr.x(), columnItr.y(), level)) {
                foreach (WorldTile *wtile, sq->tiles) {
                    if (!wtile) continue;
                    if (Tile *tile = wtile->mTiledTile) {
#else
            if (layerGroup->orderedCellsAt(columnItr, cells, opacities)) {
                for (int i = 0; i < cells.size(); i++) {
                    const Cell *cell = cells[i];
                    if (!cell->isEmpty()) {
#endif
                        const QImage &img = tile->image();

//                        painter->setOpacity(opacities[i] * opacity);

                        painter->drawImage(x, y - img.height(), img);
                    }
                }
            }

            // Advance to the next column
            ++columnItr.rx();
            --columnItr.ry();
        }

        // Advance to the next row
        if (!shifted) {
            ++rowItr.rx();
            startPos.rx() += tileWidth / 2;
            shifted = true;
        } else {
            ++rowItr.ry();
            startPos.rx() -= tileWidth / 2;
            shifted = false;
        }
    }
}


