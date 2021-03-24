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

class Layer;

class TILEDSHARED_EXPORT MapLevel
{
public:
    MapLevel(int z);

    ~MapLevel();

    void setZ(int z)
    {
        mZ = z;
    }

    int z() const
    {
        return mZ;
    }

    const QList<Layer*> &layers() const
    {
        return mLayers;
    }

    QList<Layer *> layers(Layer::Type type) const;
    QList<ObjectGroup*> objectGroups() const;
    QList<TileLayer*> tileLayers() const;

    int layerCount() const
    {
        return mLayers.count();
    }

    Layer *layerAt(int index) const
    {
        if (index < 0 || index >= mLayers.size()) {
            return nullptr;
        }
        return mLayers.at(index);
    }

    void addLayer(Layer *layer);

    void removeLayer(Layer *layer);

    int indexOfLayer(const QString &layerName, uint layertypes = Layer::AnyLayerType) const;

    void insertLayer(int index, Layer *layer);

    Layer *takeLayerAt(int index);

    int layerCount(Layer::Type type) const;

    int tileLayerCount() const
    { return layerCount(Layer::TileLayerType); }

    int objectGroupCount() const
    { return layerCount(Layer::ObjectGroupType); }

    int imageLayerCount() const
    { return layerCount(Layer::ImageLayerType); }

    bool isVisible() const
    {
        return mVisible;
    }

    void setVisible(bool visible)
    {
        mVisible = visible;
    }

protected:
    int mZ;
    QList<Layer*> mLayers;
    bool mVisible;
};

}

#endif // MAPLEVEL_H
