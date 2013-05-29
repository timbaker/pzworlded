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
#include "toolmanager.h"
#include "zoomable.h"

static bool AllowOpenGL = true;

PathView::PathView(PathDocument *doc, QWidget *parent) :
    BaseGraphicsView(AllowOpenGL, parent),
    mDocument(doc),
    mIsoScene(new IsoPathScene(doc, this)),
    mOrthoScene(new OrthoPathScene(doc, this)),
    mTileScene(new TilePathScene(doc, this)),
    mRecenterScheduled(false)
{
    setScene(mOrthoScene);

    QVector<qreal> zf = zoomable()->zoomFactors();
    zf << 8.0 << 16.0 << 48.0 << 128.0;
    zoomable()->setZoomFactors(zf);

    mOrthoIsoScale = mTileScale = zoomable()->scale();
}

void PathView::switchToIso()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos, 0);

    if (scene()->isTile()) mTileScale = zoomable()->scale();

    setScene(mIsoScene);
    zoomable()->setScale(mOrthoIsoScale);

#if 0
    setTransform(QTransform());
#endif

    centerOn(scene()->renderer()->toScene(worldPos, 0));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));

    ToolManager::instance()->setScene(scene());
}

void PathView::switchToOrtho()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos, 0);

    if (scene()->isTile()) mTileScale = zoomable()->scale();

    setScene(mOrthoScene);
    zoomable()->setScale(mOrthoIsoScale);

#if 0
    scale(1, 0.5);
    rotate(45);
#endif

    centerOn(scene()->renderer()->toScene(worldPos, 0));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));

    ToolManager::instance()->setScene(scene());
}

void PathView::switchToTile()
{
    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos, 0);

    mOrthoIsoScale = zoomable()->scale();

    mTileScene->centerOn(worldPos);
    setScene(mTileScene);
    zoomable()->setScale(mTileScale);

#if 0
    setTransform(QTransform());
#endif

    centerOn(scene()->renderer()->toScene(worldPos, 0));
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));

    ToolManager::instance()->setScene(scene());
}

qreal PathView::scale() const
{
    qreal nonTileScaleFactor = scene()->isTile() ? 1 : 32;
    return zoomable()->scale() / nonTileScaleFactor;
}

void PathView::adjustScale(qreal scale)
{
    qreal nonTileScaleFactor = scene()->isTile() ? 1 : 32;
    scale /= nonTileScaleFactor;
    BaseGraphicsView::adjustScale(scale);
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform() && scene()->isTile());
}

void PathView::recenter()
{
    mRecenterScheduled = false;

    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->renderer()->toWorld(scenePos, 0);

    scene()->scrollContentsBy(worldPos);
}

void PathView::scrollContentsBy(int dx, int dy)
{
    BaseGraphicsView::scrollContentsBy(dx, dy);

    // scrollContentsBy() gets called twice, once for the horizontal scrollbar
    // and once for the vertical scrollbar.  When changing scale, the dx/dy
    // values are quite large.  So, don't update the chunkmap until both
    // calls have completed.
    if (!mRecenterScheduled) {
        QMetaObject::invokeMethod(this, "recenter", Qt::QueuedConnection);
        mRecenterScheduled = true;
    }
}
