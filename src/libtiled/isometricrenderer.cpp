/*
 * isometricrenderer.cpp
 * Copyright 2009-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "isometricrenderer.h"

#include "map.h"
#include "mapobject.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"
#include "imagelayer.h"
#ifdef ZOMBOID
#include "ztilelayergroup.h"
#endif

#include <cmath>

using namespace Tiled;

QSize IsometricRenderer::mapSize() const
{
    // Map width and height contribute equally in both directions
    const int side = map()->height() + map()->width();
    return QSize(side * map()->tileWidth() / 2,
                 side * map()->tileHeight() / 2);
}

#ifdef ZOMBOID
QRect IsometricRenderer::boundingRect(const QRect &rect, int level) const
{
    Q_UNUSED(level)
#else
QRect IsometricRenderer::boundingRect(const QRect &rect) const
{
#endif
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    const int originX = map()->height() * tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

    return QRect(pos, size);
}

QRectF IsometricRenderer::boundingRect(const MapObject *object) const
{
    if (object->tile()) {
        const QPointF bottomCenter = tileToPixelCoords(object->position());
#ifdef ZOMBOID
        const QImage &img = object->tile()->image();
#else
        const QPixmap &img = object->tile()->image();
#endif
        return QRectF(bottomCenter.x() - img.width() / 2,
                      bottomCenter.y() - img.height(),
                      img.width(),
                      img.height()).adjusted(-1, -1, 1, 1);
    } else if (!object->polygon().isEmpty()) {
        const QPointF &pos = object->position();
        const QPolygonF polygon = object->polygon().translated(pos);
        const QPolygonF screenPolygon = tileToPixelCoords(polygon);
        return screenPolygon.boundingRect().adjusted(-2, -2, 3, 3);
    } else {
        // Take the bounding rect of the projected object, and then add a few
        // pixels on all sides to correct for the line width.
        const QRectF base = tileRectToPolygon(object->bounds()).boundingRect();
        return base.adjusted(-2, -3, 2, 2);
    }
}

QPainterPath IsometricRenderer::shape(const MapObject *object) const
{
    QPainterPath path;
    if (object->tile()) {
        path.addRect(boundingRect(object));
    } else {
        switch (object->shape()) {
        case MapObject::Rectangle:
            path.addPolygon(tileRectToPolygon(object->bounds()));
            break;
        case MapObject::Polygon:
        case MapObject::Polyline: {
            const QPointF &pos = object->position();
            const QPolygonF polygon = object->polygon().translated(pos);
            const QPolygonF screenPolygon = tileToPixelCoords(polygon);
            if (object->shape() == MapObject::Polygon) {
                path.addPolygon(screenPolygon);
            } else {
                for (int i = 1; i < screenPolygon.size(); ++i) {
                    path.addPolygon(lineToPolygon(screenPolygon[i - 1],
                                                  screenPolygon[i]));
                }
                path.setFillRule(Qt::WindingFill);
            }
            break;
        }
        }
    }
    return path;
}

#ifdef ZOMBOID
void IsometricRenderer::drawGrid(QPainter *painter, const QRectF &rect,
                                 QColor gridColor, int level,
                                 const QRect &tileBounds) const
{
    Q_UNUSED(level)

    QRect b = tileBounds;
    if (b.isEmpty())
        b = QRect(QPoint(0, 0), map()->size());
#else
void IsometricRenderer::drawGrid(QPainter *painter, const QRectF &rect,
                                 QColor gridColor) const
{
#endif
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    QRect r = rect.toAlignedRect();
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

#ifdef ZOMBOID
    const int startX = qMax(qreal(b.left()), pixelToTileCoords(r.topLeft()).x());
    const int startY = qMax(qreal(b.top()), pixelToTileCoords(r.topRight()).y());
    const int endX = qMin(qreal(b.right() + 1), pixelToTileCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(b.bottom() + 1), pixelToTileCoords(r.bottomLeft()).y());
#else
    const int startX = qMax(qreal(0), pixelToTileCoords(r.topLeft()).x());
    const int startY = qMax(qreal(0), pixelToTileCoords(r.topRight()).y());
    const int endX = qMin(qreal(map()->width()),
                          pixelToTileCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(map()->height()),
                          pixelToTileCoords(r.bottomLeft()).y());
#endif

    gridColor.setAlpha(128);

#ifdef ZOMBOID
    QPen pen;
    pen.setCosmetic(true);
    QBrush brush(gridColor, Qt::Dense4Pattern);
    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
                                             1/painter->transform().m22()));
    pen.setBrush(brush);
    painter->setPen(pen);
#else
    QPen gridPen(gridColor);
    gridPen.setDashPattern(QVector<qreal>() << 2 << 2);
    painter->setPen(gridPen);
#endif

    for (int y = startY; y <= endY; ++y) {
        const QPointF start = tileToPixelCoords(startX, (qreal)y);
        const QPointF end = tileToPixelCoords(endX, (qreal)y);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = tileToPixelCoords(x, (qreal)startY);
        const QPointF end = tileToPixelCoords(x, (qreal)endY);
        painter->drawLine(start, end);
    }
}

void IsometricRenderer::drawTileLayer(QPainter *painter,
                                      const TileLayer *layer,
                                      const QRectF &exposed) const
{
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    if (tileWidth <= 0 || tileHeight <= 1)
        return;

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull())
        rect = boundingRect(layer->bounds());

    QMargins drawMargins = layer->drawMargins();
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth);

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = pixelToTileCoords(rect.x(), rect.y());
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = tileToPixelCoords(rowItr);
    startPos.rx() -= tileWidth / 2;
    startPos.ry() += tileHeight;

    // Compensate for the layer position
    rowItr -= QPoint(layer->x(), layer->y());

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

    QTransform baseTransform = painter->transform();

    for (int y = startPos.y(); y - tileHeight < rect.bottom();
         y += tileHeight / 2)
    {
        QPoint columnItr = rowItr;

        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
#ifdef ZOMBOID
            // Multi-threading
            if (mAbortDrawing && *mAbortDrawing) {
                painter->setTransform(baseTransform);
                return;
            }
#endif
            if (layer->contains(columnItr)) {
                const Cell &cell = layer->cellAt(columnItr);
                if (!cell.isEmpty()) {
#ifdef ZOMBOID
                    const QImage &img = cell.tile->image();
#else
                    const QPixmap &img = cell.tile->image();
#endif
                    const QPoint offset = cell.tile->tileset()->tileOffset();

                    qreal m11 = 1;      // Horizontal scaling factor
                    qreal m12 = 0;      // Vertical shearing factor
                    qreal m21 = 0;      // Horizontal shearing factor
                    qreal m22 = 1;      // Vertical scaling factor
                    qreal dx = offset.x() + x;
                    qreal dy = offset.y() + y - img.height();
#if 0
                    if (cell.flippedAntiDiagonally) {
                        // Use shearing to swap the X/Y axis
                        m11 = 0;
                        m12 = 1;
                        m21 = 1;
                        m22 = 0;

                        // Compensate for the swap of image dimensions
                        dy += img.height() - img.width();
                    }
                    if (cell.flippedHorizontally) {
                        m11 = -m11;
                        m21 = -m21;
                        dx += cell.flippedAntiDiagonally ? img.height()
                                                         : img.width();
                    }
                    if (cell.flippedVertically) {
                        m12 = -m12;
                        m22 = -m22;
                        dy += cell.flippedAntiDiagonally ? img.width()
                                                         : img.height();
                    }
#endif
                    const QTransform transform(m11, m12, m21, m22, dx, dy);
                    painter->setTransform(transform * baseTransform);

#ifdef ZOMBOID
                    painter->drawImage(0, 0, img);
#else
                    painter->drawPixmap(0, 0, img);
#endif
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

    painter->setTransform(baseTransform);
}

#ifdef ZOMBOID
void IsometricRenderer::drawTileLayerGroup(QPainter *painter, ZTileLayerGroup *layerGroup,
                            const QRectF &exposed) const
{
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    if (tileWidth <= 0 || tileHeight <= 1)
        return;

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull())
        rect = layerGroup->boundingRect(this).toAlignedRect();

    QMargins drawMargins = layerGroup->drawMargins();
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth);

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = pixelToTileCoords(rect.x(), rect.y());
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = tileToPixelCoords(rowItr);
    startPos.rx() -= tileWidth / 2;
    startPos.ry() += tileHeight;

#if 0
    // Compensate for the layer position
    rowItr -= QPoint(layer->x(), layer->y());
#endif

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

    QTransform baseTransform = painter->transform();

    /*static*/ QVector<const Cell*> cells(40); // or QVarLengthArray
    /*static*/ QVector<qreal> opacities(40); // or QVarLengthArray

    layerGroup->prepareDrawing(this, rect);

    qreal opacity = painter->opacity();

    for (int y = startPos.y(); y - tileHeight < rect.bottom();
         y += tileHeight / 2)
    {
        QPoint columnItr = rowItr;

        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
            cells.resize(0);
            if (layerGroup->orderedCellsAt(this, columnItr, cells, opacities)) {
                for (int i = 0; i < cells.size(); i++) {
                    // Multi-threading
                    if (mAbortDrawing && *mAbortDrawing) {
                        painter->setTransform(baseTransform);
                        return;
                    }
                    const Cell *cell = cells[i];
                    if (!cell->isEmpty()) {
                        const QImage &img = cell->tile->image();
                        const QPoint offset = cell->tile->tileset()->tileOffset();

                        qreal m11 = 1;      // Horizontal scaling factor
                        qreal m12 = 0;      // Vertical shearing factor
                        qreal m21 = 0;      // Horizontal shearing factor
                        qreal m22 = 1;      // Vertical scaling factor
                        qreal dx = offset.x() + x;
                        qreal dy = offset.y() + y - img.height();
#if 0
                        if (cell->flippedAntiDiagonally) {
                            // Use shearing to swap the X/Y axis
                            m11 = 0;
                            m12 = 1;
                            m21 = 1;
                            m22 = 0;

                            // Compensate for the swap of image dimensions
                            dy += img.height() - img.width();
                        }
                        if (cell->flippedHorizontally) {
                            m11 = -m11;
                            m21 = -m21;
                            dx += cell->flippedAntiDiagonally ? img.height()
                                                             : img.width();
                        }
                        if (cell->flippedVertically) {
                            m12 = -m12;
                            m22 = -m22;
                            dy += cell->flippedAntiDiagonally ? img.width()
                                                             : img.height();
                        }
#endif
                        const QTransform transform(m11, m12, m21, m22, dx, dy);
                        painter->setTransform(transform * baseTransform);

                        painter->setOpacity(opacities[i] * opacity);

                        painter->drawImage(0, 0, img);
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

    painter->setTransform(baseTransform);
}
#endif // ZOMBOID

void IsometricRenderer::drawTileSelection(QPainter *painter,
                                          const QRegion &region,
                                          const QColor &color,
#ifdef ZOMBOID
                                          const QRectF &exposed,
                                          int level) const
{
    Q_UNUSED(level)
#else
                                          const QRectF &exposed) const
{
#endif
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);
    foreach (const QRect &r, region.rects()) {
        QPolygonF polygon = tileRectToPolygon(r);
        if (QRectF(polygon.boundingRect()).intersects(exposed))
            painter->drawConvexPolygon(polygon);
    }
}

void IsometricRenderer::drawMapObject(QPainter *painter,
                                      const MapObject *object,
                                      const QColor &color) const
{
    painter->save();

    QPen pen(Qt::black);

    if (object->tile()) {
#ifdef ZOMBOID
        const QImage &img = object->tile()->image();
        QPointF paintOrigin(-img.width() / 2, -img.height());
        paintOrigin += tileToPixelCoords(object->position()).toPoint();
        painter->drawImage(paintOrigin, img);
#else
        const QPixmap &img = object->tile()->image();
        QPointF paintOrigin(-img.width() / 2, -img.height());
        paintOrigin += tileToPixelCoords(object->position()).toPoint();
        painter->drawPixmap(paintOrigin, img);
#endif

        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->drawRect(QRectF(paintOrigin, img.size()));
        pen.setStyle(Qt::DotLine);
        pen.setColor(color);
        painter->setPen(pen);
        painter->drawRect(QRectF(paintOrigin, img.size()));
    } else {
        QColor brushColor = color;
#ifdef ZOMBOID
        if (color.alpha() != 255)
            brushColor.setAlpha(color.alpha());
        else
#endif
        brushColor.setAlpha(50);
        QBrush brush(brushColor);

        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);

        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);

        // TODO: Draw the object name
        // TODO: Do something sensible to make null-sized objects usable

        switch (object->shape()) {
        case MapObject::Rectangle: {
            QPolygonF polygon = tileRectToPolygon(object->bounds());
            painter->drawPolygon(polygon);

            pen.setColor(color);
            painter->setPen(pen);
            painter->setBrush(brush);
            polygon.translate(0, -1);

            painter->drawPolygon(polygon);
            break;
        }
        case MapObject::Polygon: {
            const QPointF &pos = object->position();
            const QPolygonF polygon = object->polygon().translated(pos);
            QPolygonF screenPolygon = tileToPixelCoords(polygon);

            painter->drawPolygon(screenPolygon);

            pen.setColor(color);
            painter->setPen(pen);
            painter->setBrush(brush);
            screenPolygon.translate(0, -1);

            painter->drawPolygon(screenPolygon);
            break;
        }
        case MapObject::Polyline: {
            const QPointF &pos = object->position();
            const QPolygonF polygon = object->polygon().translated(pos);
            QPolygonF screenPolygon = tileToPixelCoords(polygon);

            painter->drawPolyline(screenPolygon);

            pen.setColor(color);
            painter->setPen(pen);
            screenPolygon.translate(0, -1);

            painter->drawPolyline(screenPolygon);
            break;
        }
        }
    }

    painter->restore();
}

#ifdef ZOMBOID
void IsometricRenderer::drawFancyRectangle(QPainter *painter,
                                           const QRectF &tileBounds,
                                           const QColor &color,
                                           int level) const
{
    Q_UNUSED(level)

    painter->save();

    QPen pen(Qt::black);

    QColor brushColor = color;
    brushColor.setAlpha(50);
    QBrush brush(brushColor);

    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(2);

    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);

    QPolygonF polygon = tileRectToPolygon(tileBounds);
    painter->drawPolygon(polygon);

    pen.setColor(color);
    painter->setPen(pen);
    painter->setBrush(brush);
    polygon.translate(0, -1);
    painter->drawPolygon(polygon);

    painter->restore();
}
#endif

void IsometricRenderer::drawImageLayer(QPainter *painter,
                                       const ImageLayer *imageLayer,
                                       const QRectF &exposed) const
{
    Q_UNUSED(exposed)

    const QPixmap &img = imageLayer->image();
    QPointF paintOrigin(-img.width() / 2, -img.height());

    paintOrigin += tileToPixelCoords(imageLayer->x(), (qreal)imageLayer->y());

    painter->drawPixmap(paintOrigin, img);
}

#ifdef ZOMBOID
QPointF IsometricRenderer::pixelToTileCoords(qreal x, qreal y, int level) const
{
    Q_UNUSED(level)
#else
QPointF IsometricRenderer::pixelToTileCoords(qreal x, qreal y) const
{
#endif
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= map()->height() * tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

#ifdef ZOMBOID
QPointF IsometricRenderer::tileToPixelCoords(qreal x, qreal y, int level) const
{
    Q_UNUSED(level)
#else
QPointF IsometricRenderer::tileToPixelCoords(qreal x, qreal y) const
{
#endif
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();
    const int originX = map()->height() * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

#ifdef ZOMBOID
QPolygonF IsometricRenderer::tileRectToPolygon(const QRect &rect, int level) const
{
    Q_UNUSED(level)
#else
QPolygonF IsometricRenderer::tileRectToPolygon(const QRect &rect) const
{
#endif
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    const QPointF topRight = tileToPixelCoords(rect.topRight());
    const QPointF bottomRight = tileToPixelCoords(rect.bottomRight());
    const QPointF bottomLeft = tileToPixelCoords(rect.bottomLeft());

    QPolygonF polygon;
    polygon << QPointF(tileToPixelCoords(rect.topLeft()));
    polygon << QPointF(topRight.x() + tileWidth / 2,
                       topRight.y() + tileHeight / 2);
    polygon << QPointF(bottomRight.x(), bottomRight.y() + tileHeight);
    polygon << QPointF(bottomLeft.x() - tileWidth / 2,
                       bottomLeft.y() + tileHeight / 2);
    return polygon;
}

#ifdef ZOMBOID
QPolygonF IsometricRenderer::tileRectToPolygon(const QRectF &rect, int level) const
{
    Q_UNUSED(level)
#else
QPolygonF IsometricRenderer::tileRectToPolygon(const QRectF &rect) const
{
#endif
    QPolygonF polygon;
    polygon << QPointF(tileToPixelCoords(rect.topLeft()));
    polygon << QPointF(tileToPixelCoords(rect.topRight()));
    polygon << QPointF(tileToPixelCoords(rect.bottomRight()));
    polygon << QPointF(tileToPixelCoords(rect.bottomLeft()));
    return polygon;
}
