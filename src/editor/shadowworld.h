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

#ifndef SHADOWWORLD_H
#define SHADOWWORLD_H

#include "pathworld.h"

class WorldLookup;
class WorldNode;

class WorldChange;
class WorldModifier;

class ShadowWorld : public QObject, public PathWorld
{
    Q_OBJECT
public:
    ShadowWorld(PathWorld *orig);

    WorldLookup *lookup() const
    { return mLookup; }

    void addMoveNode(WorldNode *node, const QPointF &pos);
    void removeMoveNode(WorldNode *node);

    void addOffsetNode(WorldNode *node, const QPointF &offset);
    void removeOffsetNode(WorldNode *node);

    void nodeMoved(WorldNode *node);
    void nodeMoved(const QPointF &oldPos, const QPointF &newPos);

signals:
    void nodesMoved(const QRectF &area);

private slots:
    void processChanges();

private:
    PathWorld *mOrig;
    WorldLookup *mLookup;
    QList<WorldModifier*> mModifiers;
    QList<WorldChange*> mChanges;
    QRectF mMovedNodeArea;
};

#endif // SHADOWWORLD_H
