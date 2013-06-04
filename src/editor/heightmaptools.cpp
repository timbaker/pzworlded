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
#include "heightmapview.h"

#include <QGraphicsSceneMouseEvent>

BaseHeightMapTool::BaseHeightMapTool(const QString &name, const QIcon &icon,
                                     const QKeySequence &shortcut,
                                     QObject *parent) :
    AbstractTool(name, icon, shortcut, HeightMapToolType)
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
    BaseHeightMapTool(tr("HeightMap Height Adjuster"), QIcon(), QKeySequence()),
    mCursorItem(new QGraphicsEllipseItem)
{
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
    QPoint worldPos = mScene->toWorldInt(event->scenePos());
    if (mScene->hm()->bounds().contains(worldPos))
        mScene->hm()->setHeightAt(worldPos, mScene->hm()->heightAt(worldPos) + 10);
}

void HeightMapTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
}

