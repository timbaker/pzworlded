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

#include "tilepainter.h"

#include "path.h"
#include "pathworld.h"

TilePainter::TilePainter(PathWorld *world) :
    mWorld(world),
    mLayer(0),
    mTile(0),
    mClipType(ClipNone),
    mClipPath(0)
{
}

void TilePainter::setLayer(WorldTileLayer *layer)
{
    mLayer = layer;
}

void TilePainter::setTile(WorldTile *tile)
{
    mTile = tile;
}

void TilePainter::erase(int x, int y, int width, int height)
{
    WorldTile *oldTile = mTile;
    mTile = 0;
    fill(x, y, width, height);
    mTile = oldTile;
}

void TilePainter::erase(const QRect &r)
{
    WorldTile *oldTile = mTile;
    mTile = 0;
    fill(r);
    mTile = oldTile;
}

void TilePainter::erase(const QRegion &rgn)
{
    WorldTile *oldTile = mTile;
    mTile = 0;
    fill(rgn);
    mTile = oldTile;
}

void TilePainter::erase(WorldPath *path)
{
    WorldTile *oldTile = mTile;
    mTile = 0;
    fill(path);
    mTile = oldTile;
}

void TilePainter::fill(int x, int y, int width, int height)
{
    if (!mLayer)
        return;

    for (int x1 = x; x1 < x + width; x1++) {
        for (int y1 = y; y1 < y + height; y1++) {
            if (mClipType == ClipRegion && !mClipRegion.contains(QPoint(x1, y1)))
                continue;
            mLayer->putTile(x1, y1, mTile);
        }
    }
}

void TilePainter::fill(const QRect &r)
{
    fill(r.x(), r.y(), r.width(), r.height());
}

void TilePainter::fill(const QRegion &rgn)
{
    foreach (QRect r, rgn.rects())
        fill(r);
}

void TilePainter::fill(WorldPath *path)
{
    fill(path->region());
}

void TilePainter::strokePath(WorldPath *path, qreal thickness)
{
    QPolygonF polygon = ::strokePath(path, thickness);
    QRegion region = ::polygonRegion(polygon);
    fill(region);
}

#include <QLineF>
bool closeEnough(qreal p1, qreal p2)
{
    return qAbs(p1 - p2) <= 0.01;
}

void TilePainter::tracePath(WorldPath *path, qreal offset)
{
    if (path->nodeCount() > 100) return;
    QPolygonF fwd, bwd;
    offsetPath(path, 0.51, fwd, bwd);

    // FIXME:
    if (fwd.size() != bwd.size() || fwd.size() != path->nodeCount()) {
        return;
    }

    WorldTileLayer *layer1 = mWorld->tileLayerAt(1);
    WorldTileLayer *layer2 = mWorld->tileLayerAt(2);

    WorldTile *west = mWorld->tile(QLatin1String("street_trafficlines_01"), 0);
    WorldTile *north = mWorld->tile(QLatin1String("street_trafficlines_01"), 2);
    WorldTile *east = mWorld->tile(QLatin1String("street_trafficlines_01"), 4);
    WorldTile *south = mWorld->tile(QLatin1String("street_trafficlines_01"), 6);

    setLayer(layer1);
    for (int i = 0; i < fwd.size() - 1; i += 1) {
        QPointF p1 = fwd[i], p2 = fwd[i+1];
        if (closeEnough(p1.x(), p2.x())) { // vertical
            if (p1.y() > p2.y()) qSwap(p1.ry(), p2.ry());
            setTile((p1.x() >= path->nodeAt(i)->p.x()) ? west : east);
            fill(QRect(p1.x(), p1.y(), 1, p2.y() - p1.y()).normalized());
        } else if (closeEnough(p1.y(), p2.y())) { // horizontal
            if (p1.x() > p2.x()) qSwap(p1.rx(), p2.rx());
            setTile((p1.y() >= path->nodeAt(i)->p.y()) ? north : south);
            fill(QRect(p1.x(), p1.y(), p2.x() - p1.x(), 1).normalized());
        }
    }

    setLayer(layer2);
    for (int i = 0; i < bwd.size() - 1; i += 1) {
        QPointF p1 = bwd[i], p2 = bwd[i+1];
        if (closeEnough(p1.x(), p2.x())) { // vertical
            if (p1.y() > p2.y()) qSwap(p1.ry(), p2.ry());
            setTile((p1.x() >= path->nodeAt(i)->p.x()) ? west : east);
            fill(QRect(p1.x(), p1.y(), 1, p2.y() - p1.y()).normalized());
        } else if (closeEnough(p1.y(), p2.y())) { // horizontal
            if (p1.x() > p2.x()) qSwap(p1.rx(), p2.rx());
            setTile((p1.y() >= path->nodeAt(i)->p.y()) ? north : south);
            fill(QRect(p1.x(), p1.y(), p2.x() - p1.x(), 1).normalized());
        }
    }
}

void TilePainter::noClip()
{
    mClipType = ClipNone;
}

void TilePainter::setClip(const QRect &rect)
{
    mClipType = ClipRect;
    mClipRect = rect;
}

void TilePainter::setClip(WorldPath *path)
{
    mClipType = ClipPath;
    mClipPath = path; // FIXME: clear this if path is removed
}

void TilePainter::setClip(const QRegion &region)
{
    mClipType = ClipRegion;
    mClipRegion = region;
}
