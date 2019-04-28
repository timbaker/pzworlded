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

#include "mapimage.h"
#include "mapimagemanager.h"
#include "preferences.h"
#include "world.h"
#include "worlddocument.h"
#include "worldscene.h"
#include "zoomable.h"

#include <qmath.h>
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
    QPointF cellPos = scene()->pixelToCellCoords(mapToScene(event->pos()));
    emit statusBarCoordinatesChanged(cellPos.x() * 300, cellPos.y() * 300);

    BaseGraphicsView::mouseMoveEvent(event);
}

/////

WorldMiniMapItem::WorldMiniMapItem(WorldScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{
    for (int i = 0; i < mScene->world()->bmps().size(); i++)
        bmpAdded(i);

    foreach (OtherWorld *otherWorld, mScene->otherWorlds()) {
        foreach (WorldBMP *bmp, otherWorld->mWorld->bmps()) {
            if (MapImage *mapImage = MapImageManager::instance().getMapImage(bmp->filePath())) {
                mImages[bmp] = mapImage;
            }
        }
    }

    connect(mScene->worldDocument(), &WorldDocument::worldResized,
            this, &WorldMiniMapItem::worldResized);
    connect(mScene->worldDocument(), &WorldDocument::bmpAdded,
            this, &WorldMiniMapItem::bmpAdded);
    connect(mScene->worldDocument(), &WorldDocument::bmpAboutToBeRemoved,
            this, &WorldMiniMapItem::bmpAboutToBeRemoved);
    connect(mScene->worldDocument(), &WorldDocument::bmpCoordsChanged,
            this, &WorldMiniMapItem::bmpCoordsChanged);

    Preferences *prefs = Preferences::instance();
    connect(prefs, &Preferences::showOtherWorldsChanged, this, &WorldMiniMapItem::showOtherWorlds);
}

QRectF WorldMiniMapItem::boundingRect() const
{
    QRect bounds = mScene->world()->bounds();
    Preferences *prefs = Preferences::instance();
    if (prefs->showOtherWorlds()) {
        foreach (OtherWorld *otherWorld, mScene->otherWorlds()) {
            bounds = bounds.united(otherWorld->adjustedBounds(mScene->world()));
        }
    }
    return mScene->boundingRect(bounds);
}

void WorldMiniMapItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *,
                             QWidget *)
{
    Preferences *prefs = Preferences::instance();

    foreach (WorldBMP *bmp, mImages.keys()) {
        if (MapImage *mapImage = mImages[bmp]) {
            const QImage &image = mapImage->miniMapImage();
            QRect bmpBounds = bmp->bounds();
            if (bmp->world() != mScene->world()) { // OtherWorld.mBMP
                if (!prefs->showOtherWorlds())
                    continue;
                bmpBounds.translate(bmp->world()->getGenerateLotsSettings().worldOrigin - mScene->world()->getGenerateLotsSettings().worldOrigin);
            }
            QRectF target = mScene->boundingRect(bmpBounds);
            QRectF source = QRect(QPoint(), image.size());
            painter->drawImage(target, image, source);
        }
    }

    QColor gridColor(Qt::black);
    QPen gridPen(gridColor);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

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
    if (MapImage *mapImage = MapImageManager::instance().getMapImage(bmp->filePath())) {
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

void WorldMiniMapItem::showOtherWorlds(bool show)
{
    prepareGeometryChange();
    update();
}
