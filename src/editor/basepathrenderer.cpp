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

#include "basepathrenderer.h"

#include "pathworld.h"

#include <QDebug>
#include <QPainter>

BasePathRenderer::BasePathRenderer(PathWorld *world) :
    mWorld(world)
{
}

BasePathRenderer::~BasePathRenderer()
{
}

QPolygonF BasePathRenderer::toWorld(const QRectF &sceneRect, int level)
{
    QPolygonF pf;
    pf << toWorld(sceneRect.topLeft(), level);
    pf << toWorld(sceneRect.topRight(), level);
    pf << toWorld(sceneRect.bottomRight(), level);
    pf << toWorld(sceneRect.bottomLeft(), level);
    pf += pf.first();
    return pf;
}

void BasePathRenderer::drawGrid(QPainter *painter, const QRectF &rect, QColor gridColor, int level)
{
    const int tileWidth = mWorld->tileWidth();
    const int tileHeight = mWorld->tileHeight();

    const int mapHeight = mWorld->height();
    const int mapWidth = mWorld->width();

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
#if 0
    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
                                             1/painter->transform().m22()));
#endif
    pen.setBrush(brush);
    painter->setPen(pen);

    // Chop out every N'th grid line when the scale is small.
    int incr = 1;
    qreal scale = painter->worldMatrix().m22();
    while (tileWidth * incr * scale < 24) {
        ++incr;
    }

    /**
      * There is something badly broken with the OpenGL line stroking when the
      * painter's transform's dx/dy is a large-ish negative number. The lines
      * sometimes aren't drawn on different edges (clipping issue?), sometimes
      * the lines get double-width, and the dash pattern is messed up unless
      * the view is scrolled to just the right position . It seems likely to be
      * the result of some rounding issue. The cure is to translate the painter
      * back to an origin of 0,0.
      */
    QTransform xform = painter->transform();
    painter->setTransform(QTransform());

    for (int y = startY; y <= endY; y += 1) {
        if (y % incr) continue;
        const QPointF start = toScene(startX, (qreal)y, level) * xform;
        const QPointF end = toScene(endX + 1, (qreal)y, level) * xform;
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; x += 1) {
        if (x % incr) continue;
        const QPointF start = toScene(x, (qreal)startY, level) * xform;
        const QPointF end = toScene(x, (qreal)endY + 1, level) * xform;
        painter->drawLine(start, end);
    }
}
