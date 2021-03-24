/*
 * orthogonalrenderer.h
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef ORTHOGONALRENDERER_H
#define ORTHOGONALRENDERER_H

#include "maprenderer.h"

namespace Tiled {

/**
 * The orthogonal map renderer. This is the most basic map renderer,
 * dealing with maps that use rectangular tiles.
 */
class TILEDSHARED_EXPORT OrthogonalRenderer : public MapRenderer
{
public:
    OrthogonalRenderer(const Map *map) : MapRenderer(map) {}

    QSize mapSize() const;

#ifdef ZOMBOID
   QRect boundingRect(const QRect &rect, int level = 0) const;
#else
    QRect boundingRect(const QRect &rect) const;
#endif

    QRectF boundingRect(const MapObject *object) const;
    QPainterPath shape(const MapObject *object) const;

#ifdef ZOMBOID
    void drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor,
                  int level = 0, const QRect &tileBounds = QRect()) const;
#else
    void drawGrid(QPainter *painter, const QRectF &rect,
                  QColor gridColor) const;
#endif

    void drawTileLayer(QPainter *painter, const TileLayer *layer,
                       const QRectF &exposed = QRectF()) const;

#ifdef ZOMBOID
   void drawTileLayerGroup(QPainter *painter, ZTileLayerGroup *layerGroup,
                               const QRectF &exposed = QRectF()) const
   {
       Q_UNUSED(painter)
       Q_UNUSED(layerGroup)
       Q_UNUSED(exposed)
   }
#endif

    void drawTileSelection(QPainter *painter,
                           const QRegion &region,
                           const QColor &color,
#ifdef ZOMBOID
                            const QRectF &exposed,
                            int level = 0) const;
#else
                           const QRectF &exposed) const;
#endif

    void drawMapObject(QPainter *painter,
                       const MapObject *object,
                       const QColor &color) const;

#ifdef ZOMBOID
    virtual void drawFancyRectangle(QPainter *painter,
                                    const QRectF &tileBounds,
                                    const QColor &color,
                                    int level = 0) const;
#endif

    void drawImageLayer(QPainter *painter,
                        const ImageLayer *layer,
                        const QRectF &exposed = QRectF()) const;

#ifdef ZOMBOID
    QPointF pixelToTileCoords(qreal x, qreal y, int level = 0) const;

    using MapRenderer::tileToPixelCoords;
    QPointF tileToPixelCoords(qreal x, qreal y, int level = 0) const;
#else
    QPointF pixelToTileCoords(qreal x, qreal y) const;

    using MapRenderer::tileToPixelCoords;
    QPointF tileToPixelCoords(qreal x, qreal y) const;
#endif
};

} // namespace Tiled

#endif // ORTHOGONALRENDERER_H
