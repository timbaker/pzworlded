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

#ifndef BASEPATHRENDERER_H
#define BASEPATHRENDERER_H

#include <QPointF>
#include <QPolygonF>
#include <QRectF>

class World;

class BasePathRenderer
{
public:
    BasePathRenderer(World *world);
    virtual ~BasePathRenderer();

    virtual QPointF toScene(qreal x, qreal y, int level = 0) = 0;
    inline QPointF toScene(const QPointF &worldPt, int level = 0)
    { return toScene(worldPt.x(), worldPt.y(), level); }
    virtual QPolygonF toScene(const QRectF &worldRect, int level = 0) = 0;
    virtual QPolygonF toScene(const QPolygonF &worldPoly, int level) = 0;

    virtual QPointF toWorld(qreal x, qreal y, int level = 0) = 0;
    inline QPointF toWorld(const QPointF &scenePos, int level = 0)
    { return toWorld(scenePos.x(), scenePos.y(), level); }
    virtual QPolygonF toWorld(const QRectF &sceneRect, int level = 0);

    virtual QRectF sceneBounds(const QRectF &worldRect, int level = 0) = 0;

protected:
    World *mWorld;
};

#endif // BASEPATHRENDERER_H
