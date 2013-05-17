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

void TilePainter::erase(WorldPath::Path *path)
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

void TilePainter::fill(WorldPath::Path *path)
{
    fill(path->region());
}

void TilePainter::strokePath(WorldPath::Path *path, qreal thickness)
{
    QPolygonF polygon = WorldPath::strokePath(path, thickness);
    QRegion region = WorldPath::polygonRegion(polygon);
    fill(region);
}

#include <QLineF>
void TilePainter::tracePath(WorldPath::Path *path, qreal offset)
{
    QPolygonF polygon = WorldPath::offsetPath(path, offset);
    for (int i = 0; i < polygon.size() - 1; i += 2) {
        QPointF p1 = polygon[i], p2 = polygon[i+1];
        QLineF line1(p1, p2);
    }
}
