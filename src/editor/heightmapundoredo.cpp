/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "heightmapundoredo.h"

#include "heightmap.h"
#include "undoredo.h"
#include "worlddocument.h"

#include <QCoreApplication>

PaintHeightMap::PaintHeightMap(WorldDocument *worldDoc, const HeightMapRegion &source,
                               bool mergeable) :
    QUndoCommand(QCoreApplication::translate("UndoCommands", "Paint HeightMap")),
    mWorldDocument(worldDoc),
    mSource(source),
    mMergeable(mergeable)
{
    mErased.resize(mSource.mRegion);
    HeightMap hm(mWorldDocument->hmFile(), 2);
    foreach (QRect r, mErased.mRegion.rects()) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                hm.setCenter(x, y);
                mErased.setHeightAt(x, y, hm.heightAt(x, y));
            }
        }
    }
}

void PaintHeightMap::undo()
{
    mWorldDocument->undoRedo().paintHeightMap(mErased);
}

void PaintHeightMap::redo()
{
    mWorldDocument->undoRedo().paintHeightMap(mSource);
}

int PaintHeightMap::id() const
{
    return UndoCmd_PaintHeightMap;
}

bool PaintHeightMap::mergeWith(const QUndoCommand *other)
{
    const PaintHeightMap *o = static_cast<const PaintHeightMap*>(other);
    if (!(mWorldDocument == o->mWorldDocument &&
          o->mMergeable))
        return false;
    mSource.merge(&o->mSource, o->mSource.mRegion);
    mErased.merge(&o->mErased, o->mErased.mRegion - mErased.mRegion);
    return true;
}

