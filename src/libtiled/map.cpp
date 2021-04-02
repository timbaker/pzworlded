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
#include "maplevel.h"
#include "objectgroup.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QRandomGenerator>

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
    mCellsPerLevel(0,3),
    mBmpMain(mWidth, mHeight),
    mBmpVeg(mWidth, mHeight)
#endif
{
    mLevels.resize(8);
    for (int i = 0; i < mLevels.size(); i++) {
        mLevels[i] = new MapLevel(i);
    }
}

Map::~Map()
{
    qDeleteAll(mLevels);
#ifdef ZOMBOID
    qDeleteAll(mNoBlend);
#endif
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

int Map::layerCount() const
{
    int count = 0;
    for (MapLevel *level : mLevels) {
        count += level->layerCount();
    }
    return count;
}

int Map::layerCount(Layer::Type type) const
{
    int count = 0;
    for (MapLevel *level : mLevels) {
        count += level->layerCount(type);
    }
    return count;
}

Layer *Map::layerAt(int z, int index) const
{
    if (MapLevel *mapLevel = levelAt(z)) {
        return mapLevel->layerAt(index);
    }
    return nullptr;
}

QList<Layer *> Map::layers() const
{
    QList<Layer*> layers;
    for (MapLevel *level : mLevels) {
        layers += level->layers();
    }
    return layers;
}

QList<Layer*> Map::layers(Layer::Type type) const
{
    QList<Layer*> layers;
    for (MapLevel *level : mLevels) {
        layers += level->layers(type);
    }
    return layers;
}

QList<ObjectGroup*> Map::objectGroups() const
{
    QList<ObjectGroup*> layers;
    for (MapLevel *level : mLevels) {
        layers += level->objectGroups();
    }
    return layers;
}

QList<TileLayer*> Map::tileLayers() const
{
    QList<TileLayer*> layers;
    for (MapLevel *level : mLevels) {
        layers += level->tileLayers();
    }
    return layers;
}

void Map::addLayer(Layer *layer)
{
    adoptLayer(layer);
    levelAt(layer->level())->addLayer(layer);
}

void Map::insertLayer(int index, Layer *layer)
{
    adoptLayer(layer);
    levelAt(layer->level())->insertLayer(index, layer);
}

Layer *Map::takeLayerAt(int z, int index)
{
    if (MapLevel *mapLevel = levelAt(z)) {
        if (Layer *layer = mapLevel->takeLayerAt(index)) {
            layer->setMap(nullptr);
            for (Tileset *ts : layer->usedTilesets()) {
                removeTilesetUser(ts);
            }
            return layer;
        }
    }
    return nullptr;
}

void Map::adoptLayer(Layer *layer)
{
    layer->setMap(this);

    if (TileLayer *tileLayer = dynamic_cast<TileLayer*>(layer))
        adjustDrawMargins(tileLayer->drawMargins());

#ifdef ZOMBOID
    foreach (Tileset *ts, layer->usedTilesets())
        addTilesetUser(ts);
#endif
}

void Map::removeLayer(Layer *layer)
{
#ifdef ZOMBOID
    Q_ASSERT(layer->map() == this);
#endif
    layer->setMap(nullptr);
#ifdef ZOMBOID
    for (Tileset *ts : layer->usedTilesets())
        removeTilesetUser(ts);
#endif
    levelAt(layer->level())->removeLayer(layer);
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

    for (MapLevel *mapLevel : mLevels) {
        for (Layer *layer : mapLevel->layers()) {
            layer->replaceReferencesToTileset(oldTileset, newTileset);
        }
    }

    mTilesets.replace(index, newTileset);
}

bool Map::isTilesetUsed(Tileset *tileset) const
{
#ifdef ZOMBOID
    return mUsedTilesets.contains(tileset);
#else
    foreach (const Layer *layer, mLayers)
        if (layer->referencesTileset(tileset))
            return true;

    return false;
#endif
}

#ifdef ZOMBOID
QSet<Tileset *> Map::usedTilesets() const
{
    QList<Tileset*> keys = mUsedTilesets.keys();
    return QSet<Tileset*>(keys.constBegin(), keys.constEnd());
}

void Map::addTilesetUser(Tileset *tileset)
{
    mUsedTilesets[tileset]++;
}

void Map::removeTilesetUser(Tileset *tileset)
{
    Q_ASSERT(mUsedTilesets.contains(tileset));
    Q_ASSERT(mUsedTilesets[tileset] > 0);
    if (--mUsedTilesets[tileset] <= 0)
        mUsedTilesets.remove(tileset);
}

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
#if 1
    foreach (Tileset *ts, mUsedTilesets.keys())
        if (ts->isMissing())
            return true;
#else
    QSet<Tileset*> usedTilesets;
    foreach (TileLayer *tl, tileLayers())
        usedTilesets += tl->usedTilesets();

    foreach (Tileset *ts, mTilesets) {
        if (ts->isMissing() && usedTilesets.contains(ts))
            return true;
    }
#endif
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

int Map::levelCount() const
{
    return mLevels.size();
}

MapLevel *Map::levelAt(int z) const
{
    if (z < 0 || z >= mLevels.size()) {
        return nullptr;
    }
    return mLevels[z];
}

MapNoBlend *Map::noBlend(const QString &layerName)
{
    if (!mNoBlend.contains(layerName))
        mNoBlend[layerName] = new MapNoBlend(layerName, width(), height());
    return mNoBlend[layerName];
}
#endif // ZOMBOID

Map *Map::clone() const
{
    Map *o = new Map(mOrientation, mWidth, mHeight, mTileWidth, mTileHeight);
    o->mDrawMargins = mDrawMargins;
    for (const MapLevel *mapLevel : mLevels) {
        for (const Layer *layer : mapLevel->layers()) {
            o->addLayer(layer->clone());
        }
    }
    o->mTilesets = mTilesets;
#ifdef ZOMBOID
    Q_ASSERT(o->mUsedTilesets == mUsedTilesets);
    o->mUsedTilesets = mUsedTilesets; // not needed because of addLayer() above
    o->mBmpMain = mBmpMain;
    o->mBmpVeg = mBmpVeg;
    o->mBmpSettings.clone(mBmpSettings);
    for (MapNoBlend *noBlend : mNoBlend) {
        o->noBlend(noBlend->layerName())->replace(noBlend);
    }
#endif
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

#ifdef ZOMBOID
MapRands::MapRands(int width, int height, uint seed) :
    mSeed(seed)
{
    setSize(width, height);
}

void MapRands::setSize(int width, int height)
{
    QRandomGenerator prng(mSeed);
    resize(width);
    for (int x = 0; x < width; x++) {
        (*this)[x].resize(height);
        for (int y = 0; y < height; y++)
            (*this)[x][y] = prng.generate();
    }
}

void MapRands::setSeed(uint seed)
{
    mSeed = seed;
    setSize(size(), at(0).size());
}

/////

QString BmpBlend::dirAsString() const
{
    QMap<BmpBlend::Direction,QString> dirMap;
    dirMap[BmpBlend::N] = QLatin1String("n");
    dirMap[BmpBlend::S] = QLatin1String("s");
    dirMap[BmpBlend::E] = QLatin1String("e");
    dirMap[BmpBlend::W] = QLatin1String("w");
    dirMap[BmpBlend::NW] = QLatin1String("nw");
    dirMap[BmpBlend::SW] = QLatin1String("sw");
    dirMap[BmpBlend::NE] = QLatin1String("ne");
    dirMap[BmpBlend::SE] = QLatin1String("se");
    return dirMap[dir];
}

/////

BmpSettings::BmpSettings()
    : mBlendEdgesEverywhere(false)
{
}

BmpSettings::~BmpSettings()
{
    qDeleteAll(mAliases);
    qDeleteAll(mRules);
    qDeleteAll(mBlends);
}

void BmpSettings::setAliases(const QList<BmpAlias *> &aliases)
{
    qDeleteAll(mAliases);
    mAliases = aliases;
}

QList<BmpAlias *> BmpSettings::aliasesCopy() const
{
    QList<BmpAlias*> ret;
    foreach (BmpAlias *alias, mAliases)
        ret += new BmpAlias(alias);
    return ret;
}

void BmpSettings::setRules(const QList<BmpRule*> &rules)
{
    qDeleteAll(mRules);
    mRules = rules;
}

QList<BmpRule *> BmpSettings::rulesCopy() const
{
    QList<BmpRule *> ret;
    foreach (BmpRule *rule, mRules)
        ret += new BmpRule(rule);
    return ret;
}

void BmpSettings::setBlends(const QList<BmpBlend*> &blends)
{
    qDeleteAll(mBlends);
    mBlends = blends;
}

QList<BmpBlend *> BmpSettings::blendsCopy() const
{
    QList<BmpBlend *> ret;
    for (BmpBlend *blend : mBlends) {
        ret += new BmpBlend(blend);
    }
    return ret;
}

void BmpSettings::clone(const BmpSettings &other)
{
    mRulesFileName = other.mRulesFileName;
    mBlendsFileName = other.mBlendsFileName;
    mAliases = other.aliasesCopy();
    mRules = other.rulesCopy();
    mBlends = other.blendsCopy();
    mBlendEdgesEverywhere = other.mBlendEdgesEverywhere;
}

/////

MapBmp::MapBmp(int width, int height) :
    mImage(width, height, QImage::Format_ARGB32),
    mRands(width, height, 1)
{
    mImage.fill(Qt::black);
}

void MapBmp::resize(const QSize &size, const QPoint &offset)
{
    QImage newImage(size, QImage::Format_ARGB32);
    newImage.fill(Qt::black);

    // Copy over the preserved part
    const int startX = qMax(0, -offset.x());
    const int startY = qMax(0, -offset.y());
    const int endX = qMin(width(), size.width() - offset.x());
    const int endY = qMin(height(), size.height() - offset.y());

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            newImage.setPixel(x + offset.x(), y + offset.y(), mImage.pixel(x, y));
        }
    }

    mImage = newImage;

    mRands.setSize(size.width(), size.height());
}

QList<QRgb> MapBmp::colors() const
{
    const QRgb black = qRgb(0, 0, 0);
    QSet<QRgb> colorSet;
    for (int y = 0; y < height(); y++) {
        for (int x = 0; x < width(); x++) {
            QRgb rgb = mImage.pixel(x, y);
            if (rgb != black)
                colorSet += rgb;
        }
    }
    return QList<QRgb>(colorSet.constBegin(), colorSet.constEnd());
}

/////

MapNoBlend::MapNoBlend(const QString &layerName, int width, int height) :
    mLayerName(layerName),
    mWidth(width),
    mHeight(height),
    mBits(width * height)
{
}

void MapNoBlend::replace(const MapNoBlend *other)
{
    mWidth = other->width();
    mHeight = other->height();
    mBits = other->mBits;
}

void MapNoBlend::replace(const MapNoBlend *other, const QRegion &rgn)
{
    QRect bounds = rgn.boundingRect();
    Q_ASSERT(other->width() == bounds.width() && other->height() == bounds.height());
    QRegion clipped = rgn & QRect(0, 0, width(), height());

    for (const QRect &r : clipped) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.x(); x <= r.right(); x++) {
                set(x, y, other->get(x - bounds.x(),  y - bounds.y()));
            }
        }
    }
}

MapNoBlend MapNoBlend::copy(const QRegion &rgn)
{
    QRect bounds = rgn.boundingRect();
    MapNoBlend copy(layerName(), bounds.width(), bounds.height());
    QRegion clipped = rgn & QRect(0, 0, width(), height());
    for (const QRect &r : clipped) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.x(); x <= r.right(); x++) {
                copy.set(x - bounds.x(), y - bounds.y(), get(x, y));
            }
        }
    }
    return copy;
}

#endif // ZOMBOID
