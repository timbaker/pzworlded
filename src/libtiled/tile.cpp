/*
 * tile.cpp
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

#include "tile.h"

#include "tileset.h"

#include <QMargins>
#include <QPainter>

using namespace Tiled;

void Tile::setImage(const QImage &image)
{
    mImage = QImage();
    mImageOffset = QPoint(0, 0);
    mImageSize = image.size();

    int top = 0;
    while (top < image.height() && isRowTransparent(image, top))
        top++;
    if (top == image.height()) {
        return;
    }
    mImageOffset.setY(top);

    int bottom = image.height() - 1;
    while (bottom > top && isRowTransparent(image, bottom))
        bottom--;

    int left = 0;
    while (left < image.width() && isColumnTransparent(image, left))
        left++;
    mImageOffset.setX(left);

    int right = image.width() - 1;
    while (right > left && isColumnTransparent(image, right))
        right--;

    mImage = image.copy(left, top, right - left + 1, bottom - top + 1);
}

void Tile::setEmptyImage(int width, int height)
{
    mImage = QImage();
    mImageOffset = QPoint(0, 0);
    mImageSize = QSize(width, height);
}

QMargins Tile::drawMargins(float scale)
{
    float tileScale = tileset()->imageSource2x().isEmpty() ? 1.0f : 0.5f;
    return QMargins(mImageOffset.x(), mImageOffset.y(),
                    mImageSize.width() - (mImageOffset.x() + mImage.width()),
                    mImageSize.height() - (mImageOffset.y() + mImage.height())) * scale * tileScale;
}

QImage Tile::finalImage(int width, int height)
{
    QMargins margins = drawMargins(width / 64.0f);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QRect r(margins.left(), margins.top(), width - margins.left() - margins.right(), height - margins.top() - margins.bottom());
    painter.drawImage(r, this->image());
    painter.end();
    return image;
}

void Tile::setImage(const Tile *tile)
{
    mImage = tile->mImage;
    mImageOffset = tile->mImageOffset;
    mImageSize = tile->mImageSize;
}

bool Tile::isRowTransparent(const QImage &image, int row)
{
    for (int x = 0; x < image.width(); x++) {
        QRgb rgb = image.pixel(x, row);
        if (qAlpha(rgb) > 0)
            return false;
    }
    return true;
}

bool Tile::isColumnTransparent(const QImage &image, int col)
{
    for (int y = 0; y < image.height(); y++) {
        QRgb rgb = image.pixel(col, y);
        if (qAlpha(rgb) > 0)
            return false;
    }
    return true;
}
