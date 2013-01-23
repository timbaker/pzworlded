/*
 * map.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2010, Andrew G. Crowell <overkill9999@gmail.com>
 *
 * This file is part of libtiled.
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

#include "map.h"

#include "layer.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;

Map::Map(Orientation orientation,
         int width, int height, int tileWidth, int tileHeight):
    mOrientation(orientation),
    mWidth(width),
    mHeight(height),
    mTileWidth(tileWidth),
    mTileHeight(tileHeight)
#ifdef ZOMBOID
    ,
    mCellsPerLevel(0,3)
#endif
{
}

Map::~Map()
{
    qDeleteAll(mLayers);
}

static QMargins maxMargins(const QMargins &a,
                           const QMargins &b)
{
    return QMargins(qMax(a.left(), b.left()),
                    qMax(a.top(), b.top()),
                    qMax(a.right(), b.right()),
                    qMax(a.bottom(), b.bottom()));
}

void Map::adjustDrawMargins(const QMargins &margins)
{
    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.
#ifdef ZOMBOID
    mDrawMargins = maxMargins(QMargins(margins.left(),
                                       qMax(0, margins.top() - mTileHeight),
                                       qMax(0, margins.right() - mTileWidth),
                                       margins.bottom()),
                              mDrawMargins);
#else
    mDrawMargins = maxMargins(QMargins(margins.left(),
                                       margins.top() - mTileHeight,
                                       margins.right() - mTileWidth,
                                       margins.bottom()),
                              mDrawMargins);
#endif
}

int Map::layerCount(Layer::Type type) const
{
    int count = 0;
    foreach (Layer *layer, mLayers)
       if (layer->type() == type)
           count++;
    return count;
}

QList<Layer*> Map::layers(Layer::Type type) const
{
    QList<Layer*> layers;
    foreach (Layer *layer, mLayers)
        if (layer->type() == type)
            layers.append(layer);
    return layers;
}

QList<ObjectGroup*> Map::objectGroups() const
{
    QList<ObjectGroup*> layers;
    foreach (Layer *layer, mLayers)
        if (ObjectGroup *og = layer->asObjectGroup())
            layers.append(og);
    return layers;
}

QList<TileLayer*> Map::tileLayers() const
{
    QList<TileLayer*> layers;
    foreach (Layer *layer, mLayers)
        if (TileLayer *tl = layer->asTileLayer())
            layers.append(tl);
    return layers;
}

void Map::addLayer(Layer *layer)
{
    adoptLayer(layer);
    mLayers.append(layer);
}

int Map::indexOfLayer(const QString &layerName, uint layertypes) const
{
    for (int index = 0; index < mLayers.size(); index++)
        if (layerAt(index)->name() == layerName
                && (layertypes & layerAt(index)->type()))
            return index;

    return -1;
}

void Map::insertLayer(int index, Layer *layer)
{
    adoptLayer(layer);
    mLayers.insert(index, layer);
}

void Map::adoptLayer(Layer *layer)
{
    layer->setMap(this);

    if (TileLayer *tileLayer = dynamic_cast<TileLayer*>(layer))
        adjustDrawMargins(tileLayer->drawMargins());
}

Layer *Map::takeLayerAt(int index)
{
    Layer *layer = mLayers.takeAt(index);
    layer->setMap(0);
    return layer;
}

void Map::addTileset(Tileset *tileset)
{
    mTilesets.append(tileset);
}

void Map::insertTileset(int index, Tileset *tileset)
{
    mTilesets.insert(index, tileset);
}

int Map::indexOfTileset(Tileset *tileset) const
{
    return mTilesets.indexOf(tileset);
}

void Map::removeTilesetAt(int index)
{
    mTilesets.removeAt(index);
}

void Map::replaceTileset(Tileset *oldTileset, Tileset *newTileset)
{
    const int index = mTilesets.indexOf(oldTileset);
    Q_ASSERT(index != -1);

    foreach (Layer *layer, mLayers)
        layer->replaceReferencesToTileset(oldTileset, newTileset);

    mTilesets.replace(index, newTileset);
}

bool Map::isTilesetUsed(Tileset *tileset) const
{
    foreach (const Layer *layer, mLayers)
        if (layer->referencesTileset(tileset))
            return true;

    return false;
}

#ifdef ZOMBOID
QList<Tileset *> Map::missingTilesets() const
{
    QList<Tileset*> tilesets;
    foreach (Tileset *tileset, mTilesets) {
        if (tileset->isMissing())
            tilesets += tileset;
    }
    return tilesets;
}

bool Map::hasMissingTilesets() const
{
    foreach (Tileset *tileset, mTilesets) {
        if (tileset->isMissing())
            return true;
    }
    return false;
}

bool Map::hasUsedMissingTilesets() const
{
    QSet<Tileset*> usedTilesets;
    foreach (TileLayer *tl, tileLayers())
        usedTilesets += tl->usedTilesets();

    foreach (Tileset *ts, mTilesets) {
        if (ts->isMissing() && usedTilesets.contains(ts))
            return true;
    }
    return false;
}

void Map::addTileLayerGroup(ZTileLayerGroup *tileLayerGroup)
{
    int arrayIndex = 0;
    foreach(ZTileLayerGroup *g1, tileLayerGroups()) {
        if (g1->level() >= tileLayerGroup->level())
            break;
        arrayIndex++;
    }
    mTileLayerGroups.insert(arrayIndex, tileLayerGroup);
}
#endif

Map *Map::clone() const
{
    Map *o = new Map(mOrientation, mWidth, mHeight, mTileWidth, mTileHeight);
    o->mDrawMargins = mDrawMargins;
    foreach (const Layer *layer, mLayers)
        o->addLayer(layer->clone());
    o->mTilesets = mTilesets;
    o->setProperties(properties());
    return o;
}


QString Tiled::orientationToString(Map::Orientation orientation)
{
    switch (orientation) {
    default:
    case Map::Unknown:
        return QLatin1String("unknown");
        break;
    case Map::Orthogonal:
        return QLatin1String("orthogonal");
        break;
    case Map::Isometric:
        return QLatin1String("isometric");
        break;
#ifdef ZOMBOID
    case Map::LevelIsometric:
        return QLatin1String("levelisometric");
        break;
#endif
    case Map::Staggered:
        return QLatin1String("staggered");
        break;
    }
}

Map::Orientation Tiled::orientationFromString(const QString &string)
{
    Map::Orientation orientation = Map::Unknown;
    if (string == QLatin1String("orthogonal")) {
        orientation = Map::Orthogonal;
    } else if (string == QLatin1String("isometric")) {
        orientation = Map::Isometric;
#ifdef ZOMBOID
    } else if (string == QLatin1String("levelisometric")) {
        orientation = Map::LevelIsometric;
#endif
    } else if (string == QLatin1String("staggered")) {
        orientation = Map::Staggered;
    }
    return orientation;
}

Map *Map::fromLayer(Layer *layer)
{
    Map *result = new Map(Unknown, layer->width(), layer->height(), 0, 0);
    result->addLayer(layer);
    return result;
}
