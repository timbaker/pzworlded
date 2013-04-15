/*
 * tile.h
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
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

#ifndef TILE_H
#define TILE_H

#include "object.h"

#include <QPixmap>

namespace Tiled {

class Tileset;

class TILEDSHARED_EXPORT Tile : public Object
{
public:
#ifdef ZOMBOID
    Tile(const QImage &image, int id, Tileset *tileset):
#else
    Tile(const QPixmap &image, int id, Tileset *tileset):
#endif
        mId(id),
        mTileset(tileset),
        mImage(image)
    {}

    /**
     * Returns ID of this tile within its tileset.
     */
    int id() const { return mId; }

    /**
     * Returns the tileset that this tile is part of.
     */
    Tileset *tileset() const { return mTileset; }

#ifdef ZOMBOID
    /**
     * Returns the image of this tile.
     */
    const QImage &image() const { return mImage; }

    /**
     * Sets the image of this tile.
     */
    void setImage(const QImage &image) { mImage = image; }
#else
    /**
     * Returns the image of this tile.
     */
    const QPixmap &image() const { return mImage; }

    /**
     * Sets the image of this tile.
     */
    void setImage(const QPixmap &image) { mImage = image; }
#endif

    /**
     * Returns the width of this tile.
     */
    int width() const { return mImage.width(); }

    /**
     * Returns the height of this tile.
     */
    int height() const { return mImage.height(); }

    /**
     * Returns the size of this tile.
     */
    QSize size() const { return mImage.size(); }

private:
    int mId;
    Tileset *mTileset;
#ifdef ZOMBOID
    QImage mImage;
#else
    QPixmap mImage;
#endif
};

} // namespace Tiled

#ifdef ZOMBOID
#include <QMetaType>
Q_DECLARE_METATYPE(Tiled::Tile*)
#endif

#endif // TILE_H
