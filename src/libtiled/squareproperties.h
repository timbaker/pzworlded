/*
 * squareproperties.h
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

#ifndef SQUAREPROPERTIES_H
#define SQUAREPROPERTIES_H

#include "properties.h"

#include <QHash>
#include <QRegion>

namespace Tiled {

class TILEDSHARED_EXPORT SquarePropertiesGrid
{
public:
    SquarePropertiesGrid(int width, int height) :
        mWidth(width),
        mHeight(height)
    {
    }

    int width() const
    {
        return mWidth;
    }

    int height() const
    {
        return mHeight;
    }

    int size() const
    {
        return mWidth * mHeight;
    }

    QRect bounds() const
    {
        return QRect(0, 0, mWidth, mHeight);
    }

    bool isValidPosition(int x, int y) const
    {
        return (x >= 0) && (x < mWidth) && (y >= 0) && (y < mHeight);
    }

    bool hasPropertiesAt(int x, int y) const
    {
        return isValidPosition(x, y) && mCells.contains(x + y * mWidth);
    }

    const Properties &at(int index) const
    {
        QHash<int,Properties>::const_iterator it = mCells.find(index);
        if (it != mCells.end()) {
            return *it;
        }
        return mEmptyCell;
    }

    const Properties &at(int x, int y) const
    {
        return at(y * mWidth + x);
    }

    void replace(int index, const Properties &cell)
    {
        QHash<int,Properties>::iterator it = mCells.find(index);
        if (it == mCells.end()) {
            if (cell.isEmpty())
                return;
            mCells.insert(index, cell);
        } else if (!cell.isEmpty()) {
            (*it) = cell;
        } else {
            mCells.erase(it);
        }
    }

    void replace(int x, int y, const Properties &cell)
    {
        int index = y * mWidth + x;
        replace(index, cell);
    }

    bool isEmpty() const
    {
        return mCells.isEmpty();
    }

    void clear()
    {
        mCells.clear();
    }

    SquarePropertiesGrid *clone() const;
    SquarePropertiesGrid *clone(const QRect &r) const;
    SquarePropertiesGrid *clone(const QRegion &rgn) const;

    void copy(const SquarePropertiesGrid& other);
    void copy(const SquarePropertiesGrid& other, const QRegion &rgn);

    QRegion region() const;

private:
    int mWidth, mHeight;
    QHash<int,Properties> mCells;
    Properties mEmptyCell;
};

} // namespace Tiled

#endif // SQUAREPROPERTIES_H
