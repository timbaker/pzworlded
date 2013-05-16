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

#include "pathview.h"

#include "isopathscene.h"
#include "orthopathscene.h"
#include "tilepathscene.h"
#include "zoomable.h"

PathView::PathView(PathDocument *doc, QWidget *parent) :
    BaseGraphicsView(true, parent),
    mDocument(doc),
    mIsoScene(new IsoPathScene(doc, this)),
    mOrthoScene(new OrthoPathScene(doc, this)),
    mTileScene(new TilePathScene(doc, this))
{
    setScene(mOrthoScene);

    mOrthoIsoScale = mTileScale = zoomable()->scale();
}

void PathView::switchToIso()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos);

    if (scene()->isTile()) mTileScale = zoomable()->scale();

    setScene(mIsoScene);
    zoomable()->setScale(mOrthoIsoScale);

    centerOn(scene()->renderer()->toScene(worldPos));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));
}

void PathView::switchToOrtho()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos);

    if (scene()->isTile()) mTileScale = zoomable()->scale();

    setScene(mOrthoScene);
    zoomable()->setScale(mOrthoIsoScale);

    centerOn(scene()->renderer()->toScene(worldPos));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));
}

void PathView::switchToTile()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos);

    mOrthoIsoScale = zoomable()->scale();

    mTileScene->centerOn(worldPos);
    setScene(mTileScene);
    zoomable()->setScale(mTileScale);

    centerOn(scene()->renderer()->toScene(worldPos));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));
}
