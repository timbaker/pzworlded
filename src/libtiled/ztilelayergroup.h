/*
 * ztilelayergroup.h
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ZTILELAYERGROUP_H
#define ZTILELAYERGROUP_H

#include "tiled_global.h"

#include <QVector>
#include <QRect>
#include <QMargins>

namespace Tiled {

class Cell;
class Map;
class MapRenderer;
class Tile;
class TileLayer;

enum class ZTileRenderOrder
{
    Floor,
    WestWall,
    NorthWall,
    West,
    North,
    East,
    South,
    EastWall,
    SouthWall
};

class TILEDSHARED_EXPORT ZTileRenderInfo
{
public:
    ZTileRenderInfo()
        : mTile(nullptr)
        , mOpacity(qreal(1.0))
        , mOrder(ZTileRenderOrder::West)
    {

    }

    ZTileRenderInfo(Tiled::Tile* tile)
        : mTile(tile)
        , mOpacity(qreal(1.0))
        , mOrder(ZTileRenderOrder::West)
    {

    }

    Tiled::Tile* mTile;
    qreal mOpacity;
    QPoint mOffset; // Rendering offset, to move north walls onto the south edge, for example.
    ZTileRenderOrder mOrder;
};

class TILEDSHARED_EXPORT ZTileLayerGroup
{
public:

    ZTileLayerGroup(Map *map, int level);
    ZTileLayerGroup(const ZTileLayerGroup& other) = delete;
    ZTileLayerGroup &operator=(const ZTileLayerGroup& other) = delete;
    virtual ~ZTileLayerGroup() {}

    virtual void addTileLayer(TileLayer *layer, int index);
    virtual void removeTileLayer(TileLayer *layer);

    // Layer
    virtual QRect bounds() const;

    // TileLayer
    virtual QMargins drawMargins() const;

    virtual QRectF boundingRect(const MapRenderer *renderer) const;

    virtual bool orderedCellsAt(const Tiled::MapRenderer *renderer, const QPoint &point, QVector<const Cell*>& cells,
                                QVector<qreal> &opacities) const = 0;
    virtual bool orderedTilesAt(const Tiled::MapRenderer *renderer, const QPoint &point,
                                QVector<ZTileRenderInfo>& tileInfo) const = 0;
    virtual void prepareDrawing(const MapRenderer *renderer, const QRect &rect) = 0;

    void setLevel(int level) { mLevel = level; }
    int level() const { return mLevel; }

    const QVector<TileLayer*> &layers() const { return mLayers; }
    int layerCount() const { return mLayers.count(); }

    bool isVisible() const;
    void setVisible(bool visible);

    Map *mMap;
    QVector<TileLayer*> mLayers;
    QVector<int> mIndices;
    int mLevel;
};

} // namespace Tiled

#endif // ZTILELAYERGROUP_H
