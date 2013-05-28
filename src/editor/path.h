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
class WorldPath;
class WorldPathLayer;
class WorldScript;

class WorldNode
{
public:
    WorldNode();
    WorldNode(qreal x, qreal y);
    WorldNode(const QPointF &mPos);
    ~WorldNode();

    void setPath(WorldPath *path)
    { Q_ASSERT(!path || !mPath); mPath = path; }
    WorldPath *path() const
    { return mPath; }

    WorldPathLayer *layer() const;

    void setPos(const QPointF &pos)
    { mPos = pos; }
    void setX(qreal x)
    { mPos.rx() = x; }
    void setY(qreal y)
    { mPos.ry() = y; }
    const QPointF &pos() const
    { return mPos; }

    WorldNode *clone() const;

    int index();

private:
    WorldPath *mPath;
    QPointF mPos;
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
    WorldPath(WorldPathLayer *layer);
    ~WorldPath();

    WorldPathLayer *layer() const
    { return mLayer; }

    void insertNode(int index, WorldNode *node);
    WorldNode *removeNode(int index);
    const NodeList &nodes() const
    { return mNodes; }
    WorldNode *nodeAt(int index);
    int nodeCount() const
    { return mNodes.size(); }
    int nodeCount(WorldNode *node)
    { return mNodes.count(node); }
    int indexOf(WorldNode *node)
    { return mNodes.indexOf(node); }
    WorldNode *first() const
    { return mNodes.size() ? mNodes.first() : 0; }
    WorldNode *last() const
    { return mNodes.last() ? mNodes.last() : 0; }

    QPointF nodePos(int index)
    {
        if (WorldNode *node = nodeAt(index))
            return node->pos();
        Q_ASSERT(false);
        return QPointF();
    }

    int index();

    void setClosed(bool closed)
    { mClosed = closed; }
    bool isClosed() const
    { return mClosed; }

    QRectF bounds();
    QPolygonF polygon(bool stroked = true);

    QRegion region();

    void setLayer(WorldPathLayer *layer);

    WorldPath *clone() const;

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

    PathTags &tags() { return mTags; }

    bool isVisible() const
    { return mVisible; }

    void setVisible(bool visible)
    { mVisible = visible; }

    void setStrokeWidth(qreal width)
    { mStrokeWidth = width; }
    qreal strokeWidth() const
    { return mStrokeWidth; }

    PathTexture &texture()
    { return mTexture; }

private:
    QList<WorldNode*> mNodes;
    PathTags mTags;

    QRectF mBounds;
    QPolygonF mPolygon;

    WorldPathLayer *mLayer;
    ScriptList mScripts;
    bool mVisible;
    bool mClosed;

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

    void insertPath(int index, WorldPath *path);
    WorldPath *removePath(int index);
    const QList<WorldPath*> &paths() const
    { return mPaths; }
    int pathCount() const
    { return mPaths.size(); }
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
    QList<WorldPath*> mPaths;
    bool mVisible;
    WorldLookup *mLookup;
};

#endif // PATH_H
