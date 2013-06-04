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

#include "heightmapview.h"

#include "heightmap.h"
#include "heightmapdocument.h"
#include "heightmaptools.h"

#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>

/////

HeightMapItem::HeightMapItem(HeightMapScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{
    setFlag(ItemUsesExtendedStyleOption);
}

QRectF HeightMapItem::boundingRect() const
{
    return mScene->boundingRect(mScene->hm()->bounds());
}

void HeightMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    HeightMap *hm = mScene->hm();

    painter->setBrush(Qt::lightGray);
#if 1
    const int tileWidth = mScene->tileWidth();
    const int tileHeight = mScene->tileHeight();

    if (tileWidth <= 0 || tileHeight <= 1)
        return;

    QRect rect = option->exposedRect.toAlignedRect();
//    if (rect.isNull())
//        rect = layerGroup->boundingRect(this).toAlignedRect();

    QMargins drawMargins/* = layerGroup->drawMargins();
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth)*/;

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = mScene->toWorld(rect.x(), rect.y());
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = mScene->toScene(rowItr);
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

    for (int y = startPos.y(); y - tileHeight < rect.bottom(); y += tileHeight / 2) {
        QPoint columnItr = rowItr;
        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
            if (hm->contains(columnItr.x(), columnItr.y())) {
                int h = hm->heightAt(columnItr.x(), columnItr.y());
                QPolygonF poly = mScene->toScene(QRect(columnItr.x(), columnItr.y(), 1, 1));
                poly.translate(0, -h);
                painter->drawPolygon(poly);
                if (h > 0) {
                    painter->drawLine(poly[1], poly[1] + QPointF(0, h));
                    painter->drawLine(poly[2], poly[2] + QPointF(0, h));
                    painter->drawLine(poly[3], poly[3] + QPointF(0, h));
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
#else
    for (int y = 0; y < hm->height(); y++) {
        for (int x = 0; x < hm->width(); x++) {
            QPolygonF poly = mScene->toScene(QRect(x, y, 1, 1));
            if (option->exposedRect.intersects(poly.boundingRect())) {
                int h = hm->heightAt(x, y);
                poly.translate(0, h);
                painter->drawPolygon(poly);
                if (h > 0) {
                    painter->drawLine(poly[1], poly[1] += QPointF(0, h));
                    painter->drawLine(poly[2], poly[2] + QPointF(0, h));
                    painter->drawLine(poly[3], poly[3] + QPointF(0, h));
                }
            }
        }
    }
#endif
}

/////

HeightMapScene::HeightMapScene(HeightMapDocument *hmDoc, QObject *parent) :
    BaseGraphicsScene(HeightMapSceneType, parent),
    mDocument(hmDoc),
    mHeightMap(new HeightMap(hmDoc->hmFile())),
    mHeightMapItem(new HeightMapItem(this)),
    mActiveTool(0)
{
    addItem(mHeightMapItem);
}

HeightMapScene::~HeightMapScene()
{
    delete mHeightMap;
}

void HeightMapScene::setTool(AbstractTool *tool)
{
    BaseHeightMapTool *hmTool = tool ? tool->asHeightMapTool() : 0;

    if (mActiveTool == hmTool)
        return;

    if (mActiveTool) {
        mActiveTool->deactivate();
    }

    mActiveTool = hmTool;

    if (mActiveTool) {
        mActiveTool->activate();
    }
}

QPointF HeightMapScene::toScene(const QPointF &p) const
{
    return toScene(p.x(), p.y());
}

QPointF HeightMapScene::toScene(qreal x, qreal y) const
{
    const int originX = hm()->height() * tileWidth() / 2; // top-left corner
    const int originY = 0;
    return QPointF((x - y) * tileWidth() / 2 + originX,
                   (x + y) * tileHeight() / 2 + originY);
}

QPolygonF HeightMapScene::toScene(const QRect &rect) const
{
    QPolygonF polygon;
    polygon += toScene(rect.topLeft());
    polygon += toScene(rect.topRight() + QPoint(1, 0));
    polygon += toScene(rect.bottomRight() + QPoint(1, 1));
    polygon += toScene(rect.bottomLeft() + QPoint(0, 1));
    polygon += polygon.first();
    return polygon;
}

QRect HeightMapScene::boundingRect(const QRect &rect) const
{
    const int originX = hm()->height() * tileWidth() / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth() / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight() / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth() / 2,
                     side * tileHeight() / 2);

    return QRect(pos, size);
}

QPointF HeightMapScene::toWorld(const QPointF &scenePos) const
{
    return toWorld(scenePos.x(), scenePos.y());
}

QPointF HeightMapScene::toWorld(qreal x, qreal y) const
{
    const qreal ratio = (qreal) tileWidth() / tileHeight();

    x -= hm()->height() * tileWidth() / 2;
    y -= 0/*map()->cellsPerLevel().y() * (maxLevel() - level) * tileHeight*/;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight(),
                   my / tileHeight());
}

QPoint HeightMapScene::toWorldInt(const QPointF &scenePos) const
{
    QPointF p = toWorld(scenePos);
    return QPoint(p.x(), p.y()); // don't use toPoint() it rounds up
}

void HeightMapScene::setCenter(int x, int y)
{
    mHeightMap->setCenter(x, y);
}

void HeightMapScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mousePressEvent(event);
}

void HeightMapScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mouseMoveEvent(event);
}

/////

HeightMapView::HeightMapView(QWidget *parent) :
    BaseGraphicsView(AlwaysGL, parent),
    mRecenterScheduled(false)
{
}

void HeightMapView::scrollContentsBy(int dx, int dy)
{
    BaseGraphicsView::scrollContentsBy(dx, dy);

    // scrollContentsBy() gets called twice, once for the horizontal scrollbar
    // and once for the vertical scrollbar.  When changing scale, the dx/dy
    // values are quite large.  So, don't update the heightmap until both
    // calls have completed.
    if (!mRecenterScheduled) {
        QMetaObject::invokeMethod(this, "recenter", Qt::QueuedConnection);
        mRecenterScheduled = true;
    }
}

void HeightMapView::recenter()
{
    mRecenterScheduled = false;

    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = ((HeightMapScene*)scene())->toWorld(scenePos.x(), scenePos.y());

    ((HeightMapScene*)scene())->setCenter(worldPos.x(), worldPos.y());
}
