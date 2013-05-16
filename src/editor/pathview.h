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

#ifndef BASEWORLDVIEW_H
#define BASEWORLDVIEW_H

#include "basegraphicsview.h"

class BasePathScene;
class IsoPathScene;
class OrthoPathScene;
class PathDocument;
class TilePathScene;

class PathView : public BaseGraphicsView
{
public:
    PathView(PathDocument *doc, QWidget *parent = 0);

    void scrollContentsBy(int dx, int dy);

    void setDocument(PathDocument *doc);
    PathDocument *document() const
    { return mDocument; }

    BasePathScene *scene() const
    { return (BasePathScene*)mScene; }

    void switchToIso();
    void switchToOrtho();
    void switchToTile();

private:
    PathDocument *mDocument;
    IsoPathScene *mIsoScene;
    OrthoPathScene *mOrthoScene;
    TilePathScene *mTileScene;
    qreal mOrthoIsoScale;
    qreal mTileScale;
};

#endif // BASEWORLDVIEW_H
