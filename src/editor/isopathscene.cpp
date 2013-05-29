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

#include "isopathscene.h"

#include "pathworld.h"

IsoPathScene::IsoPathScene(PathDocument *doc, QObject *parent) :
    BasePathScene(doc, parent)
{
    setRenderer(new IsoPathRenderer(world()));
    setDocument(doc);
}

/////

IsoPathRenderer::IsoPathRenderer(PathWorld *world) :
    BasePathRenderer(world)
{
}

QPointF IsoPathRenderer::toScene(qreal x, qreal y, int level)
{
    const qreal tileWidth = 64;
    const qreal tileHeight = 32;
    const int worldHeight = mWorld->height();
    const int levelHeight = 3;
    const int maxLevel = mWorld->maxLevel();

    const int originX = worldHeight * tileWidth / 2; // top-left corner
    const int originY = levelHeight * (maxLevel - level) * tileHeight;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY);
}

QPolygonF IsoPathRenderer::toScene(const QRectF &worldRect, int level)
{
    QPolygonF pf;
    pf << toScene(worldRect.topLeft(), level);
    pf << toScene(worldRect.topRight(), level);
    pf << toScene(worldRect.bottomRight(), level);
    pf << toScene(worldRect.bottomLeft(), level);
    pf += pf.first();
    return pf;
}

QPolygonF IsoPathRenderer::toScene(const QPolygonF &worldPoly, int level)
{
    QPolygonF pf;
    foreach (QPointF p, worldPoly)
        pf += toScene(p, level);
    return pf;
}

QPointF IsoPathRenderer::toWorld(qreal x, qreal y, int level)
{
    const qreal tileWidth = 64;
    const qreal tileHeight = 32;
    const qreal ratio = tileWidth / tileHeight;

    const int worldHeight = mWorld->height();
    const int levelHeight = 3;
    const int maxLevel = mWorld->maxLevel();

    x -= worldHeight * tileWidth / 2;
    y -= levelHeight * (maxLevel - level) * tileHeight;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QRectF IsoPathRenderer::sceneBounds(const QRectF &worldRect, int level)
{
    return toScene(worldRect, level).boundingRect();
}
