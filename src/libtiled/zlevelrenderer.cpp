/*
 * zlevelrenderer.cpp
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

#include "zlevelrenderer.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"
#include "imagelayer.h"
#include "ztilelayergroup.h"

#include <cmath>

using namespace Tiled;

#define DISPLAY_TILE_WIDTH (map()->tileWidth() * (is2x() ? 2 : 1))
#define DISPLAY_TILE_HEIGHT (map()->tileHeight() * (is2x() ? 2 : 1))

QSize ZLevelRenderer::mapSize() const
{
    // Map width and height contribute equally in both directions
    const int side = map()->height() + map()->width();
    return QSize(side * DISPLAY_TILE_WIDTH / 2
                 + maxLevel() * map()->cellsPerLevel().x() * DISPLAY_TILE_WIDTH,
                 side * DISPLAY_TILE_HEIGHT / 2
                 + maxLevel() * map()->cellsPerLevel().y() * DISPLAY_TILE_HEIGHT);
}

QRect ZLevelRenderer::boundingRect(const QRect &rect, int level) const
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    const int originX = map()->height() * tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2
                     + (maxLevel() - level) * map()->cellsPerLevel().y() * tileHeight);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

   return QRect(pos, size);
}

QRectF ZLevelRenderer::boundingRect(const MapObject *object) const
{
    Layer *layer = object->objectGroup();
    int level = layer ? layer->level() : 0;
    if (object->tile()) {
        const QPointF bottomCenter = tileToPixelCoords(object->position(), level);
        const QImage &img = object->tile()->image();
        return QRectF(bottomCenter.x() - img.width() / 2,
                      bottomCenter.y() - img.height(),
                      img.width(),
                      img.height()).adjusted(-1, -1, 1, 1);
    } else if (!object->polygon().isEmpty()) {
        const QPointF &pos = object->position();
        const QPolygonF polygon = object->polygon().translated(pos);
        const QPolygonF screenPolygon = tileToPixelCoords(polygon, level);
        return screenPolygon.boundingRect().adjusted(-2, -2, 3, 3);
    } else {
        // Take the bounding rect of the projected object, and then add a few
        // pixels on all sides to correct for the line width.
        const QRectF base = tileRectToPolygon(object->bounds(), level).boundingRect();
        return base.adjusted(-2, -3, 2, 2);
    }
}

QPainterPath ZLevelRenderer::shape(const MapObject *object) const
{
    Layer *layer = object->objectGroup();
    int level = layer ? layer->level() : 0;
#if 1
    /*
AddRemoveMapObject::removeObject
    mObjectGroup->removeObject(mMapObject) <<--------- why layer==0
    mMapDocument->emitObjectRemoved(mMapObject);
        MapDocument::deselectObjects
            emit selectedObjectsChanged()
                MapScene::updateSelectedObjectItems()
                    item->setEditable(...);
                    unsetCursor()
                        MapObjectItem::shape()
                            <this function>
    */
    if (layer == 0)
        return QPainterPath();
#else
    Q_ASSERT(layer);
#endif

    QPainterPath path;
    if (object->tile()) {
        path.addRect(boundingRect(object));
    } else {
        switch (object->shape()) {
        case MapObject::Rectangle:
            path.addPolygon(tileRectToPolygon(object->bounds(), level));
            break;
        case MapObject::Polygon:
        case MapObject::Polyline: {
            const QPointF &pos = object->position();
            const QPolygonF polygon = object->polygon().translated(pos);
            const QPolygonF screenPolygon = tileToPixelCoords(polygon, level);
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

void ZLevelRenderer::drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor, int level) const
{
    const int tileWidth = map()->tileWidth();
    const int tileHeight = map()->tileHeight();

    QRect r = rect.toAlignedRect();
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), pixelToTileCoords(r.topLeft(), level).x());
    const int startY = qMax(qreal(0), pixelToTileCoords(r.topRight(), level).y());
    const int endX = qMin(qreal(map()->width()),
                          pixelToTileCoords(r.bottomRight(), level).x());
    const int endY = qMin(qreal(map()->height()),
                          pixelToTileCoords(r.bottomLeft(), level).y());

    gridColor.setAlpha(128);

    QPen pen;
    pen.setCosmetic(true);
    QBrush brush(gridColor, Qt::Dense4Pattern);
//    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
//                                             1/painter->transform().m22()));
    pen.setBrush(brush);
    painter->setPen(pen);

    for (int y = startY; y <= endY; ++y) {
        const QPointF start = tileToPixelCoords(startX, (qreal)y, level);
        const QPointF end = tileToPixelCoords(endX, (qreal)y, level);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = tileToPixelCoords(x, (qreal)startY, level);
        const QPointF end = tileToPixelCoords(x, (qreal)endY, level);
        painter->drawLine(start, end);
    }
}

static Tile *g_missing_tile = 0;

void ZLevelRenderer::drawTileLayer(QPainter *painter,
                                      const TileLayer *layer,
                                      const QRectF &exposed) const
{
    int level = layer->level();

    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    if (tileWidth <= 0 || tileHeight <= 1)
        return;

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull())
        rect = boundingRect(layer->bounds(), level);

    QMargins drawMargins = layer->drawMargins() * (is2x() ? 2 : 1);
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth);

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = pixelToTileCoords(rect.x(), rect.y(), level);
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = tileToPixelCoords(rowItr, level);

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
            // Multi-threading
            if (mAbortDrawing && *mAbortDrawing) {
                painter->setTransform(baseTransform);
                return;
            }
            if (layer->contains(columnItr)) {
                const Cell &cell = layer->cellAt(columnItr);
                if (!cell.isEmpty()) {
                    QImage img = cell.tile->image();
                    const QPoint offset = cell.tile->tileset()->tileOffset() + cell.tile->offset();

                    qreal m11 = 1;      // Horizontal scaling factor
                    qreal m12 = 0;      // Vertical shearing factor
                    qreal m21 = 0;      // Horizontal shearing factor
                    qreal m22 = 1;      // Vertical scaling factor
                    qreal dx = offset.x() + x;
                    qreal dy = offset.y() + y - cell.tile->height();

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

                    if (tileWidth == cell.tile->width() * 2) {
                        m11 *= 2.0f;
                        m22 *= 2.0f;
                        dx += cell.tile->offset().x();
                        dy -= cell.tile->height() - cell.tile->offset().y();
                    } else if (tileWidth == cell.tile->width() / 2) {
                        float scale = 0.5f;
                        m11 *= scale;
                        m22 *= scale;
                        dy += cell.tile->height() / 2;
                    }

                    const QTransform transform(m11, m12, m21, m22, dx, dy);
                    painter->setTransform(transform * baseTransform);

                    painter->drawImage(0, 0, img);
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
#include <QThreadStorage>
void ZLevelRenderer::drawTileLayerGroup(QPainter *painter, ZTileLayerGroup *layerGroup,
                            const QRectF &exposed) const
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    if (tileWidth <= 0 || tileHeight <= 1 || layerGroup->bounds().isEmpty())
        return;

    int level = layerGroup->level();

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull())
        rect = layerGroup->boundingRect(this).toAlignedRect();

    QMargins drawMargins = layerGroup->drawMargins() * (is2x() ? 2 : 1);
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth);

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = pixelToTileCoords(rect.x(), rect.y(), level);
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = tileToPixelCoords(rowItr, level);
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

    QTransform baseTransform = painter->transform();

#if 0
    QThreadStorage<QVector<const Cell*> > cellStorage;
    QThreadStorage<QVector<qreal> > opacityStorage;
    QVector<const Cell*> &cells = cellStorage.localData();
    QVector<qreal> &opacities = opacityStorage.localData();
#else
    /*static*/ QVector<const Cell*> cells(40); // or QVarLengthArray
    /*static*/ QVector<qreal> opacities(40); // or QVarLengthArray
#endif

    layerGroup->prepareDrawing(this, rect);

    qreal opacity = painter->opacity();

    for (int y = startPos.y(); y - tileHeight < rect.bottom();
         y += tileHeight / 2)
    {
        QPoint columnItr = rowItr;

        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
            cells.resize(0);
            if (layerGroup->orderedCellsAt(columnItr, cells, opacities)) {
                for (int i = 0; i < cells.size(); i++) {
                    // Multi-threading
                    if (mAbortDrawing && *mAbortDrawing) {
                        painter->setTransform(baseTransform);
                        return;
                    }
                    const Cell *cell = cells[i];
                    if (!cell->isEmpty()) {
                        Tile *tile = cell->tile;
                        if (tile->image().isNull()) {
                            if (g_missing_tile == 0) {
                                Tileset *ts = new Tileset(QLatin1String("MISSING"), 64, 128);
                                if (ts->loadFromImage(QImage(QLatin1String(":/images/missing-tile.png")), QLatin1String(":/images/missing-tile.png"))) {
                                    g_missing_tile = ts->tileAt(0);
                                }
                            }
                            if (g_missing_tile)
                                tile = g_missing_tile;
                        }
                        QImage img = tile->image();
                        const QPoint offset = tile->tileset()->tileOffset() + tile->offset();

                        qreal m11 = 1;      // Horizontal scaling factor
                        qreal m12 = 0;      // Vertical shearing factor
                        qreal m21 = 0;      // Horizontal shearing factor
                        qreal m22 = 1;      // Vertical scaling factor
                        qreal dx = offset.x() + x;
                        qreal dy = offset.y() + y - tile->height();

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

                        if (tileWidth == tile->width() * 2) {
                            m11 *= 2.0f;
                            m22 *= 2.0f;
                            dx += tile->offset().x();
                            dy -= tile->height() - tile->offset().y();
                        } else if (tileWidth == tile->width() / 2) {
                            float scale = 0.5f;
                            m11 *= scale;
                            m22 *= scale;
//                            dx += (tileWidth - img.width() * scale) / 2;
//                            dy += (tile->tileset()->tileHeight() - img.height() * scale);
//                            dy -= (tileHeight - tileHeight * scale) / 2;
                            dy += tile->height() / 2;
                        }

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

void ZLevelRenderer::drawTileSelection(QPainter *painter,
                                          const QRegion &region,
                                          const QColor &color,
                                          const QRectF &exposed,
                                          int level) const
{
    painter->setBrush(color);
    painter->setPen(Qt::NoPen);
    foreach (const QRect &r, region.rects()) {
        QPolygonF polygon = tileRectToPolygon(r, level);
        if (QRectF(polygon.boundingRect()).intersects(exposed))
            painter->drawConvexPolygon(polygon);
    }
}

void ZLevelRenderer::drawMapObject(QPainter *painter,
                                      const MapObject *object,
                                      const QColor &color) const
{
    Layer *layer = object->objectGroup();
    int level = layer->level();

    painter->save();

    QPen pen(Qt::black);

    if (object->tile()) {
        const QImage &img = object->tile()->image();
        QPointF paintOrigin(-img.width() / 2, -img.height());
        paintOrigin += tileToPixelCoords(object->position(), level).toPoint();
        painter->drawImage(paintOrigin, img);

        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->drawRect(QRectF(paintOrigin, img.size()));
        pen.setStyle(Qt::DotLine);
        pen.setColor(color);
        painter->setPen(pen);
        painter->drawRect(QRectF(paintOrigin, img.size()));
    } else {
        QColor brushColor = color;
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
            QPolygonF polygon = tileRectToPolygon(object->bounds(), level);
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
            QPolygonF screenPolygon = tileToPixelCoords(polygon, level);

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
            QPolygonF screenPolygon = tileToPixelCoords(polygon, level);

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

void ZLevelRenderer::drawFancyRectangle(QPainter *painter,
                                        const QRectF &tileBounds,
                                        const QColor &color,
                                        int level) const
{
    painter->save();

    QPen pen(Qt::black);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    pen.setWidth(2);
    painter->setPen(pen);
    painter->setRenderHint(QPainter::Antialiasing);
    QPolygonF polygon = tileRectToPolygon(tileBounds, level);
    painter->drawPolygon(polygon);

    pen.setColor(color);
    painter->setPen(pen);
    QColor brushColor = color;
    brushColor.setAlpha(50);
    QBrush brush(brushColor);
    painter->setBrush(brush);
    polygon.translate(0, -1);
    painter->drawPolygon(polygon);

    painter->restore();
}

void ZLevelRenderer::drawImageLayer(QPainter *painter,
                                       const ImageLayer *imageLayer,
                                       const QRectF &exposed) const
{
    Q_UNUSED(exposed)

    const QPixmap &img = imageLayer->image();
    QPointF paintOrigin(-img.width() / 2, -img.height());

    paintOrigin += tileToPixelCoords(imageLayer->x(), (qreal)imageLayer->y());

    painter->drawPixmap(paintOrigin, img);
}

QPointF ZLevelRenderer::pixelToTileCoords(qreal x, qreal y, int level) const
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= map()->height() * tileWidth / 2;
#ifdef ZOMBOID
    y -= map()->cellsPerLevel().y() * (maxLevel() - level) * tileHeight;
#endif
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QPointF ZLevelRenderer::tileToPixelCoords(qreal x, qreal y, int level) const
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;
#ifdef ZOMBOID
    const int originX = map()->height() * tileWidth / 2; // top-left corner
    const int originY = map()->cellsPerLevel().y() * (maxLevel() - level) * tileHeight;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY);
#else
    const int originX = map()->height() * tileWidth / 2;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
#endif
}

QPolygonF ZLevelRenderer::tileRectToPolygon(const QRect &rect, int level) const
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    const QPointF topRight = tileToPixelCoords(rect.topRight(), level);
    const QPointF bottomRight = tileToPixelCoords(rect.bottomRight(), level);
    const QPointF bottomLeft = tileToPixelCoords(rect.bottomLeft(), level);

    QPolygonF polygon;
    polygon << QPointF(tileToPixelCoords(rect.topLeft(), level));
    polygon << QPointF(topRight.x() + tileWidth / 2,
                       topRight.y() + tileHeight / 2);
    polygon << QPointF(bottomRight.x(), bottomRight.y() + tileHeight);
    polygon << QPointF(bottomLeft.x() - tileWidth / 2,
                       bottomLeft.y() + tileHeight / 2);
    return polygon;
}

QPolygonF ZLevelRenderer::tileRectToPolygon(const QRectF &rect, int level) const
{
    QPolygonF polygon;
    polygon << QPointF(tileToPixelCoords(rect.topLeft(), level));
    polygon << QPointF(tileToPixelCoords(rect.topRight(), level));
    polygon << QPointF(tileToPixelCoords(rect.bottomRight(), level));
    polygon << QPointF(tileToPixelCoords(rect.bottomLeft(), level));
    return polygon;
}
