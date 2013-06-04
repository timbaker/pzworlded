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

#include "mapimagemanager.h"
#include "world.h"
#include "worlddocument.h"
#include "worldscene.h"
#include "zoomable.h"

#include <QGraphicsPixmapItem>
#include <QMouseEvent>

WorldView::WorldView(QWidget *parent)
    : BaseGraphicsView(NeverGL, parent)
{
    QVector<qreal> zoomFactors = zoomable()->zoomFactors();
    zoomable()->setZoomFactors(zoomFactors << 6.0 << 8.0);
}

void WorldView::setScene(WorldScene *scene)
{
    BaseGraphicsView::setScene(scene);

    mMiniMapItem = new WorldMiniMapItem(scene);
    addMiniMapItem(mMiniMapItem);
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

/////

WorldMiniMapItem::WorldMiniMapItem(WorldScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{
    for (int i = 0; i < mScene->world()->bmps().size(); i++)
        bmpAdded(i);

    connect(mScene->worldDocument(), SIGNAL(worldResized(QSize)),
            SLOT(worldResized()));
    connect(mScene->worldDocument(), SIGNAL(bmpAdded(int)),
            SLOT(bmpAdded(int)));
    connect(mScene->worldDocument(), SIGNAL(bmpAboutToBeRemoved(int)),
            SLOT(bmpAboutToBeRemoved(int)));
    connect(mScene->worldDocument(), SIGNAL(bmpCoordsChanged(int)),
            SLOT(bmpCoordsChanged(int)));
}

QRectF WorldMiniMapItem::boundingRect() const
{
    return mScene->boundingRect(mScene->world()->bounds());
}

void WorldMiniMapItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *,
                             QWidget *)
{
    foreach (WorldBMP *bmp, mImages.keys()) {
        if (MapImage *mapImage = mImages[bmp]) {
            const QImage &image = mapImage->image();
            QRectF target = mScene->boundingRect(bmp->bounds());
            QRectF source = QRect(QPoint(0, 0), image.size());
            painter->drawImage(target, image, source);
        }
    }

    QPolygonF polygon = mScene->cellRectToPolygon(mScene->world()->bounds());
    painter->drawPolygon(polygon);
}

void WorldMiniMapItem::worldResized()
{
    update();
}

void WorldMiniMapItem::bmpAdded(int index)
{
    WorldBMP *bmp = mScene->world()->bmps().at(index);
    if (MapImage *mapImage = MapImageManager::instance()->getMapImage(bmp->filePath())) {
        mImages[bmp] = mapImage;
    }
    update();
}

void WorldMiniMapItem::bmpAboutToBeRemoved(int index)
{
    WorldBMP *bmp = mScene->world()->bmps().at(index);
    mImages.remove(bmp);
    update();
}

void WorldMiniMapItem::bmpCoordsChanged(int index)
{
    Q_UNUSED(index)
    update();
}
