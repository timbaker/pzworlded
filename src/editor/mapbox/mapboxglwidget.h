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

#ifndef MAPBOXGLWIDGET_H
#define MAPBOXGLWIDGET_H

#include <QMapboxGL>
#include <QOpenGLWidget>

class MapboxGLWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    MapboxGLWidget(QWidget* parent = nullptr);
    ~MapboxGLWidget();

    // QWidget implementation.
    void keyPressEvent(QKeyEvent *ev) final;
    void mousePressEvent(QMouseEvent *ev) final;
    void mouseMoveEvent(QMouseEvent *ev) final;
    void wheelEvent(QWheelEvent *ev) final;

    // QOpenGLWidget implementation.
    void initializeGL() final;
    void paintGL() final;

    qreal pixelRatio() {
        return devicePixelRatioF();
    }

    void setJson(const QMap<QString, QByteArray> &json);

signals:
    void mapChanged(QMapboxGL::MapChange change);

public:
    QPointF m_lastPos;

    QMapboxGLSettings m_settings;
    QScopedPointer<QMapboxGL> m_map;

};

#endif // MAPBOXGLWIDGET_H
