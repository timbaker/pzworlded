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

#ifndef BASEWORLDSCENE_H
#define BASEWORLDSCENE_H

#include "basegraphicsscene.h"

#include <QGraphicsItem>

class BasePathRenderer;
class BasePathScene;
class PathDocument;
class PathWorld;
class WorldPathLayer;

class PathLayerItem : public QGraphicsItem
{
public:
    PathLayerItem(WorldPathLayer *layer, BasePathScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *);

private:
    WorldPathLayer *mLayer;
    BasePathScene *mScene;
};

class BasePathScene : public BaseGraphicsScene
{
public:
    BasePathScene(PathDocument *doc, QObject *parent = 0);
    ~BasePathScene();

    void setDocument(PathDocument *doc);
    PathDocument *document() const
    { return mDocument; }

    PathWorld *world() const;

    void setRenderer(BasePathRenderer *renderer);
    BasePathRenderer *renderer() const
    { return mRenderer; }

    virtual bool isIso() const { return false; }
    virtual bool isOrtho() const { return false; }
    virtual bool isTile() const { return false; }

    virtual void scrollContentsBy(const QPointF &worldPos)
    { Q_UNUSED(worldPos) }

private:
    PathDocument *mDocument;
    BasePathRenderer *mRenderer;
    QList<PathLayerItem*> mPathLayerItems;
};

#endif // BASEWORLDSCENE_H
