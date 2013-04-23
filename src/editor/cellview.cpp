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

#include "cellview.h"

#include "cellscene.h"
#include "preferences.h"
#include "zoomable.h"

#include "maprenderer.h"

#include <QMouseEvent>

CellView::CellView(QWidget *parent) :
    BaseGraphicsView(true, parent)
{
    zoomable()->setScale(0.25);
}

CellScene *CellView::scene() const
{
    return static_cast<CellScene*>(mScene);
}

void CellView::mouseMoveEvent(QMouseEvent *event)
{
    QPoint tilePos = scene()->renderer()->pixelToTileCoordsInt(mapToScene(event->pos()));
    emit statusBarCoordinatesChanged(tilePos.x(), tilePos.y());

    BaseGraphicsView::mouseMoveEvent(event);
}

QRectF CellView::sceneRectForMiniMap() const
{
    if (!scene() || !scene()->renderer())
        return QRectF();
    return scene()->renderer()->boundingRect(QRect(0, 0, 300, 300));
}
