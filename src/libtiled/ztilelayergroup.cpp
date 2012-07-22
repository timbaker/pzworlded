/*
 * ztilelayergroup.cpp
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

#include "ztilelayergroup.h"

#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"

using namespace Tiled;

ZTileLayerGroup::ZTileLayerGroup(Map *map, int level)
    : mMap(map)
    , mLevel(level)
    , mVisible(true)
{
}

void ZTileLayerGroup::addTileLayer(TileLayer *layer, int index)
{
    Q_ASSERT(layer->level() == mLevel);
    if (mLayers.contains(layer))
        return;
    int arrayIndex = 0;
    foreach(int index1, mIndices) {
        if (index1 >= index)
            break;
        arrayIndex++;
    }
    mLayers.insert(arrayIndex, layer);
    mIndices.insert(arrayIndex, index);
    ++arrayIndex;
    while (arrayIndex < mIndices.count())
    {
        mIndices[arrayIndex] += 1;
        arrayIndex++;
    }
    layer->setGroup(this);
//    layer->setLevel(mLevel);
}

void ZTileLayerGroup::removeTileLayer(TileLayer *layer)
{
    if (mLayers.contains(layer)) {
        int arrayIndex = mLayers.indexOf(layer);
        mLayers.remove(arrayIndex);
        mIndices.remove(arrayIndex);
        while (arrayIndex < mIndices.count())
        {
            mIndices[arrayIndex] -= 1;
            arrayIndex++;
        }
        layer->setGroup(0);
//        layer->setLevel(0);
    }
}

QRect ZTileLayerGroup::bounds() const
{
    QRect r;
    foreach (TileLayer *tl, mLayers)
        r |= tl->bounds();
    return r;
}

// from map.cpp
static void maxMargins(const QMargins &a,
                           const QMargins &b,
                           QMargins &out)
{
    out.setLeft(qMax(a.left(), b.left()));
    out.setTop(qMax(a.top(), b.top()));
    out.setRight(qMax(a.right(), b.right()));
    out.setBottom(qMax(a.bottom(), b.bottom()));
}

QMargins ZTileLayerGroup::drawMargins() const
{
    QMargins m;
    foreach (TileLayer *tl, mLayers) {
        maxMargins(m, tl->drawMargins(), m);
    }
    return m;
}

QRectF ZTileLayerGroup::boundingRect(const MapRenderer *renderer) const
{
    QRectF boundingRect = renderer->boundingRect(bounds(), mLevel);

    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.

    QMargins drawMargins = this->drawMargins();
    boundingRect.adjust(-drawMargins.left(),
                -qMax(0, drawMargins.top() - mMap->tileHeight()),
                qMax(0, drawMargins.right() - mMap->tileWidth()),
                drawMargins.bottom());

    return boundingRect;
}
