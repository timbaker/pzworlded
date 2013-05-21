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
class WorldChanger;
class WorldLookup;
class WorldNode;
class WorldPath;
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

    void trackMouse(QGraphicsSceneMouseEvent *event);

    const NodeSet &selectedNodes()
    { return mSelectedNodes; }
    void setSelectedNodes(const NodeSet &selection);

    QList<WorldNode*> lookupNodes(const QRectF &sceneRect);
    WorldNode *topmostNodeAt(const QPointF &scenePos);

    void nodeRemoved(WorldNode *node)
    { mSelectedNodes.remove(node); }

    void redrawNode(id_t id);

private:
    qreal nodeRadius() const;

private:
    BasePathScene *mScene;
    NodeSet mSelectedNodes;
    QMap<WorldNode*,QPointF> mNodeOffset;
    id_t mHoverNode;
};

class BasePathScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    BasePathScene(PathDocument *doc, QObject *parent = 0);
    ~BasePathScene();

    void setDocument(PathDocument *doc);
    PathDocument *document() const
    { return mDocument; }

    PathWorld *world() const;
    WorldLookup *lookup() const;

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

    void setSelectedPaths(const PathSet &selection);
    const PathSet &selectedPaths() const
    { return mSelectedPaths; }
    QList<WorldPath *> lookupPaths(const QRectF &sceneRect);
    WorldPath *topmostPathAt(const QPointF &scenePos);

    WorldChanger *changer() const
    { return mChanger; }

    WorldPath *mHighlightPath;

public slots:
    void afterAddNode(WorldNode *node);
    void afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node);
    void afterMoveNode(WorldNode *node, const QPointF &prev);

    void afterAddPath(WorldPath *path);
    void afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);

    void nodeMoved(WorldNode *node);

private:
    PathDocument *mDocument;
    BasePathRenderer *mRenderer;
    QList<PathLayerItem*> mPathLayerItems;
    PathSet mSelectedPaths;
    NodesItem *mNodeItem;
    WorldPathLayer *mCurrentPathLayer;
    BasePathTool *mActiveTool;
    WorldChanger *mChanger;
};

#endif // BASEWORLDSCENE_H
