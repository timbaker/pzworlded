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

#include "path.h"

using namespace WorldPath;

const int InvalidId = 0;

/////

Node::Node() :
    id(InvalidId)
{
}

Node::Node(id_t id, coord_t x, coord_t y) :
    id(id),
    p(x, y)
{
}

Node::Node(id_t id, const Point &p) :
    id(id),
    p(p)
{
}

Node::~Node()
{
}

/////

Path::Path() :
    id(InvalidId)
{
}

Path::Path(id_t id) :
    id(id)
{
}

Path::~Path()
{
}

void Path::insertNode(int index, Node *node)
{
    mPolygon.clear();
    mBounds = Rect();
    nodes.insert(index, node);
}

Node *Path::removeNode(int index)
{
    mPolygon.clear();
    mBounds = Rect();
    return nodes.takeAt(index);
}

bool Path::isClosed() const
{
    return (nodes.size() > 1) && (nodes.first() == nodes.last());
}

Rect Path::bounds()
{
    if (mBounds.isNull()) {
        mBounds = polygon().boundingRect();
    }
    return mBounds;
}

Polygon Path::polygon()
{
    if (mPolygon.isEmpty()) {
        foreach (Node *node, nodes)
            mPolygon += node->p;
    }
    return mPolygon;
}

/////

Layer::Layer() :
    mLevel(0)
{
}

Layer::Layer(const QString &name, int level) :
    mName(name),
    mLevel(level)
{
}

Layer::~Layer()
{
    qDeleteAll(mPaths);
    qDeleteAll(mNodes);
}

void Layer::insertNode(int index, Node *node)
{
    mNodes.insert(index, node);
    mNodeByID[node->id] = node;
}

Node *Layer::removeNode(int index)
{
    Node *node = mNodes.takeAt(index);
    mNodeByID.remove(node->id);
    return node;
}

Node *Layer::node(id_t id)
{
    if (mNodeByID.contains(id))
        return mNodeByID[id];
    return 0;
}

void Layer::insertPath(int index, Path *path)
{
    mPaths.insert(index, path);
}

Path *Layer::removePath(int index)
{
    return mPaths.takeAt(index);
}

QList<Path *> Layer::paths(Rect &bounds)
{
    return mPaths;
}

QList<Path*> Layer::paths(coord_t x, coord_t y, coord_t width, coord_t height)
{
    return mPaths; // TODO: spatial partitioning
}
