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

#include "mapmanager.h"
#include "path.h"
#include "pathworld.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include <QDebug>

TilePainter::TilePainter(PathWorld *world) :
    mWorld(world),
    mLayer(0),
    mTile(0),
    mTileAlias(0),
    mTileRule(0),
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
    mTileAlias = 0;
    mTileRule = 0;
}

void TilePainter::setTileAlias(WorldTileAlias *alias)
{
    mTile = 0;
    mTileAlias = alias;
    mTileRule = 0;
}

void TilePainter::setTileRule(WorldTileRule *rule)
{
    mTile = 0;
    mTileAlias = 0;
    mTileRule = rule;
    mLayer = rule ? mWorld->tileLayer(rule->mLayer) : 0;
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

    WorldTileLayer *conditionLayer = (mTileRule && mTileRule->mCondition) ?
                mWorld->tileLayer(mTileRule->mCondition->mLayer) : 0;

    for (int x1 = x; x1 < x + width; x1++) {
        for (int y1 = y; y1 < y + height; y1++) {
            if (mClipType == ClipRegion && !mClipRegion.contains(QPoint(x1, y1)))
                continue;
            if (mTileRule) {
                if (false && conditionLayer) {
                    if (mTileRule->mCondition->mTiles.contains(conditionLayer->getTile(x1, y1)))
                        continue;
                }
                mLayer->putTile(x1, y1, mTileRule->mTiles[qrand() % mTileRule->mTiles.size()]);
            } else if (mTileAlias)
                mLayer->putTile(x1, y1, mTileAlias->mTiles[qrand() % mTileAlias->mTiles.size()]);
            else
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

#include <QBitmap>
#include <QElapsedTimer>
void TilePainter::fill(WorldPath *path)
{
#if 0
    QBitmap b = polygonBitmap(path->polygon());
    QRect r = path->bounds().toAlignedRect();
    for (int x1 = r.x(); x1 < r.x() + r.width(); x1++) {
        for (int y1 = r.y(); y1 < r.y() + r.height(); y1++) {
            if (mClipType == ClipRegion && !mClipRegion.contains(QPoint(x1, y1)))
                continue;
            if (b.)
            if (mTileRule) {
                if (false && conditionLayer) {
                    if (mTileRule->mCondition->mTiles.contains(conditionLayer->getTile(x1, y1)))
                        continue;
                }
                mLayer->putTile(x1, y1, mTileRule->mTiles[qrand() % mTileRule->mTiles.size()]);
            } else if (mTileAlias)
                mLayer->putTile(x1, y1, mTileAlias->mTiles[qrand() % mTileAlias->mTiles.size()]);
            else
                mLayer->putTile(x1, y1, mTile);
        }
    }
#else
    QElapsedTimer timer;
    timer.start();
    QRegion region = path->region();

    // Don't call QRegion:contains() for every tile location, it's too slow!
    ClipType clipType = mClipType;
    if (mClipType == ClipRegion) {
        region &= mClipRegion;
        mClipType = ClipNone;
    }

    // Clip to tile sink bounds
    if (mLayer) {
        QRegion sinkRgn;
        foreach (WorldTileLayer::TileSink *sink, mLayer->mSinks)
            sinkRgn |= sink->bounds();
        region &= sinkRgn;
    }

    qint64 elapsed = timer.elapsed();
    if (elapsed > 100) qDebug() << "TilePainter::fill(WorldPath) REGION took" << elapsed << "ms!";
    timer.restart();
    fill(region);
    if (timer.elapsed() > 100) qDebug() << "TilePainter::fill(WorldPath) FILL took" << timer.elapsed() << "ms!" << "(#rects=" << region.rectCount() << ")";

    // Re-enable clipping region
    if (clipType == ClipRegion)
        mClipType = ClipRegion;
#endif
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

void TilePainter::drawMap(int wx, int wy, MapInfo *mapInfo)
{
    if (!mapInfo || !mapInfo->map() || !mapInfo->map()->tileLayerCount())
        return;

    WorldTileAlias *alias = mTileAlias;
    mTileAlias = 0;

    // This is the classic map composition mode.
    // If the map has a tile in 0_Floor, then erase all other layers/
    // If the map doesn't have a tile in 0_Floor, then erase all other layers
    // whose name doesn't start with 0_Floor.
    int n = mapInfo->map()->indexOfLayer(QLatin1String("0_Floor"), Tiled::Layer::TileLayerType);
    if (n >= 0) {
        Tiled::TileLayer *tl = mapInfo->map()->layerAt(n)->asTileLayer();
        QRegion floorRegion = tl->region().translated(wx, wy);
        foreach (WorldTileLayer *wtl, world()->tileLayers()) {
            if (wtl->mName.startsWith(QLatin1String("0_"))) {
                setLayer(wtl);
                erase(floorRegion);
            }
        }
        QRegion nonFloorRegion = QRegion(mapInfo->bounds()) - floorRegion;
        nonFloorRegion.translate(wx, wy);
        foreach (WorldTileLayer *wtl, world()->tileLayers()) {
            if (!wtl->mName.startsWith(QLatin1String("0_Floor"))) {
                setLayer(wtl);
                erase(nonFloorRegion);
            }
        }
    }

    foreach (Tiled::TileLayer *tl, mapInfo->map()->tileLayers()) {
        if (WorldTileLayer *wtl = world()->tileLayer(tl->name())) {
            setLayer(wtl);
            for (int y = 0; y < tl->height(); y++) {
                for (int x = 0; x < tl->width(); x++) {
                    Tiled::Tile *tile = tl->cellAt(x, y).tile;
                    if (!tile) continue;
                    if (WorldTileset *wts = world()->tileset(tile->tileset()->name())) { // FIXME: slow
                        setTile(wts->tileAt(tile->id())); // FIXME: assumes valid tile id
                        fill(wx + x, wy + y, 1, 1);
                    }
                }
            }
        }
    }

    mTileAlias = alias;
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
