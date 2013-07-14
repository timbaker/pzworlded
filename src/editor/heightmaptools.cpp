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

#include "heightmaptools.h"

#include "heightmap.h"
#include "heightmapdocument.h"
#include "heightmapview.h"
#include "heightmapundoredo.h"
#include "worlddocument.h"

#include <QDebug>
#include <QGraphicsSceneMouseEvent>

BaseHeightMapTool::BaseHeightMapTool(const QString &name, const QIcon &icon,
                                     const QKeySequence &shortcut,
                                     QObject *parent) :
    AbstractTool(name, icon, shortcut, HeightMapToolType, parent)
{
}

void BaseHeightMapTool::setScene(BaseGraphicsScene *scene)
{
    mScene = scene ? scene->asHeightMapScene() : 0;
}

BaseGraphicsScene *BaseHeightMapTool::scene() const
{
    return mScene;
}

void BaseHeightMapTool::updateEnabledState()
{
    setEnabled(mScene != 0);
}

/////

HeightMapTool *HeightMapTool::mInstance = 0;

HeightMapTool *HeightMapTool::instance()
{
    if (!mInstance)
        mInstance = new HeightMapTool;
    return mInstance;
}

HeightMapTool::HeightMapTool() :
    BaseHeightMapTool(tr("HeightMap Height Adjuster"),
                      QIcon(QLatin1String(":images/22x22/heightmap-paint")),
                      QKeySequence()),
    mCursorItem(new QGraphicsPolygonItem)
{
    mCursorItem->setPen(QPen(Qt::black, 4));

    mStrength = 1.0;
    mOuterRadius = 4;
    mInnerRadius = 1;
    mRampPower = 1; // unused
}

void HeightMapTool::activate()
{
    mScene->addItem(mCursorItem);
}

void HeightMapTool::deactivate()
{
    mScene->removeItem(mCursorItem);
}

void HeightMapTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        paint(event->scenePos(), true, false);
    }
    if (event->buttons() & Qt::RightButton) {
        paint(event->scenePos(), false, false);
    }
}

void HeightMapTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint worldPos = mScene->toWorldInt(event->scenePos());
    if (worldPos == mLastWorldPos)
        return;
    mLastWorldPos = worldPos;

#if 1
    QRectF r(worldPos - QPointF(mOuterRadius, mOuterRadius),
             QSize(mOuterRadius * 2, mOuterRadius * 2));
    mCursorItem->setPolygon(mScene->toSceneF(r));
#elif 0
    QTransform t;
    t.translate(mScene->toScene(worldPos).x(), mScene->toScene(worldPos).y());
    t.scale(1,0.5);
    t.rotate(-45);
    mCursorItem->setTransform(t);
    mCursorItem->setRect(QRectF(-QPointF(mOuterRadius, mOuterRadius) * 64,
                                QSize(mOuterRadius * 2, mOuterRadius * 2) * 64));
#else
    QRectF r(worldPos - QPointF(mOuterRadius, mOuterRadius),
             QSize(mOuterRadius * 2, mOuterRadius * 2));
    mCursorItem->setRect(mScene->boundingRect(r.toAlignedRect()));
#endif

    if (event->buttons() & Qt::LeftButton) {
        paint(event->scenePos(), true, true);
    }
    if (event->buttons() & Qt::RightButton) {
        paint(event->scenePos(), false, true);
    }
}

void HeightMapTool::paint(const QPointF &scenePos, bool heightUp, bool mergeable)
{
    QPoint worldPos = mScene->toWorldInt(scenePos);
    int d = heightUp ? +5 : -5;

    QRect updateRect(worldPos.x() - mOuterRadius - 2,
                     worldPos.y() - mOuterRadius - 2,
                     mOuterRadius * 2 + 4, mOuterRadius * 2 + 4);

    qreal scale = 1.0 / ((mInnerRadius * mInnerRadius) - (mOuterRadius*mOuterRadius));
    qreal bias = -(mOuterRadius*mOuterRadius) * scale;

    HeightMapRegion hmr;
    for (int x = updateRect.left(); x <= updateRect.right(); ++x) {
        for (int y = updateRect.top(); y <= updateRect.bottom(); y++) {
            QPoint p(x, y);
            if (mScene->worldBounds().contains(p)) {
                qreal dx = x - worldPos.x();
                qreal dy = y - worldPos.y();
                qreal s = dx * dx + dy * dy;
                qreal delta = mStrength * qBound(s * scale + bias, 0.0, 1.0);
                hmr.resize(hmr.mRegion | QRegion(p.x(), p.y(), 1, 1));
                hmr.setHeightAt(p.x(), p.y(), mScene->hm()->heightAt(p) + delta * d);
            }
        }
    }
    mScene->document()->worldDocument()->paintHeightMap(hmr, mergeable);
}

