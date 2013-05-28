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

#ifndef PATH_H
#define PATH_H

#include "global.h"

#include <QList>
#include <QMap>
#include <QRectF>
#include <QRegion>
#include <QPolygonF>
#include <QString>
#include <QTransform>

class WorldLookup;
class WorldPathLayer;
class WorldScript;

class WorldNode
{
public:
    WorldNode();
    WorldNode(id_t id, qreal x, qreal y);
    WorldNode(id_t id, const QPointF &p);
    ~WorldNode();

    void setPos(const QPointF &pos)
    { p = pos; }
    void setX(qreal x)
    { p.rx() = x; }
    void setY(qreal y)
    { p.ry() = y; }
    const QPointF &pos() const
    { return p; }

    WorldNode *clone() const;

    void addedToPath(WorldPath *path);
    void removedFromPath(WorldPath *path);

    int index();

    WorldPathLayer *layer;
    id_t id;
    QPointF p;
    QMap<WorldPath*,int> mPaths;
};

class WorldTexture
{
public:
    QString mName;
    QString mFileName;
    QSize mSize;
    unsigned int mGLid;
};

class PathTexture
{
public:
    PathTexture() : mTexture(0), mScale(1, 1), mRotation(0), mAlignWorld(false) {}
    WorldTexture *mTexture;
    QTransform mTransform;
    QSizeF mScale;
    qreal mRotation;
    QPointF mTranslation;
    bool mAlignWorld;
};

class WorldPath
{
public:
    WorldPath();
    WorldPath(id_t id);
    ~WorldPath();

    WorldPathLayer *layer() const
    { return mLayer; }

    void insertNode(int index, WorldNode *node);
    WorldNode *removeNode(int index);
    WorldNode *nodeAt(int index);
    int nodeCount() const
    { return nodes.size(); }
    int nodeCount(WorldNode *node)
    { return nodes.count(node); }
    WorldNode *first() const
    { return nodes.size() ? nodes.first() : 0; }
    WorldNode *last() const
    { return nodes.last() ? nodes.last() : 0; }

    void registerWithNodes();
    void unregisterFromNodes();

    bool isClosed() const;

    QRectF bounds();
    QPolygonF polygon();

    QRegion region();

    void setOwner(WorldPathLayer *owner);

    WorldPath *clone(WorldPathLayer *layer) const;

    void nodeMoved()
    {
        mBounds = QRectF();
        mPolygon.clear();
    }

    void insertScript(int index, WorldScript *script);
    WorldScript *removeScript(int index);
    const ScriptList &scripts() const
    { return mScripts; }
    int scriptCount() const
    { return mScripts.size(); }

    bool isVisible() const
    { return mVisible; }

    void setVisible(bool visible)
    { mVisible = visible; }

    void setStrokeWidth(qreal width)
    { mStrokeWidth = width; }
    qreal strokeWidth() const
    { return mStrokeWidth; }

    id_t id;
    QList<WorldNode*> nodes;
    QMap<QString,QString> tags;

    QRectF mBounds;
    QPolygonF mPolygon;

    WorldPathLayer *mLayer;
    ScriptList mScripts;
    bool mVisible;

    PathTexture mTexture;
    qreal mStrokeWidth;
};

QPolygonF strokePath(WorldPath *path, qreal thickness);
void offsetPath(WorldPath *path, qreal offset, QPolygonF &fwd, QPolygonF &bwd);
QRegion polygonRegion(const QPolygonF &poly);
QBitmap polygonBitmap(const QPolygonF &poly);

class WorldPathLayer
{
public:
    WorldPathLayer();
    WorldPathLayer(const QString &mName);
    ~WorldPathLayer();

    void setName(const QString &name)
    { mName = name; }
    const QString &name() const
    { return mName; }

    void setLevel(WorldLevel *wlevel)
    { mLevel = wlevel; }
    WorldLevel *wlevel() const
    { return mLevel; }
    int level() const;

    void insertNode(int index, WorldNode *node);
    WorldNode *removeNode(int index);
    const QList<WorldNode*> &nodes() const
    { return mNodes; }
    int nodeCount() const
    { return mNodes.size(); }
    WorldNode *node(id_t id);
    int indexOf(WorldNode *node);

    void insertPath(int index, WorldPath *path);
    WorldPath *removePath(int index);
    const QList<WorldPath*> &paths() const
    { return mPaths; }
    int pathCount() const
    { return mPaths.size(); }
    WorldPath *path(id_t id);
    int indexOf(WorldPath *path);

    void setVisible(bool visible)
    { mVisible = visible; }
    bool isVisible() const
    { return mVisible; }

    WorldLookup *lookup();

//    QList<WorldPath*> paths(QRectF &bounds);
//    QList<WorldPath*> paths(qreal x, qreal y, qreal width, qreal height);

    WorldPathLayer *clone() const;

private:
    WorldLevel *mLevel;
    QString mName;
    QList<WorldNode*> mNodes;
    QMap<id_t,WorldNode*> mNodeByID;
    QList<WorldPath*> mPaths;
    QMap<id_t,WorldPath*> mPathByID;
    bool mVisible;
    WorldLookup *mLookup;
};

#endif // PATH_H
