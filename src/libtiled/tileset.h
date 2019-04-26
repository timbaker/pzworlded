/*
 * tileset.h
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyrigth 2009, Edward Hutchins <eah1@yahoo.com>
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

#ifndef TILESET_H
#define TILESET_H

#include "object.h"

#include <QColor>
#include <QList>
#include <QPoint>
#ifdef ZOMBOID
#include "asset.h"
#include <QSize>
#endif
#include <QString>

class QImage;

namespace Tiled {

class Tile;

#ifdef ZOMBOID
class Tileset;
class TILEDSHARED_EXPORT TilesetImageCache
{
public:
    ~TilesetImageCache();
    Tileset *addTileset(Tileset *ts);
    Tileset *findMatch(Tileset *ts, const QString &imageSource, const QString &imageSource2x);
    QList<Tileset*> mTilesets;
};

#endif

/**
 * A tileset, representing a set of tiles.
 *
 * This class currently only supports loading tiles from a tileset image, using
 * loadFromImage(). There is no way to add or remove arbitrary tiles.
 */
#ifdef ZOMBOID
class TILEDSHARED_EXPORT Tileset : public Asset, public Object
#else
class TILEDSHARED_EXPORT Tileset : public Object
#endif
{
public:
#ifdef ZOMBOID
    Tileset(AssetPath path, AssetManager* manager, AssetParams* params)
        : Asset(path, manager)
    {
        Q_UNUSED(params);
    }
#endif
    /**
     * Constructor.
     *
     * @param name        the name of the tileset
     * @param tileWidth   the width of the tiles in the tileset
     * @param tileHeight  the height of the tiles in the tileset
     * @param tileSpacing the spacing between the tiles in the tileset image
     * @param margin      the margin around the tiles in the tileset image
     */
    Tileset(const QString &name, int tileWidth, int tileHeight,
            int tileSpacing = 0, int margin = 0):
        Asset(name, nullptr),
        mName(name),
        mTileWidth(tileWidth),
        mTileHeight(tileHeight),
        mTileSpacing(tileSpacing),
        mMargin(margin),
        mImageWidth(0),
        mImageHeight(0),
        mColumnCount(0)
  #ifdef ZOMBOID
        , mMissing(false),
        mLoaded(false)
  #endif
    {
        Q_ASSERT(tileSpacing >= 0);
        Q_ASSERT(margin >= 0);
    }

    /**
     * Destructor.
     */
    ~Tileset();

    /**
     * Returns the name of this tileset.
     */
    const QString &name() const { return mName; }

    /**
     * Sets the name of this tileset.
     */
    void setName(const QString &name) { mName = name; }

    /**
     * Returns the file name of this tileset. When the tileset isn't an
     * external tileset, the file name is empty.
     */
    const QString &fileName() const { return mFileName; }

    /**
     * Sets the filename of this tileset.
     */
    void setFileName(const QString &fileName) { mFileName = fileName; }

    /**
     * Returns whether this tileset is external.
     */
    bool isExternal() const { return !mFileName.isEmpty(); }

    /**
     * Returns the width of the tiles in this tileset.
     */
    int tileWidth() const { return mTileWidth; }

    /**
     * Returns the height of the tiles in this tileset.
     */
    int tileHeight() const { return mTileHeight; }

    /**
     * Returns the spacing between the tiles in the tileset image.
     */
    int tileSpacing() const { return mTileSpacing; }

    /**
     * Returns the margin around the tiles in the tileset image.
     */
    int margin() const { return mMargin; }

    /**
     * Returns the offset that is applied when drawing the tiles in this
     * tileset.
     */
    QPoint tileOffset() const { return mTileOffset; }

    /**
     * @see tileOffset
     */
    void setTileOffset(QPoint offset) { mTileOffset = offset; }

    /**
     * Returns the tile for the given tile ID.
     * The tile ID is local to this tileset, which means the IDs are in range
     * [0, tileCount() - 1].
     */
    Tile *tileAt(int id) const;

    /**
     * Returns the number of tiles in this tileset.
     */
    int tileCount() const { return mTiles.size(); }

    /**
     * Returns the number of tile columns in the tileset image.
     */
    int columnCount() const { return mColumnCount; }

    /**
     * Returns the width of the tileset image.
     */
    int imageWidth() const { return mImageWidth; }

    /**
     * Returns the height of the tileset image.
     */
    int imageHeight() const { return mImageHeight; }

    /**
     * Returns the transparent color, or an invalid color if no transparent
     * color is used.
     */
    QColor transparentColor() const { return mTransparentColor; }

    /**
     * Sets the transparent color. Pixels with this color will be masked out
     * when loadFromImage() is called.
     */
    void setTransparentColor(const QColor &c) { mTransparentColor = c; }

    /**
     * Load this tileset from the given tileset \a image. This will replace
     * existing tile images in this tileset with new ones. If the new image
     * contains more tiles than exist in the tileset new tiles will be
     * appended, if there are fewer tiles the excess images will be blanked.
     *
     * The tile width and height of this tileset must be higher than 0.
     *
     * @param image    the image to load the tiles from
     * @param fileName the file name of the image, which will be remembered
     *                 as the image source of this tileset.
     * @return <code>true</code> if loading was successful, otherwise
     *         returns <code>false</code>
     */
    bool loadFromImage(const QImage &image, const QString &fileName);

#ifdef ZOMBOID
    bool loadFromCache(Tileset *cached);
    friend class TilesetImageCache;
#endif

#ifdef ZOMBOID
    bool loadFromNothing(const QSize &imageSize, const QString &fileName);
#endif

    /**
     * This checks if there is a similar tileset in the given list.
     * It is needed for replacing this tileset by its similar copy.
     */
    Tileset *findSimilarTileset(const QList<Tileset*> &tilesets) const;

    /**
     * Returns the file name of the external image that contains the tiles in
     * this tileset. Is an empty string when this tileset doesn't have a
     * tileset image.
     */
    const QString &imageSource() const { return mImageSource; }

    /**
     * Returns the column count that this tileset would have if the tileset
     * image would have the given \a width. This takes into account the tile
     * size, margin and spacing.
     */
    int columnCountForWidth(int width) const;

#ifdef ZOMBOID
//    Tileset *clone() const;

    void setMissing(bool missing)
    { mMissing = missing; }

    /**
      * Returns true if the source image wasn't successfully loaded.
      * Instead of failing to load a map, we just display a special
      * "missing" tile.
      */
    bool isMissing() const
    { return mMissing; }

    void setImageSource(const QString &source)
    { mImageSource = source; }

    void setLoaded(bool loaded)
    { mLoaded = loaded; }

    bool isLoaded() const
    { return mLoaded; }

    void setImageSource2x(const QString &source)
    { mImageSource2x = source; }

    const QString &imageSource2x() const { return mImageSource2x; }
#endif

private:
    QString mName;
    QString mFileName;
    QString mImageSource;
    QColor mTransparentColor;
    int mTileWidth;
    int mTileHeight;
    int mTileSpacing;
    int mMargin;
    QPoint mTileOffset;
    int mImageWidth;
    int mImageHeight;
    int mColumnCount;
    QList<Tile*> mTiles;
#ifdef ZOMBOID
    bool mMissing;
    bool mLoaded;
    QString mImageSource2x;
#endif
};

} // namespace Tiled

#endif // TILESET_H
