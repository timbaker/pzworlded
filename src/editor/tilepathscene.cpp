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

#include "tile.h"

#include <qmath.h>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;

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
        if (painter->worldMatrix().m22() < 0.25) return;
        QColor gridColor = Preferences::instance()->gridColor();
        mScene->renderer()->drawGrid(painter, option->exposedRect, gridColor,
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

TSLevelItem::TSLevelItem(int level,
                                                     TilePathScene *scene,
                                                     QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mLevel(level)
    , mScene(scene)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = mScene->renderer()->sceneBounds(
                QRect(0, 0,
                      mScene->world()->width() * 300,
                      mScene->world()->height() * 300), mLevel);
}

void TSLevelItem::synchWithTileLayers()
{
//    mLayerGroup->synch();

    QRectF bounds = mScene->renderer()->sceneBounds(
                QRect(0, 0, mScene->world()->width() * 300, mScene->world()->height() * 300), mLevel);
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
    BasePathScene(doc, parent),
    mGridItem(new TSGridItem(this))
{
    setBackgroundBrush(Qt::darkGray);

    TilePathRenderer *renderer = new TilePathRenderer(this, world());
    setRenderer(renderer);
    setDocument(doc);

    mChunkMap = new WorldChunkMap(document());

    qDeleteAll(items());

    foreach (WorldTileLayer *wtl, world()->tileLayers()) {
        int level;
        MapComposite::levelForLayer(wtl->mName, &level);
        while (level + 1 > mLayerGroupItems.size()) {
            TSLevelItem *item = new TSLevelItem(level, this);
            mLayerGroupItems += item;
            addItem(item);
        }
    }

    mGridItem->updateBoundingRect();
    addItem(mGridItem);

    setSceneRect(mGridItem->boundingRect());
}

TilePathScene::~TilePathScene()
{
    delete mChunkMap;
}

void TilePathScene::setTool(AbstractTool *tool)
{
    Q_UNUSED(tool)
}

void TilePathScene::scrollContentsBy(const QPointF &worldPos)
{
    mChunkMap->setCenter(worldPos.x(), worldPos.y());
}

void TilePathScene::centerOn(const QPointF &worldPos)
{
    Q_UNUSED(worldPos)
}

int TilePathScene::currentLevel()
{
    return 0; // FIXME
}

/////

TilePathRenderer::TilePathRenderer(TilePathScene *scene, World *world) :
    BasePathRenderer(world),
    mScene(scene)
{
}

TilePathRenderer::~TilePathRenderer()
{
}

QPointF TilePathRenderer::toScene(qreal x, qreal y, int level)
{
    const int tileWidth = 64;
    const int tileHeight = 32;
    const int mapHeight = mScene->world()->height() * 300;
    const int tilesPerLevel = 3;
    const int maxLevel = 16;

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
    const int tileWidth = 64;
    const int tileHeight = 32;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    const int mapHeight = mScene->world()->height() * 300;
    const int tilesPerLevel = 3;
    const int maxLevel = 16;

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

void TilePathRenderer::drawLevel(QPainter *painter, int level, const QRectF &exposed)
{
    const int tileWidth = 64;
    const int tileHeight = 32;

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

void TilePathRenderer::drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor, int level)
{
    const int tileWidth = 64;
    const int tileHeight = 32;

    const int mapHeight = mScene->world()->height() * 300;
    const int mapWidth = mScene->world()->width() * 300;

    QRect r = rect.toAlignedRect();
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), toWorld(r.topLeft(), level).x());
    const int startY = qMax(qreal(0), toWorld(r.topRight(), level).y());
    const int endX = qMin(qreal(mapWidth), toWorld(r.bottomRight(), level).x());
    const int endY = qMin(qreal(mapHeight), toWorld(r.bottomLeft(), level).y());

    gridColor.setAlpha(128);

    QPen pen;
    QBrush brush(gridColor, Qt::Dense4Pattern);
    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
                                             1/painter->transform().m22()));
    pen.setBrush(brush);
    painter->setPen(pen);

    for (int y = startY; y <= endY; ++y) {
        const QPointF start = toScene(startX, (qreal)y, level);
        const QPointF end = toScene(endX, (qreal)y, level);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = toScene(x, (qreal)startY, level);
        const QPointF end = toScene(x, (qreal)endY, level);
        painter->drawLine(start, end);
    }
}
