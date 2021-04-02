/*
 * map.h
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

#ifndef MAP_H
#define MAP_H

#include "layer.h"
#include "object.h"

#ifdef ZOMBOID
#include <QBitArray>
#endif
#include <QList>
#include <QMargins>
#include <QSize>

namespace Tiled {

class MapLevel;
class Tile;
class Tileset;
class ObjectGroup;
#ifdef ZOMBOID
class ZTileLayerGroup;
#endif

#ifdef ZOMBOID
/**
  * This class represents a grid of random numbers for each cell in a Map.
  * The random numbers are used by the BmpBlender class.
  */
class TILEDSHARED_EXPORT MapRands : public QVector<QVector<quint32> >
{
public:
    MapRands(int width, int height, uint seed);
    void setSize(int width, int height);
    void setSeed(uint seed);
    uint seed() const { return mSeed; }
private:
    uint mSeed;
};

class TILEDSHARED_EXPORT MapBmp
{
public:
    MapBmp(int width, int height);

    QImage &rimage() { return mImage; }
    MapRands &rrands() { return mRands; }

    const QImage image() const { return mImage; }
    const MapRands &rands() const { return mRands; }

    int width() const { return mImage.width(); }
    int height() const { return mImage.height(); }

    QRgb pixel(const QPoint &pt) const { return mImage.pixel(pt); }
    QRgb pixel(int x, int y) const { return mImage.pixel(x, y); }
    void setPixel(int x, int y, QRgb rgb) { mImage.setPixel(x, y, rgb); }

    quint32 rand(int x, int y) { return mRands[x][y]; }

    void resize(const QSize &size, const QPoint &offset);
    void merge(const QPoint &pos, const MapBmp *other);

    QList<QRgb> colors() const;

    QImage mImage;
    MapRands mRands;
};

class TILEDSHARED_EXPORT MapNoBlend
{
public:
    MapNoBlend(const QString &layerName, int width, int height);

    QString layerName() const { return mLayerName; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    void set(int x, int y, bool noblend) { mBits.setBit(x + y * mWidth, noblend); }
    void set(const QPoint &p, bool noblend) { set(p.x(), p.y(), noblend); }

    bool get(int x, int y) const { return mBits.testBit(x + y * mWidth); }
    bool get(const QPoint &p) const { return get(p.x(), p.y()); }

    void replace(const MapNoBlend *other);

    void replace(const MapNoBlend *other, const QRegion &rgn);

    MapNoBlend copy(const QRegion &rgn);

private:
    QString mLayerName;
    int mWidth;
    int mHeight;
    QBitArray mBits;
};

class TILEDSHARED_EXPORT BmpAlias
{
public:
    QString name;
    QStringList tiles;

    BmpAlias(const BmpAlias *other) :
        name(other->name),
        tiles(other->tiles)
    {}
    BmpAlias(const QString &name, const QStringList &tiles) :
        name(name),
        tiles(tiles)
    {}
};

class TILEDSHARED_EXPORT BmpRule
{
public:
    QString label;
    int bitmapIndex;
    QRgb color;
    QStringList tileChoices;
    QString targetLayer;
    QRgb condition;

    BmpRule(const BmpRule *other) :
        label(other->label),
        bitmapIndex(other->bitmapIndex),
        color(other->color),
        tileChoices(other->tileChoices),
        targetLayer(other->targetLayer),
        condition(other->condition)
    {}
    BmpRule(const QString &label, int bitmapIndex, QRgb col,
            const QStringList &tiles, const QString &layer, QRgb condition) :
        label(label),
        bitmapIndex(bitmapIndex),
        color(col),
        tileChoices(tiles),
        targetLayer(layer),
        condition(condition)
    {}
    BmpRule(const QString &label, int bitmapIndex, QRgb col, QString tile,
            QString layer, QRgb condition);
};

class TILEDSHARED_EXPORT BmpBlend
{
public:
    enum Direction {
        Unknown,
        N,
        S,
        E,
        W,
        NW,
        NE,
        SW,
        SE
    };

    QString targetLayer;
    QString mainTile;
    QString blendTile;
    Direction dir;
    QStringList ExclusionList;
    QStringList exclude2;

    BmpBlend(const BmpBlend *other) :
        targetLayer(other->targetLayer),
        mainTile(other->mainTile),
        blendTile(other->blendTile),
        dir(other->dir),
        ExclusionList(other->ExclusionList),
        exclude2(other->exclude2)
    {}
    BmpBlend(const QString &layer, const QString &main, const QString &blend,
          Direction dir, const QStringList &exclusions, const QStringList &exclude2) :
        targetLayer(layer), mainTile(main), blendTile(blend), dir(dir),
        ExclusionList(exclusions), exclude2(exclude2)
    {}

    QString dirAsString() const;
};

class TILEDSHARED_EXPORT BmpSettings
{
public:
    BmpSettings();
    ~BmpSettings();

    void setRulesFile(const QString &fileName)
    { mRulesFileName = fileName; }
    QString rulesFile() const
    { return mRulesFileName; }

    void setBlendsFile(const QString &fileName)
    { mBlendsFileName = fileName; }
    QString blendsFile() const
    { return mBlendsFileName; }

    void setAliases(const QList<BmpAlias*> &aliases);
    const QList<BmpAlias*> &aliases() const
    { return mAliases; }
    QList<BmpAlias*> aliasesCopy() const;

    void setRules(const QList<BmpRule*> &rules);
    const QList<BmpRule*> &rules() const
    { return mRules; }
    QList<BmpRule*> rulesCopy() const;

    void setBlends(const QList<BmpBlend*> &blends);
    const QList<BmpBlend*> &blends() const
    { return mBlends; }
    QList<BmpBlend*> blendsCopy() const;

    void setBlendEdgesEverywhere(bool everywhere)
    { mBlendEdgesEverywhere = everywhere; }
    bool isBlendEdgesEverywhere() const
    { return mBlendEdgesEverywhere; }

    void clone(const BmpSettings &other);

private:
    QString mRulesFileName;
    QString mBlendsFileName;
    QList<BmpAlias*> mAliases;
    QList<BmpRule*> mRules;
    QList<BmpBlend*> mBlends;
    bool mBlendEdgesEverywhere;
};

#endif // ZOMBOID

/**
 * A tile map. Consists of a stack of layers, each can be either a TileLayer
 * or an ObjectGroup.
 *
 * It also keeps track of the list of referenced tilesets.
 */
class TILEDSHARED_EXPORT Map : public Object
{
public:
    /**
     * The orientation of the map determines how it should be rendered. An
     * Orthogonal map is using rectangular tiles that are aligned on a
     * straight grid. An Isometric map uses diamond shaped tiles that are
     * aligned on an isometric projected grid. A Hexagonal map uses hexagon
     * shaped tiles that fit into each other by shifting every other row.
     *
     * Only Orthogonal, Isometric and Staggered maps are supported by this
     * version of Tiled.
     */
    enum Orientation {
        Unknown,
        Orthogonal,
        Isometric,
#ifdef ZOMBOID
        LevelIsometric,
#endif
        Staggered
    };

    /**
     * Constructor, taking map orientation, size and tile size as parameters.
     */
    Map(Orientation orientation,
        int width, int height,
        int tileWidth, int tileHeight);

    Map(const Map &other) = delete;

    Map& operator=(const Map& other) = delete;

    /**
     * Destructor.
     */
    ~Map();

    /**
     * Returns the orientation of the map.
     */
    Orientation orientation() const { return mOrientation; }

    /**
     * Sets the orientation of the map.
     */
    void setOrientation(Orientation orientation)
    { mOrientation = orientation; }

    /**
     * Returns the width of this map.
     */
    int width() const { return mWidth; }

    /**
     * Sets the width of this map.
     */
    void setWidth(int width) { mWidth = width; }

    /**
     * Returns the height of this map.
     */
    int height() const { return mHeight; }

    /**
     * Sets the height of this map.
     */
    void setHeight(int height) { mHeight = height; }

    /**
     * Returns the size of this map. Provided for convenience.
     */
    QSize size() const { return QSize(mWidth, mHeight); }

    /**
     * Returns the tile width of this map.
     */
    int tileWidth() const { return mTileWidth; }

    /**
     * Returns the tile height used by this map.
     */
    int tileHeight() const { return mTileHeight; }

#ifdef ZOMBOID
    void setCellsPerLevel(const QPoint &cellsPerLevel) { mCellsPerLevel = cellsPerLevel; }
    QPoint cellsPerLevel() const { return mCellsPerLevel; }
#endif

    /**
     * Adjusts the draw margins to be at least as big as the given margins.
     * Called from tile layers when their tiles change.
     */
    void adjustDrawMargins(const QMargins &margins);

    /**
     * Returns the margins that have to be taken into account when figuring
     * out which part of the map to repaint after changing some tiles.
     *
     * @see TileLayer::drawMargins
     */
    QMargins drawMargins() const { return mDrawMargins; }

    /**
     * Returns the number of layers of this map.
     */
    int layerCount() const;

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
    Layer *layerAt(int z, int index) const;

    /**
     * Returns the list of layers of this map. This is useful when you want to
     * use foreach.
     */
    QList<Layer*> layers() const;

    QList<Layer*> layers(Layer::Type type) const;
    QList<ObjectGroup*> objectGroups() const;
    QList<TileLayer*> tileLayers() const;

    /**
     * Adds a layer to this map.
     */
    void addLayer(Layer *layer);

    void insertLayer(int index, Layer *layer);

    /**
     * Removes the layer at the given index from this map and returns it.
     * The caller becomes responsible for the lifetime of this layer.
     */
    Layer *takeLayerAt(int z, int index);

    void removeLayer(Layer *layer);

    /**
     * Adds a tileset to this map. The map does not take ownership over its
     * tilesets, this is merely for keeping track of which tilesets are used by
     * the map, and their saving order.
     *
     * @param tileset the tileset to add
     */
    void addTileset(Tileset *tileset);

    /**
     * Inserts \a tileset at \a index in the list of tilesets used by this map.
     */
    void insertTileset(int index, Tileset *tileset);

    /**
     * Returns the index of the given \a tileset, or -1 if it is not used in
     * this map.
     */
    int indexOfTileset(Tileset *tileset) const;

    /**
     * Removes the tileset at \a index from this map.
     *
     * \warning Does not make sure that this map no longer refers to tiles from
     *          the removed tileset!
     *
     * \sa addTileset
     */
    void removeTilesetAt(int index);

    /**
     * Replaces all tiles from \a oldTileset with tiles from \a newTileset.
     * Also replaces the old tileset with the new tileset in the list of
     * tilesets.
     */
    void replaceTileset(Tileset *oldTileset, Tileset *newTileset);

    /**
     * Returns the tilesets that the tiles on this map are using.
     */
    const QList<Tileset*> &tilesets() const { return mTilesets; }

    /**
     * Returns whether the given \a tileset is used by any tile layer of this
     * map.
     */
    bool isTilesetUsed(Tileset *tileset) const;

#ifdef ZOMBOID
    QSet<Tileset*> usedTilesets() const;

    void addTilesetUser(Tileset *tileset);
    void removeTilesetUser(Tileset *tileset);

    QList<Tileset*> missingTilesets() const;
    bool hasMissingTilesets() const;
    bool hasUsedMissingTilesets() const;
#endif

#ifdef ZOMBOID
    void addTileLayerGroup(ZTileLayerGroup *tileLayerGroup);
    const QList<ZTileLayerGroup*> tileLayerGroups() const { return mTileLayerGroups; }
    int tileLayerGroupCount() const { return mTileLayerGroups.size(); }
#endif

#ifdef ZOMBOID
    int levelCount() const;

    MapLevel *levelAt(int z) const;

    MapBmp &rbmp(int index) { return index ? mBmpVeg : mBmpMain; }
    MapBmp bmp(int index) const { return index ? mBmpVeg : mBmpMain; }

    MapBmp &rbmpMain() { return mBmpMain; }
    MapBmp &rbmpVeg() { return mBmpVeg; }

    MapBmp bmpMain() const { return mBmpMain; }
    MapBmp bmpVeg() const { return mBmpVeg; }

    MapNoBlend *noBlend(const QString &layerName);
    QList<MapNoBlend*> noBlends() const { return mNoBlend.values(); }

    BmpSettings *rbmpSettings() { return &mBmpSettings; }
    const BmpSettings *bmpSettings() const { return &mBmpSettings; }
#endif

    Map *clone() const;

    /**
     * Creates a new map that contains the given \a layer. The map size will be
     * determined by the size of the layer.
     *
     * The orientation defaults to Unknown and the tile width and height will
     * default to 0. In case this map needs to be rendered, these properties
     * will need to be properly set.
     */
    static Map *fromLayer(Layer *layer);

private:
    void adoptLayer(Layer *layer);

    Orientation mOrientation;
    int mWidth;
    int mHeight;
    int mTileWidth;
    int mTileHeight;
    QMargins mDrawMargins;
    QList<Tileset*> mTilesets;
#ifdef ZOMBOID
    QVector<MapLevel*> mLevels;
    QPoint mCellsPerLevel;
    QList<ZTileLayerGroup*> mTileLayerGroups;
    QMap<Tileset*,int> mUsedTilesets;
    MapBmp mBmpMain;
    MapBmp mBmpVeg;
    QMap<QString,MapNoBlend*> mNoBlend;
    BmpSettings mBmpSettings;
#endif
};

/**
 * Helper function that converts the map orientation to a string value. Useful
 * for map writers.
 *
 * @return The map orientation as a lowercase string.
 */
TILEDSHARED_EXPORT QString orientationToString(Map::Orientation);

/**
 * Helper function that converts a string to a map orientation enumerator.
 * Useful for map readers.
 *
 * @return The map orientation matching the given string, or Map::Unknown if
 *         the string is unrecognized.
 */
TILEDSHARED_EXPORT Map::Orientation orientationFromString(const QString &);

} // namespace Tiled

#endif // MAP_H
