/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "worldview.h"

#include "world.h"
#include "worldscene.h"

#include <QMouseEvent>

WorldView::WorldView(QWidget *parent)
    : BaseGraphicsView(parent)
{
}

void WorldView::setScene(WorldScene *scene)
{
    BaseGraphicsView::setScene(scene);

    QPolygonF polygon = scene->cellRectToPolygon(scene->world()->bounds());
    QGraphicsPolygonItem *item = new QGraphicsPolygonItem(polygon);
    addMiniMapItem(item);
}

WorldScene *WorldView::scene() const
{
    return static_cast<WorldScene*>(mScene);
}

void WorldView::mouseMoveEvent(QMouseEvent *event)
{
    QPoint tilePos = scene()->pixelToCellCoordsInt(mapToScene(event->pos()));
    emit statusBarCoordinatesChanged(tilePos.x(), tilePos.y());

    BaseGraphicsView::mouseMoveEvent(event);
}
