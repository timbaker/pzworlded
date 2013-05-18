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

#include "global.h"

#include <QGraphicsItem>

class BasePathRenderer;
class BasePathScene;
class BasePathTool;
class PathDocument;
class PathWorld;
class WorldNode;
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

class NodesItem : public QGraphicsItem
{
public:
    NodesItem(BasePathScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *);

    const NodeSet &selectedNodes()
    { return mSelectedNodes; }
    void setSelectedNodes(const NodeSet &selection);

    void setDragging(WorldNode *node, bool dragging);
    void setDragOffset(WorldNode *node, const QPointF &offset);
    QPointF dragOffset(WorldNode *node);

    QList<WorldNode*> lookupNodes(const QRectF &sceneRect);
    WorldNode *topmostNodeAt(const QPointF &scenePos);

private:
    qreal nodeRadius() const;

private:
    BasePathScene *mScene;
    QMap<WorldNode*,QPointF> mNodeOffset;
    NodeSet mSelectedNodes;
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

    void setTool(AbstractTool *tool);

    void keyPressEvent(QKeyEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    NodesItem *nodeItem() const
    { return mNodeItem; }

    WorldPathLayer *currentPathLayer() const
    { return mCurrentPathLayer; }

private:
    PathDocument *mDocument;
    BasePathRenderer *mRenderer;
    QList<PathLayerItem*> mPathLayerItems;
    NodesItem *mNodeItem;
    WorldPathLayer *mCurrentPathLayer;
    BasePathTool *mActiveTool;
};

#endif // BASEWORLDSCENE_H
