/*
 * maplevel.h
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

#ifndef MAPLEVEL_H
#define MAPLEVEL_H

#include "layer.h"

namespace Tiled
{
class ZTileLayerGroup;

class TILEDSHARED_EXPORT MapLevel
{
public:
    MapLevel(Map *map, int level);

    ~MapLevel();

    static QString layerNameWithoutPrefix(const QString& name);
    static bool levelForLayer(const QString &layerName, int *levelPtr = nullptr);

    void setMap(Map *map)
    {
        mMap = map;
    }

    Map *map() const
    {
        return mMap;
    }

    int level() const
    {
        return mZ;
    }

    MapLevel *clone(Map *map) const;

    /**
     * Returns the number of layers of this map.
     */
    int layerCount() const
    { return mLayers.size(); }

    /**
     * Convenience function that returns the number of layers of this map that
     * match the given \a type.
     */
    int layerCount(Layer::Type type) const;

    int tileLayerCount() const
    { return layerCount(Layer::TileLayerType); }

    int objectGroupCount() const
    { return layerCount(Layer::ObjectGroupType); }

    int imageLayerCount() const
    { return layerCount(Layer::ImageLayerType); }

    /**
     * Returns the layer at the specified index.
     */
    Layer *layerAt(int index) const
    { return mLayers.at(index); }

    /**
     * Returns the list of layers of this map. This is useful when you want to
     * use foreach.
     */
    const QList<Layer*> &layers() const { return mLayers; }

    QList<Layer*> layers(Layer::Type type) const;
    QList<ObjectGroup*> objectGroups() const;
    QList<TileLayer*> tileLayers() const;

    /**
     * Adds a layer to this map.
     */
    void addLayer(Layer *layer);

    /**
     * Returns the index of the layer given by \a layerName, or -1 if no
     * layer with that name is found.
     *
     * The second optional parameter specifies the layer types which are
     * searched.
     */
    int indexOfLayer(const QString &layerName,
                     uint layerTypes = Layer::AnyLayerType) const;

    /**
     * Adds a layer to this map, inserting it at the given index.
     */
    void insertLayer(int index, Layer *layer);

    /**
     * Removes the layer at the given index from this map and returns it.
     * The caller becomes responsible for the lifetime of this layer.
     */
    Layer *takeLayerAt(int index);
#if 0
    void setTileLayerGroup(ZTileLayerGroup *tileLayerGroup);

    ZTileLayerGroup *tileLayerGroup()
    {
        return mTileLayerGroup;
    }
#endif
private:
    void adoptLayer(Layer *layer);

private:
    Map *mMap;
    int mZ;
    QList<Layer*> mLayers;
#if 0
    ZTileLayerGroup *mTileLayerGroup;
#endif
};

} // namespace Tiled

#endif // MAPLEVEL_H
