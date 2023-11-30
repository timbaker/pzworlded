/*
 * squareproperties.cpp
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

#include "squareproperties.h"

using namespace Tiled;

SquarePropertiesGrid *SquarePropertiesGrid::clone() const
{
    return new SquarePropertiesGrid(*this);
}

SquarePropertiesGrid *SquarePropertiesGrid::clone(const QRect &r) const
{
    SquarePropertiesGrid *klone = new SquarePropertiesGrid(r.width(), r.height());
    const QRect r2 = r & bounds();
    for (int x = r2.left(); x <= r2.right(); x++) {
        for (int y = r2.top(); y <= r2.bottom(); y++) {
            klone->replace(x - r.x(), y - r.y(), at(x, y));
        }
    }
    return klone;
}

SquarePropertiesGrid *SquarePropertiesGrid::clone(const QRegion &rgn) const
{
    QRect r = rgn.boundingRect();
    SquarePropertiesGrid *klone = new SquarePropertiesGrid(r.width(), r.height());
    for (QRect r2 : rgn) {
        r2 &= bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                klone->replace(x - r.x(), y - r.y(), at(x, y));
            }
        }
    }
    return klone;
}

void SquarePropertiesGrid::copy(const SquarePropertiesGrid &other)
{
    QRect r = bounds() & other.bounds();
    for (int y = r.top(); y <= r.bottom(); y++) {
        for (int x = r.left(); x <= r.right(); x++) {
            replace(x - r.x(), y - r.y(), other.at(x, y));
        }
    }
}

void SquarePropertiesGrid::copy(const SquarePropertiesGrid &other, const QRegion &rgn)
{
    for (QRect r2 : rgn) {
        r2 &= bounds() & other.bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                replace(x, y, other.at(x, y));
            }
        }
    }
}

QRegion SquarePropertiesGrid::region() const
{
    QRegion rgn;
    for (int index : mCells.keys()) {
        int x = index % width();
        int y = index / width();
        rgn += QRect(x, y, 1, 1);
    }
    return rgn;
}
