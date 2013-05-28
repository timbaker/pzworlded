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

#ifndef ORTHOPATHSCENE_H
#define ORTHOPATHSCENE_H

#include "basepathscene.h"

#include "basepathrenderer.h"

class OrthoPathRenderer : public BasePathRenderer
{
public:
    OrthoPathRenderer(PathWorld *world);

    using BasePathRenderer::toScene;
    QPointF toScene(qreal x, qreal y, int level);
    QPolygonF toScene(const QRectF &worldRect, int level);
    QPolygonF toScene(const QPolygonF &worldPoly, int level);

    QPointF toWorld(qreal x, qreal y, int level);

    QRectF sceneBounds(const QRectF &worldRect, int level);

};

class OrthoPathScene : public BasePathScene
{
public:
    OrthoPathScene(PathDocument *doc, QObject *parent = 0);

    bool isOrtho() const { return true; }
};

#endif // ORTHOPATHSCENE_H
