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

#include "global.h"

#include <QList>
#include <QObject>
#include <QPointF>

class PathWorld;
class WorldNode;
class WorldPath;
class WorldPathLayer;
class WorldScript;

class QUndoStack;

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
    WorldChanger(PathWorld *world);
    ~WorldChanger();

    PathWorld *world() const
    { return mWorld; }

    void doAddNode(WorldPathLayer *layer, int index, WorldNode *node);
    void doRemoveNode(WorldPathLayer *layer, int index, WorldNode *node);
    void afterAddNode(WorldNode *node);
    void afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node);

    void doMoveNode(WorldNode *node, const QPointF &pos);
    void afterMoveNode(WorldNode *node, const QPointF &prev);

    void doAddPath(WorldPathLayer *layer, int index, WorldPath *path);
    void doRemovePath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterAddPath(WorldPathLayer *layer, int index, WorldPath *path);
    void afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path);

    void doAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void doRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);
    void afterAddNodeToPath(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node);

    void doAddScriptToPath(WorldPath *path, int index, WorldScript *script);
    void afterAddScriptToPath(WorldPath *path, int index, WorldScript *script);
    void afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script);

    void doAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void doRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void afterRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer);

    void doReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int newIndex);
    void afterReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex);

    void doChangeScriptParameters(WorldScript *script, const ScriptParams &params);
    void afterChangeScriptParameters(WorldScript *script);

    void doSetPathVisible(WorldPath *path, bool visible);
    void afterSetPathVisible(WorldPath *path, bool visible);

    void doSetPathLayerVisible(WorldPathLayer *layer, bool visible);
    void afterSetPathLayerVisible(WorldPathLayer *layer, bool visible);

    void doSetPathLayersVisible(WorldLevel *wlevel, bool visible);
    void afterSetPathLayersVisible(WorldLevel *wlevel, bool visible);

    const WorldChangeList &changes() const
    { return mChanges; }

    int changeCount() const
    { return mChanges.size(); }

    WorldChangeList takeChanges(bool undoFirst = true);
    void undoAndForget();

    void beginUndoMacro(QUndoStack *undoStack, const QString &text);
    void endUndoMacro();

    void beginUndoCommand(QUndoStack *undoStack);
    void endUndoCommand();

    void undo();

signals:
    void afterAddNodeSignal(WorldNode *node);
    void afterRemoveNodeSignal(WorldPathLayer *layer, int index, WorldNode *node);
    void afterMoveNodeSignal(WorldNode *node, const QPointF &prev);

    void afterAddPathSignal(WorldPathLayer *layer, int index, WorldPath *path);
    void afterRemovePathSignal(WorldPathLayer *layer, int index, WorldPath *path);
    void afterAddNodeToPathSignal(WorldPath *path, int index, WorldNode *node);
    void afterRemoveNodeFromPathSignal(WorldPath *path, int index, WorldNode *node);

    void afterAddScriptToPathSignal(WorldPath *path, int index, WorldScript *script);
    void afterRemoveScriptFromPathSignal(WorldPath *path, int index, WorldScript *script);

    void afterAddPathLayerSignal(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void beforeRemovePathLayerSignal(WorldLevel *wlevel, int index, WorldPathLayer *layer);
    void afterRemovePathLayerSignal(WorldLevel *wlevel, int index, WorldPathLayer *layer);

    void afterReorderPathLayerSignal(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex);

    void afterChangeScriptParametersSignal(WorldScript *script);

    void afterSetPathVisibleSignal(WorldPath *path, bool visible);
    void afterSetPathLayerVisibleSignal(WorldPathLayer *layer, bool visible);
    void afterSetPathLayersVisibleSignal(WorldLevel *wlevel, bool visible);

private:
    void addChange(WorldChange *change);

private:
    PathWorld *mWorld;
    WorldChangeList mChanges;
    WorldChangeList mChangesReversed;
    QUndoStack *mUndoStack;
};

#endif // WORLDCHANGER_H
