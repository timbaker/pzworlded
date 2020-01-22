/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "mapboxglwidget.h"

#include "mapboxcoordinate.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMenu>

MapboxGLWidget::MapboxGLWidget(QWidget *parent)
    : QOpenGLWidget (parent)
{

}

MapboxGLWidget::~MapboxGLWidget()
{
    // Make sure we have a valid context so we
    // can delete the QMapboxGL.
    makeCurrent();
}

void MapboxGLWidget::keyPressEvent(QKeyEvent *ev)
{
    switch (ev->key()) {
    case Qt::Key_Tab:
        break;
    default:
        break;
    }

    ev->accept();
}


void MapboxGLWidget::mousePressEvent(QMouseEvent *ev)
{
    m_lastPos = ev->localPos();
#if 0
    if (ev->type() == QEvent::MouseButtonPress) {
        if (ev->buttons() == Qt::RightButton) {
            QMenu menu;
            QAction *gotoAction = menu.addAction(tr("Go Here"));
            QAction *action = menu.exec(ev->screenPos().toPoint());
            if (action == gotoAction) {
                QMapbox::Coordinate mc = m_map->coordinateForPixel(ev->pos());
                GameCoordinate gc = GameCoordinate::fromLatLng(mc);
                emit gotoLocation(gc.x, gc.y);
            }

        }
    }

    if (ev->type() == QEvent::MouseButtonDblClick) {
        if (ev->buttons() == Qt::LeftButton) {
            m_map->scaleBy(2.0, m_lastPos);
        } else if (ev->buttons() == Qt::RightButton) {
            m_map->scaleBy(0.5, m_lastPos);
        }
    }
#endif
    ev->accept();
}

void MapboxGLWidget::mouseMoveEvent(QMouseEvent *ev)
{
    QPointF delta = ev->localPos() - m_lastPos;
#if 0
    if (!delta.isNull()) {
        if (ev->buttons() == Qt::LeftButton && ev->modifiers() & Qt::ShiftModifier) {
            m_map->setPitch(m_map->pitch() - delta.y());
        } else if (ev->buttons() == Qt::LeftButton) {
            m_map->moveBy(delta);
        } else if (ev->buttons() == Qt::RightButton) {
            m_map->rotateBy(m_lastPos, ev->localPos());
        }
    }
#endif
    m_lastPos = ev->localPos();
    ev->accept();
}

void MapboxGLWidget::wheelEvent(QWheelEvent *ev)
{
    if (ev->orientation() == Qt::Horizontal) {
        return;
    }

    float factor = ev->delta() / 1200.;
    if (ev->delta() < 0) {
        factor = factor > -1 ? factor : 1 / factor;
    }
#if 0
    m_map->scaleBy(1 + factor, ev->pos());
#endif
    ev->accept();
}

void MapboxGLWidget::initializeGL()
{
#if 0
    m_settings.setAssetPath(QStringLiteral("D:/pz/worktree/build40-weather/workdir/media/mapbox"));

    m_map.reset(new QMapboxGL(nullptr, m_settings, size(), pixelRatio()));
    connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));
    // Forward this signal
    connect(m_map.data(), &QMapboxGL::mapChanged, this, &MapboxGLWidget::mapChanged);

    m_map->setCoordinateZoom(QMapbox::Coordinate(-0.06042606042606043, 0.09841515314633154), 15);

#if 1
    try {
        m_map->setStyleJson(lootableStyleJson);
    } catch (const std::exception& ex) {
        qDebug() << ex.what();
    }
#else
    QString styleUrl(QStringLiteral("file://D:/pz/worktree/build40-weather/pzmapbox/data/pz-style.json"));
    m_map->setStyleUrl(styleUrl);
    setWindowTitle(QStringLiteral("Mapbox GL: ") + styleUrl);
#endif
#endif
}

void MapboxGLWidget::paintGL()
{
#if 0
    m_map->resize(size());
    m_map->setFramebufferObject(defaultFramebufferObject(), size() * pixelRatio());
    m_map->render();
#endif
}
