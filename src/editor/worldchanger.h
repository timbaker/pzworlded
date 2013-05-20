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

#ifndef WORLDCHANGER_H
#define WORLDCHANGER_H

#include <QList>
#include <QObject>
#include <QPointF>

class WorldNode;

class WorldChanger;

class WorldChange
{
public:
    WorldChange(WorldChanger *changer);
    virtual ~WorldChange();

    void setChanger(WorldChanger *changer);

    virtual void redo() = 0;
    virtual void undo() = 0;

    virtual QString text() const = 0;

protected:
    WorldChanger *mChanger;
};

typedef QList<WorldChange*> WorldChangeList;

class WorldChanger : public QObject
{
    Q_OBJECT
public:
    WorldChanger();
    ~WorldChanger();

    void doMoveNode(WorldNode *node, const QPointF &pos);
    void afterMoveNode(WorldNode *node, const QPointF &prev);

    const WorldChangeList &changes() const
    { return mChanges; }

    int changeCount() const
    { return mChanges.size(); }

    WorldChangeList takeChanges();
    void undoAndForget();

    void undo();

signals:
    void afterMoveNodeSignal(WorldNode *node, const QPointF &prev);

private:
    WorldChangeList mChanges;
    WorldChangeList mChangesReversed;
};

#endif // WORLDCHANGER_H
