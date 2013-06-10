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

#ifndef HEIGHTMAPTOOLS_H
#define HEIGHTMAPTOOLS_H

#include "scenetools.h"

class HeightMapScene;

class BaseHeightMapTool : public AbstractTool
{
public:
    BaseHeightMapTool(const QString &name,
                      const QIcon &icon,
                      const QKeySequence &shortcut,
                      QObject *parent = 0);

    void setScene(BaseGraphicsScene *scene);
    BaseGraphicsScene *scene() const;

    virtual void keyPressEvent(QKeyEvent *event) { Q_UNUSED(event) }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }

protected:
    void updateEnabledState();

protected:
    HeightMapScene *mScene;
};

class HeightMapTool : public BaseHeightMapTool
{
    Q_OBJECT
public:
    static HeightMapTool *instance();
    static void deleteInstance();

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    virtual void languageChanged() {}

    qreal outerRadius() const { return mOuterRadius; }
    qreal innerRadius() const { return mInnerRadius; }

private:
    void paint(const QPointF &scenePos, bool heightUp, bool mergeable);

private:
    Q_DISABLE_COPY(HeightMapTool)
    static HeightMapTool *mInstance;
    HeightMapTool();

    qreal mStrength;
    qreal mOuterRadius;
    qreal mInnerRadius;
    qreal mRampPower;

    QGraphicsPolygonItem *mCursorItem;
    QPoint mLastWorldPos;
};

#endif // HEIGHTMAPTOOLS_H
