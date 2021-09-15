/*
 * tileset.cpp
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

#include "tileset.h"
#include "tile.h"

#include <QBitmap>

using namespace Tiled;

Tileset::~Tileset()
{
    qDeleteAll(mTiles);
}

Tile *Tileset::tileAt(int id) const
{
    return (id < mTiles.size()) ? mTiles.at(id) : 0;
}

static QMap<QString,QImage> TILESET_IMAGES;
static size_t TILESET_BYTES = 0L;
#include <QDebug>
#include "texture_atlas.h"
#include <QPainter>

static bool tryCreateAtlas(Tileset *tileset, const QImage &image, int size)
{
    // TODO: sort from largest to smallest or vice-versa before adding
    Atlas *atlas = nullptr;
    const int padding = 1;
    QMap<int,uint32_t> ids;
    if (atlas_create(&atlas, size, padding)) {
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (tile->image().isNull())
                continue;
            uint32_t id;
            if (atlas_gen_texture(atlas, &id)) {
                if (atlas_allocate_vtex_space(atlas, id, tile->image().width(), tile->image().height())) {
                    ids[i] = id;
                } else {
                    atlas_destroy(atlas);
                    return false;
                }
            }
        }
    } else {
        qDebug() << "NOPE";
        return false;
    }

    if (ids.empty() == false) {
        uint32_t extents[4];
        extents[0] = std::numeric_limits<uint16_t>::max();
        extents[1] = std::numeric_limits<uint16_t>::max();
        extents[2] = extents[3] = 0;
        for (uint32_t id : qAsConst(ids)) {
            uint16_t xywh[4];
            atlas_get_vtex_xywh_coords(atlas, id, 0, xywh);
            extents[0] = std::min(extents[0], (uint32_t) xywh[0] - padding);
            extents[1] = std::min(extents[1], (uint32_t) xywh[1] - padding);
            extents[2] = std::max(extents[2], uint32_t(xywh[0] + xywh[2] + padding));
            extents[3] = std::max(extents[3], uint32_t(xywh[1] + xywh[3] + padding));
        }
        int width = extents[2] - extents[0];
        int height = extents[3] - extents[1];
//        qDebug() << fileName << image.width() << image.height() << "->" << width << height;
        QImage image4(width, height, image.format());
        image4.fill(Qt::transparent);
        QPainter painter(&image4);
        for (auto it = ids.cbegin(); it != ids.cend(); it++) {
            uint16_t xywh[4];
            atlas_get_vtex_xywh_coords(atlas, it.value(), 0, xywh);

            Tile *tile = tileset->tileAt(it.key());

            Tile::UVST uvst1;
            uvst1.u = xywh[0] / float(width);
            uvst1.v = xywh[1] / float(height);
            uvst1.s = (xywh[0] + xywh[2]) / float(width);
            uvst1.t = (xywh[1] + xywh[3]) / float(height);
            tile->setAtlasUVST(uvst1);

            tile->setAtlasSize(tile->image().size());

            painter.drawImage(QRect(xywh[0], xywh[1], xywh[2], xywh[3]), tile->image());

            /////
            tile->setEmptyImage();
            /////
        }
        painter.end();
        tileset->setImage(image4);
    }

    if (atlas != nullptr) {
        atlas_destroy(atlas);
    }

    return true;
}

bool Tileset::loadFromImage(const QImage &image, const QString &fileName)
{
    Q_ASSERT(mTileWidth > 0 && mTileHeight > 0);

    if (image.isNull())
        return false;

#ifdef ZOMBOID
    int mTileWidth = this->mTileWidth;
    int mTileHeight = this->mTileHeight;
    if (!mImageSource2x.isEmpty()) {
        mTileWidth *= 2;
        mTileHeight *= 2;
    }
#endif

    const int stopWidth = image.width() - mTileWidth;
    const int stopHeight = image.height() - mTileHeight;

    int oldTilesetSize = mTiles.size();
    int tileNum = 0;
#ifdef ZOMBOID
    QImage image3 = image;
    // For some reason this changes the const 'image'
    replaceTransparentColor(image3, mTransparentColor);
//    setImage(image3); // This is used to create an OpenGL texture.
    TILESET_IMAGES[fileName] = QImage();// image3;
#if 0
    TILESET_BYTES = 0L;
    for (const QImage& image : TILESET_IMAGES) {
        TILESET_BYTES += image.bytesPerLine() * image.height();
    }
    qDebug() << TILESET_IMAGES.size() << "images" << (TILESET_BYTES / (1024 * 1024)) << "MB";
#endif
    QImage image2 = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
//    replaceTransparentColor(image2, mTransparentColor);
#endif
    for (int y = mMargin; y <= stopHeight; y += mTileHeight + mTileSpacing) {
        for (int x = mMargin; x <= stopWidth; x += mTileWidth + mTileSpacing) {
#ifdef ZOMBOID
            QImage tileImage = image2.copy(x, y, mTileWidth, mTileHeight);

            if (tileNum < oldTilesetSize) {
                mTiles.at(tileNum)->setImage(tileImage);
            } else {
                mTiles.append(new Tile(tileImage, tileNum, this));
            }
#else
            const QImage tileImage = image.copy(x, y, mTileWidth, mTileHeight);
            QPixmap tilePixmap = QPixmap::fromImage(tileImage);

            if (mTransparentColor.isValid()) {
                const QImage mask =
                        tileImage.createMaskFromColor(mTransparentColor.rgb());
                tilePixmap.setMask(QBitmap::fromImage(mask));
            }

            if (tileNum < oldTilesetSize) {
                mTiles.at(tileNum)->setImage(tilePixmap);
            } else {
                mTiles.append(new Tile(tilePixmap, tileNum, this));
            }
#endif
            ++tileNum;
        }
    }

    // Blank out any remaining tiles to avoid confusion
    while (tileNum < oldTilesetSize) {
#ifdef ZOMBOID
        mTiles.at(tileNum)->setEmptyImage(mTileWidth, mTileHeight);
#else
        QPixmap tilePixmap = QPixmap(mTileWidth, mTileHeight);
        tilePixmap.fill();
        mTiles.at(tileNum)->setImage(tilePixmap);
#endif
        ++tileNum;
    }

    if (tryCreateAtlas(this, image3, 1024) == false) {
        if (tryCreateAtlas(this, image3, 2048) == false) {
            tryCreateAtlas(this, image3, 4096);
        }
    }

    mImageWidth = image.width();
    mImageHeight = image.height();
    mColumnCount = columnCountForWidth(mImageWidth);
#ifdef ZOMBOID
    mLoaded = true;
#endif
    mImageSource = fileName;
    return true;
}

#ifdef ZOMBOID
bool Tileset::loadFromCache(Tileset *cached)
{
    Q_ASSERT(mTileWidth == cached->tileWidth() && mTileHeight == cached->tileHeight());
    Q_ASSERT(mTileSpacing == cached->tileSpacing());
    Q_ASSERT(mMargin == cached->margin());
    Q_ASSERT(mTransparentColor == cached->transparentColor());

    int mTileWidth = this->mTileWidth;
    int mTileHeight = this->mTileHeight;
    if (!cached->mImageSource2x.isEmpty()) {
        mTileWidth *= 2;
        mTileHeight *= 2;
    }

    int oldTilesetSize = mTiles.size();
    int tileNum = 0;

    for (; tileNum < cached->tileCount(); ++tileNum) {
        Tile *tile = cached->tileAt(tileNum);
        if (tileNum < oldTilesetSize) {
            mTiles.at(tileNum)->setImage(tile);
            mTiles.at(tileNum)->setAtlasUVST(tile->atlasUVST());
            mTiles.at(tileNum)->setAtlasSize(tile->atlasSize());
        } else {
            mTiles.append(new Tile(tile, tileNum, this));
        }
    }

    // Blank out any remaining tiles to avoid confusion
    while (tileNum < oldTilesetSize) {
        mTiles.at(tileNum)->setEmptyImage(mTileWidth, mTileHeight);
        ++tileNum;
    }

    mImageWidth = cached->imageWidth();
    mImageHeight = cached->imageHeight();
    mImageSource2x = cached->imageSource2x();
    mColumnCount = columnCountForWidth(mImageWidth);
    mImageSource = cached->imageSource();
    mImage = cached->image();
    mLoaded = true;
    return true;
}

bool Tileset::loadFromNothing(const QSize &imageSize, const QString &fileName)
{
    Q_ASSERT(mTileWidth > 0 && mTileHeight > 0);

    if (imageSize.isEmpty())
        return false;

    const int stopWidth = imageSize.width() - mTileWidth;
    const int stopHeight = imageSize.height() - mTileHeight;

    int oldTilesetSize = mTiles.size();
    int tileNum = 0;

    for (int y = mMargin; y <= stopHeight; y += mTileHeight + mTileSpacing) {
        for (int x = mMargin; x <= stopWidth; x += mTileWidth + mTileSpacing) {
            if (tileNum < oldTilesetSize) {
                mTiles.at(tileNum)->setEmptyImage(mTileWidth, mTileHeight);
            } else {
                mTiles.append(new Tile(mTileWidth, mTileHeight, tileNum, this));
            }
            ++tileNum;
        }
    }

    // Blank out any remaining tiles to avoid confusion
    while (tileNum < oldTilesetSize) {
        mTiles.at(tileNum)->setEmptyImage(mTileWidth, mTileHeight);
        ++tileNum;
    }

    mImageWidth = imageSize.width();
    mImageHeight = imageSize.height();
    mColumnCount = columnCountForWidth(mImageWidth);
    mImageSource = fileName;
    mImage = QImage();
//    mLoaded = true;
    return true;
}

#endif // ZOMBOID

Tileset *Tileset::findSimilarTileset(const QList<Tileset*> &tilesets) const
{
    foreach (Tileset *candidate, tilesets) {
        if (candidate != this
        #ifdef ZOMBOID
            && ((candidate->imageSource() == imageSource()) || (!imageSource2x().isEmpty() && (candidate->imageSource2x() == imageSource2x())))
        #else
            && candidate->imageSource() == imageSource()
        #endif
            && candidate->tileWidth() == tileWidth()
            && candidate->tileHeight() == tileHeight()
            && candidate->tileSpacing() == tileSpacing()
            && candidate->margin() == margin()) {
                return candidate;
        }
    }
    return 0;
}

int Tileset::columnCountForWidth(int width) const
{
    Q_ASSERT(mTileWidth > 0);
#ifdef ZOMBOID
    if (!mImageSource2x.isEmpty())
        return (width - mMargin + mTileSpacing) / (mTileWidth * 2 + mTileSpacing);
#endif
    return (width - mMargin + mTileSpacing) / (mTileWidth + mTileSpacing);
}

#ifdef ZOMBOID
Tileset *Tileset::clone() const
{
    Tileset *clone = new Tileset(*this);

    for (int i = 0; i < clone->mTiles.size(); i++) {
        clone->mTiles[i] = new Tile(mTiles[i], i, clone);
        clone->mTiles[i]->mergeProperties(mTiles[i]->properties());
    }

    return clone;
}

void Tileset::replaceTransparentColor(QImage &image, const QColor &transparentColor)
{
    if (transparentColor.isValid() == false) {
        return;
    }
    QRgb rgba = transparentColor.rgba();
    QRgb transparent = qRgba(0,0,0,0);
    for (int y = 0, y2 = image.height(); y < y2; y++) {
        for (int x = 0, x2 = image.width(); x < x2; x++) {
            if (image.pixel(x, y) == rgba) {
                image.setPixel(x, y, transparent);
            }
        }
    }
}

// // // // //

TilesetImageCache::~TilesetImageCache()
{
    qDeleteAll(mTilesets);
}

#include <QDebug>

Tileset *TilesetImageCache::addTileset(Tileset *ts)
{
    Tileset *cached = new Tileset(QLatin1String("cached"), ts->tileWidth(), ts->tileHeight(), ts->tileSpacing(), ts->margin());
    cached->mTransparentColor = ts->transparentColor();
    cached->mImageSource = ts->imageSource(); // FIXME: make canonical
    cached->mImageSource2x = ts->imageSource2x();
    cached->mImage = ts->image();
    cached->mTiles.reserve(ts->tileCount());
    cached->mImageWidth = ts->imageWidth();
    cached->mImageHeight = ts->imageHeight();
    cached->mColumnCount = ts->columnCount();

    for (int tileNum = 0; tileNum < ts->tileCount(); ++tileNum) {
        Tile *tile = ts->tileAt(tileNum);
        cached->mTiles.append(new Tile(tile, tileNum, cached));
    }

    mTilesets.append(cached);

//    qDebug() << "added tileset image " << ts->imageSource() << " to cache";

    return cached;
}

Tileset *TilesetImageCache::findMatch(Tileset *ts, const QString &imageSource, const QString &imageSource2x)
{
    foreach (Tileset *candidate, mTilesets) {
        if (((candidate->imageSource() == imageSource) || (!imageSource2x.isEmpty() && (candidate->imageSource2x() == imageSource2x)))
                && candidate->tileWidth() == ts->tileWidth()
                && candidate->tileHeight() == ts->tileHeight()
                && candidate->tileSpacing() == ts->tileSpacing()
                && candidate->margin() == ts->margin()
                && candidate->transparentColor() == ts->transparentColor()) {
//            qDebug() << "retrieved tileset image " << candidate->imageSource() << " from cache";
            return candidate;
        }
    }
    return NULL;
}

#endif
