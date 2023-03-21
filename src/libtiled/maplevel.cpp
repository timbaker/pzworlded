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

#include "map.h"
#include "tilelayer.h"

using namespace Tiled;

MapLevel::MapLevel(Map *map, int level)
    : mMap(map)
    , mZ(level)
{

}

MapLevel::~MapLevel()
{
    qDeleteAll(mLayers);
}

QString MapLevel::layerNameWithoutPrefix(const QString &name)
{
    int pos = name.indexOf(QLatin1Char('_')) + 1; // Could be "-1 + 1 == 0"
    return name.mid(pos);
}

bool MapLevel::levelForLayer(const QString &layerName, int *levelPtr)
{
    if (levelPtr) (*levelPtr) = 0;

    int pos = layerName.indexOf(QLatin1Char('_'));
    if (pos == -1) {
        return false;
    }

    // See if the layer name matches "0_foo" or "1_bar" etc.
    QStringList sl = layerName.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        uint level = sl[0].toUInt(&conversionOK);
        if (levelPtr) (*levelPtr) = level;
        return conversionOK;
    }
    return false;
}

MapLevel *MapLevel::clone(Map *map) const
{
    MapLevel *copy = new MapLevel(map, level());
    for (Layer *layer : layers()) {
        copy->addLayer(layer->clone());
    }
    return copy;
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
    adoptLayer(layer);
    mLayers.append(layer);
}

int MapLevel::indexOfLayer(const QString &layerName, uint layertypes) const
{
    QString layerName2 = layerName.trimmed();
    int level;
    if (levelForLayer(layerName2, &level)) {
        layerName2 = layerNameWithoutPrefix(layerName2);
    }
    for (int index = 0; index < mLayers.size(); index++) {
        Layer *layer = layerAt(index);
        if ((layer->name() == layerName2) && (layertypes & layer->type())) {
            return index;
        }
    }
    return -1;
}

void MapLevel::insertLayer(int index, Layer *layer)
{
    adoptLayer(layer);
    mLayers.insert(index, layer);
}

void MapLevel::adoptLayer(Layer *layer)
{
    map()->adoptLayer(layer);
}

Layer *MapLevel::takeLayerAt(int index)
{
    Layer *layer = mLayers.takeAt(index);
#ifdef ZOMBOID
    Q_ASSERT(layer->map() == map());
#endif
    layer->setMap(nullptr);
#ifdef ZOMBOID
    foreach (Tileset *ts, layer->usedTilesets()) {
        map()->removeTilesetUser(ts);
    }
#endif
    return layer;
}
