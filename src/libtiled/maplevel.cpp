/*
 * maplevel.cpp
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

#include "maplevel.h"

using namespace Tiled;

MapLevel::MapLevel(int z)
    : mZ(z)
    , mVisible(true)
{

}

MapLevel::~MapLevel()
{
    qDeleteAll(mLayers);
}

int MapLevel::indexOfLayer(const QString &layerName, uint layertypes) const
{
    for (int index = 0; index < mLayers.size(); index++) {
        Layer *layer = layerAt(index);
        if ((layer->name() == layerName) && (layertypes & layer->type())) {
            return index;
        }
    }
    return -1;
}

void MapLevel::insertLayer(int index, Layer *layer)
{
    Q_ASSERT(layer->level() == z());
    mLayers.insert(index, layer);
}

Layer *MapLevel::takeLayerAt(int index)
{
    Layer *layer = mLayers.takeAt(index);
    return layer;
}

int MapLevel::layerCount(Layer::Type type) const
{
    int count = 0;
    for (Layer *layer : mLayers) {
       if (layer->type() == type) {
           count++;
       }
    }
    return count;
}

QList<Layer*> MapLevel::layers(Layer::Type type) const
{
    QList<Layer*> layers;
    for (Layer *layer : mLayers) {
        if (layer->type() == type) {
            layers.append(layer);
        }
    }
    return layers;
}

QList<ObjectGroup*> MapLevel::objectGroups() const
{
    QList<ObjectGroup*> layers;
    for (Layer *layer : mLayers) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            layers.append(og);
        }
    }
    return layers;
}

QList<TileLayer*> MapLevel::tileLayers() const
{
    QList<TileLayer*> layers;
    for (Layer *layer : mLayers) {
        if (TileLayer *tl = layer->asTileLayer()) {
            layers.append(tl);
        }
    }
    return layers;
}

void MapLevel::addLayer(Layer *layer)
{
    Q_ASSERT(layer != nullptr);
    Q_ASSERT(mLayers.contains(layer) == false);
    if (layer == nullptr) {
        return;
    }
    mLayers.append(layer);
}

void MapLevel::removeLayer(Layer *layer)
{
    mLayers.removeOne(layer);
}
