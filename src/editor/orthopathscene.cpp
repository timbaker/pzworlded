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

#include "orthopathscene.h"

OrthoPathScene::OrthoPathScene(PathDocument *doc, QObject *parent) :
    BasePathScene(doc, parent)
{
    setRenderer(new OrthoPathRenderer(world()));

    setDocument(doc);
}

void OrthoPathScene::setTool(AbstractTool *tool)
{
}

/////

OrthoPathRenderer::OrthoPathRenderer(World *world) :
    BasePathRenderer(world)
{
}

QPointF OrthoPathRenderer::toScene(qreal x, qreal y, int level)
{
    Q_UNUSED(level);
    return QPointF(x, y);
}

QPolygonF OrthoPathRenderer::toScene(const QRectF &worldRect, int level)
{
    QPolygonF pf;
    pf << toScene(worldRect.topLeft(), level);
    pf << toScene(worldRect.topRight(), level);
    pf << toScene(worldRect.bottomRight(), level);
    pf << toScene(worldRect.bottomLeft(), level);
    pf += pf.first();
    return pf;
}

QPolygonF OrthoPathRenderer::toScene(const QPolygonF &worldPoly, int level)
{
    QPolygonF pf;
    foreach (QPointF p, worldPoly)
        pf += toScene(p, level);
    return pf;
}

QPointF OrthoPathRenderer::toWorld(qreal x, qreal y, int level)
{
    Q_UNUSED(level);
    return QPointF(x, y);
}

QRectF OrthoPathRenderer::sceneBounds(const QRectF &worldRect, int level)
{
    return toScene(worldRect, level).boundingRect();
}
