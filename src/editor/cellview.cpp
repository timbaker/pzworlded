/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "cellview.h"

#include "cellscene.h"
#include "preferences.h"
#include "zoomable.h"

#include "maprenderer.h"

#include <QGLWidget>
#include <QMouseEvent>

CellView::CellView(QWidget *parent) :
    BaseGraphicsView(parent)
{
#ifndef QT_NO_OPENGL
    Preferences *prefs = Preferences::instance();
    setUseOpenGL(prefs->useOpenGL());
    connect(prefs, SIGNAL(useOpenGLChanged(bool)), SLOT(setUseOpenGL(bool)));
#endif

    zoomable()->setScale(0.25);
}

CellScene *CellView::scene() const
{
    return static_cast<CellScene*>(mScene);
}

void CellView::mouseMoveEvent(QMouseEvent *event)
{
    QPoint tilePos = scene()->renderer()->pixelToTileCoordsInt(mapToScene(event->pos()));
    emit statusBarCoordinatesChanged(tilePos.x(), tilePos.y());

    BaseGraphicsView::mouseMoveEvent(event);
}

// I put this in BaseGraphicsView so WorldScene could use it, but the OpenGL
// backend chokes on overly-large BMP images.  It uses QCache in the
// QGLTextureCache::insert() method which *deletes the texture* and other
// code doesn't test for that.
void CellView::setUseOpenGL(bool useOpenGL)
{
#ifndef QT_NO_OPENGL
    QWidget *oldViewport = viewport();
    QWidget *newViewport = viewport();
    if (useOpenGL && QGLFormat::hasOpenGL()) {
        if (!qobject_cast<QGLWidget*>(viewport())) {
            QGLFormat format = QGLFormat::defaultFormat();
            format.setDepth(false); // No need for a depth buffer
            format.setSampleBuffers(true); // Enable anti-aliasing
            newViewport = new QGLWidget(format);
        }
    } else {
        if (qobject_cast<QGLWidget*>(viewport()))
            newViewport = 0;
    }

    if (newViewport != oldViewport) {
        if (mMiniMap) {
            mMiniMap->setVisible(false);
            mMiniMap->setParent(static_cast<QWidget*>(parent()));
        }
        setViewport(newViewport);
        if (mMiniMap) {
            mMiniMap->setParent(this);
            mMiniMap->setVisible(Preferences::instance()->showMiniMap());
            if (scene())
                mMiniMap->sceneRectChanged(scene()->sceneRect());
        }
    }

    QWidget *v = viewport();
    v->setAttribute(Qt::WA_StaticContents);
    v->setMouseTracking(true);
#endif
}
