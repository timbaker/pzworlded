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

    PathList lookupPaths(const QRectF &sceneRect);
private:
    WorldPathLayer *mLayer;
    BasePathScene *mScene;
};

class WorldLevelItem : public QGraphicsItem
{
public:
    WorldLevelItem(WorldLevel *wlevel, BasePathScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const
    { return QRectF(); }

    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *)
    {}

    void afterAddPathLayer(int index, WorldPathLayer *layer);
    void beforeRemovePathLayer(int index, WorldPathLayer *layer);
    void afterReorderPathLayer(WorldPathLayer *layer, int oldIndex);

    void afterSetPathLayerVisible(WorldPathLayer *layer, bool visible);

private:
    WorldLevel *mLevel;
    BasePathScene *mScene;
    QList<PathLayerItem*> mPathLayerItems;
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

    NodeList lookupNodes(const QRectF &sceneRect);
    WorldNode *topmostNodeAt(const QPointF &scenePos);

    void nodeRemoved(WorldNode *node);

    void redrawNode(id_t id);

    qreal nodeRadius() const;

    void currentPathLayerChanged();

private:
    BasePathScene *mScene;
    NodeSet mSelectedNodes;
    QMap<WorldNode*,QPointF> mNodeOffset;
    id_t mHoverNode;
};

struct PathSegment
{
    PathSegment() :
        mPath(0),
        mNodeIndex(-1)
    {
    }

    PathSegment(WorldPath *path, int nodeIndex) :
        mPath(path),
        mNodeIndex(nodeIndex)
    {
    }

    WorldPath *path() const
    { return mPath; }

    int nodeIndex() const
    { return mNodeIndex; }

    bool operator==(const PathSegment &other)
    { return mPath == other.mPath && mNodeIndex == other.mNodeIndex; }

    bool operator!=(const PathSegment &other)
    { return !(*this == other); }

    WorldPath *mPath;
    int mNodeIndex;
};

typedef QList<PathSegment> SegmentList;

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

    WorldPathLayer *currentPathLayer() const;

    void setSelectedPaths(const PathSet &selection);
    const PathSet &selectedPaths() const
    { return mSelectedPaths; }
    QList<WorldPath *> lookupPaths(const QRectF &sceneRect);
    WorldPath *topmostPathAt(const QPointF &scenePos, int *indexPtr = 0);

    void setSelectedSegments(const SegmentList &segments);
    const SegmentList &selectedSegments() const
    { return mSelectedSegments; }

    void setHighlightSegment(WorldPath *path, int nodeIndex);
    const PathSegment &highlightSegment() const
    { return mHighlightSegment; }

    WorldChanger *changer() const
    { return mChanger; }

    WorldPath *mHighlightPath;

    unsigned int loadGLTexture(const QString &fileName);

public slots:
    void afterAddNode(WorldNode *node);
    void afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node);
    void afterMoveNode(WorldNode *node, const QPointF &prev);

    void afterAddPath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);

    void afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void afterReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex);

    void afterSetPathLayerVisible(WorldPathLayer *layer, bool visible);

    void nodeMoved(WorldNode *node);

    void currentPathLayerChanged(WorldPathLayer *layer);

    void pathTextureChanged(WorldPath *path);

public:
    unsigned int mTextureId;
private:
    PathDocument *mDocument;
    BasePathRenderer *mRenderer;
    PathSet mSelectedPaths;
    SegmentList mSelectedSegments;
    PathSegment mHighlightSegment;
    NodesItem *mNodeItem;
    BasePathTool *mActiveTool;
    WorldChanger *mChanger;
    QList<WorldLevelItem*> mLevelItems;
};

#endif // BASEWORLDSCENE_H
