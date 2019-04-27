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
    return (id < mTiles.size()) ? mTiles.at(id) : nullptr;
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
    QImage image2 = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
#endif
    for (int y = mMargin; y <= stopHeight; y += mTileHeight + mTileSpacing) {
        for (int x = mMargin; x <= stopWidth; x += mTileWidth + mTileSpacing) {
#ifdef ZOMBOID
            QImage tileImage = image2.copy(x, y, mTileWidth, mTileHeight);

            if (mTransparentColor.isValid()) {
                for (int x = 0; x < mTileWidth; x++) {
                    for (int y = 0; y < mTileHeight; y++) {
                        if (tileImage.pixel(x, y) == mTransparentColor.rgba())
                            tileImage.setPixel(x, y, qRgba(0,0,0,0));
                    }
                }
            }

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
    return nullptr;
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
#if 0
Tileset *Tileset::clone() const
{
    Tileset *clone = new Tileset(*this);

    for (int i = 0; i < clone->mTiles.size(); i++) {
        clone->mTiles[i] = new Tile(mTiles[i], i, clone);
        clone->mTiles[i]->mergeProperties(mTiles[i]->properties());
    }

    return clone;
}
#endif

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
    return nullptr;
}

#endif
