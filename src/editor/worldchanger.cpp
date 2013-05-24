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

#include "worldchanger.h"

#include "path.h"
#include "pathworld.h"
#include "pathundoredo.h"

#include <QUndoStack>

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QVector<QVector<int> >;
#endif

namespace WorldChanges
{

class MoveNode : public WorldChange
{
public:
    MoveNode(WorldChanger *changer, WorldNode *node, const QPointF &pos) :
        WorldChange(changer),
        mNode(node),
        mNewPos(pos),
        mOldPos(node->pos())
    {

    }

    void redo()
    {
        mNode->setPos(mNewPos);
        mChanger->afterMoveNode(mNode, mOldPos);
    }

    void undo()
    {
        mNode->setPos(mOldPos);
        mChanger->afterMoveNode(mNode, mNewPos);
    }

    QString text() const
    {
        return mChanger->tr("Move Node");
    }

    WorldNode *mNode;
    QPointF mNewPos;
    QPointF mOldPos;
};

class AddNode : public WorldChange
{
public:
    AddNode(WorldChanger *changer, WorldPathLayer *layer, int index, WorldNode *node) :
        WorldChange(changer),
        mLayer(layer),
        mIndex(index),
        mNode(node)
    {

    }

    void redo()
    {
        mLayer->insertNode(mIndex, mNode);
        mChanger->afterAddNode(mNode);
    }

    void undo()
    {
        mLayer->removeNode(mIndex);
        mChanger->afterRemoveNode(mLayer, mIndex, mNode);
    }

    QString text() const
    {
        return mChanger->tr("Add Node");
    }

    WorldPathLayer *mLayer;
    int mIndex;
    WorldNode *mNode;
};

class RemoveNode : public AddNode
{
public:
    RemoveNode(WorldChanger *changer, WorldPathLayer *layer, int index, WorldNode *node) :
        AddNode(changer, layer, index, node)
    {}
    void redo() { AddNode::undo(); }
    void undo() { AddNode::redo(); }
    QString text() { return mChanger->tr("Remove Node"); }
};

class AddPath : public WorldChange
{
public:
    AddPath(WorldChanger *changer, WorldPathLayer *layer, int index, WorldPath *path) :
        WorldChange(changer),
        mLayer(layer),
        mIndex(index),
        mPath(path)
    {
    }

    void redo()
    {
        mLayer->insertPath(mIndex, mPath);
        mChanger->afterAddPath(mLayer, mIndex, mPath);
    }

    void undo()
    {
        mLayer->removePath(mIndex);
        mChanger->afterRemovePath(mLayer, mIndex, mPath);
    }

    QString text() const
    {
        return mChanger->tr("Add Path");
    }

    WorldPathLayer *mLayer;
    int mIndex;
    WorldPath *mPath;
};

class RemovePath : public AddPath
{
public:
    RemovePath(WorldChanger *changer, WorldPathLayer *layer, int index, WorldPath *path) :
        AddPath(changer, layer, index, path)
    { }
    void redo() { AddPath::undo(); }
    void undo() { AddPath::redo(); }
    QString text() const { return mChanger->tr("Remove Path"); }
};

class AddNodeToPath : public WorldChange
{
public:
    AddNodeToPath(WorldChanger *changer, WorldPath *path, int index, WorldNode *node) :
        WorldChange(changer),
        mPath(path),
        mIndex(index),
        mNode(node)
    {

    }

    void redo()
    {
        mPath->insertNode(mIndex, mNode);
        mChanger->afterAddNodeToPath(mPath, mIndex, mNode);
    }

    void undo()
    {
        mPath->removeNode(mIndex);
        mChanger->afterRemoveNodeFromPath(mPath, mIndex, mNode);
    }

    QString text() const
    {
        return mChanger->tr("Add Node To Path");
    }

    WorldPath *mPath;
    int mIndex;
    WorldNode *mNode;
};

class RemoveNodeFromPath : public AddNodeToPath
{
public:
    RemoveNodeFromPath(WorldChanger *changer, WorldPath *path, int index, WorldNode *node) :
        AddNodeToPath(changer, path, index, node)
    {}
    void redo() { AddNodeToPath::undo(); }
    void undo() { AddNodeToPath::redo(); }
    QString text() const { return mChanger->tr("Remove Node From Path"); }
};

class AddScriptToPath : public WorldChange
{
public:
    AddScriptToPath(WorldChanger *changer, WorldPath *path, int index, WorldScript *script) :
        WorldChange(changer),
        mPath(path),
        mIndex(index),
        mScript(script)
    {

    }

    ~AddScriptToPath()
    {
//        delete mScript;
    }

    void redo()
    {
        mPath->insertScript(mIndex, mScript);
        mChanger->afterAddScriptToPath(mPath, mIndex, mScript);
        mScript = 0;
    }

    void undo()
    {
        mScript = mPath->removeScript(mIndex);
        mChanger->afterRemoveScriptFromPath(mPath, mIndex, mScript);
    }

    QString text() const
    {
        return mChanger->tr("Add Script To Path");
    }

    WorldPath *mPath;
    int mIndex;
    WorldScript *mScript;
};

class ChangeScriptParameters : public WorldChange
{
public:
    ChangeScriptParameters(WorldChanger *changer, WorldScript *script, const ScriptParams &params) :
        WorldChange(changer),
        mScript(script),
        mNewParams(params),
        mOldParams(script->mParams)
    {

    }

    void redo()
    {
        mScript->mParams = mNewParams;
        mChanger->afterChangeScriptParameters(mScript);
    }

    void undo()
    {
        mScript->mParams = mOldParams;
        mChanger->afterChangeScriptParameters(mScript);
    }

    QString text() const
    {
        return mChanger->tr("Change Script Parameters");
    }

    WorldScript *mScript;
    ScriptParams mNewParams;
    ScriptParams mOldParams;
};

class SetPathVisible : public WorldChange
{
public:
    SetPathVisible(WorldChanger *changer, WorldPath *path, bool visible) :
        WorldChange(changer),
        mPath(path),
        mVisible(visible),
        mWasVisible(path->isVisible())
    {
    }

    void redo()
    {
        mPath->setVisible(mVisible);
        mChanger->afterSetPathVisible(mPath, mVisible);
    }

    void undo()
    {
        mPath->setVisible(mWasVisible);
        mChanger->afterSetPathVisible(mPath, mWasVisible);
    }

    QString text() const
    {
        return mChanger->tr(mVisible ? "Show Path" : "Hide Path");
    }

    WorldPath *mPath;
    bool mVisible;
    bool mWasVisible;
};

} // namespace WorldChanges;

/////

WorldChange::WorldChange(WorldChanger *changer) :
    mChanger(changer)
{
}

WorldChange::~WorldChange()
{
}

void WorldChange::setChanger(WorldChanger *changer)
{
    mChanger = changer;
}

using namespace WorldChanges;

/////

WorldChanger::WorldChanger() :
    mUndoStack(0)
{
}

WorldChanger::~WorldChanger()
{
    qDeleteAll(mChanges);
}

void WorldChanger::doAddNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    addChange(new AddNode(this, layer, index, node));
}

void WorldChanger::doRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    addChange(new RemoveNode(this, layer, index, node));
}

void WorldChanger::afterAddNode(WorldNode *node)
{
    emit afterAddNodeSignal(node);
}

void WorldChanger::afterRemoveNode(WorldPathLayer *layer, int index, WorldNode *node)
{
    emit afterRemoveNodeSignal(layer, index, node);
}

void WorldChanger::doMoveNode(WorldNode *node, const QPointF &pos)
{
    addChange(new MoveNode(this, node, pos));
}

void WorldChanger::afterMoveNode(WorldNode *node, const QPointF &prev)
{
    emit afterMoveNodeSignal(node, prev);
}

void WorldChanger::doAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    addChange(new AddPath(this, layer, index, path));
}

void WorldChanger::doRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    addChange(new RemovePath(this, layer, index, path));
}

void WorldChanger::afterAddPath(WorldPathLayer *layer, int index, WorldPath *path)
{
    emit afterAddPathSignal(layer, index, path);
}

void WorldChanger::afterRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    emit afterRemovePathSignal(layer, index, path);
}

void WorldChanger::doAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    addChange(new AddNodeToPath(this, path, index, node));
}

void WorldChanger::doRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    addChange(new RemoveNodeFromPath(this, path, index, node));
}

void WorldChanger::afterAddNodeToPath(WorldPath *path, int index, WorldNode *node)
{
    emit afterAddNodeToPathSignal(path, index, node);
}

void WorldChanger::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    emit afterRemoveNodeFromPathSignal(path, index, node);
}

void WorldChanger::doAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    addChange(new AddScriptToPath(this, path, index, script));
}

void WorldChanger::afterAddScriptToPath(WorldPath *path, int index, WorldScript *script)
{
    emit afterAddScriptToPathSignal(path, index, script);
}

void WorldChanger::afterRemoveScriptFromPath(WorldPath *path, int index, WorldScript *script)
{
    emit afterRemoveScriptFromPathSignal(path, index, script);
}

void WorldChanger::doChangeScriptParameters(WorldScript *script, const ScriptParams &params)
{
    addChange(new ChangeScriptParameters(this, script, params));
}

void WorldChanger::afterChangeScriptParameters(WorldScript *script)
{
    emit afterChangeScriptParametersSignal(script);
}

void WorldChanger::doSetPathVisible(WorldPath *path, bool visible)
{
    addChange(new SetPathVisible(this, path, visible));
}

void WorldChanger::afterSetPathVisible(WorldPath *path, bool visible)
{
    emit afterSetPathVisibleSignal(path, visible);
}

WorldChangeList WorldChanger::takeChanges(bool undoFirst)
{
    if (undoFirst) undo();
    WorldChangeList changes = mChanges;
    mChanges.clear();
    mChangesReversed.clear();
    return changes;
}

void WorldChanger::undoAndForget()
{
    qDeleteAll(takeChanges());
}

void WorldChanger::beginUndoMacro(QUndoStack *undoStack, const QString &text)
{
    Q_ASSERT(mUndoStack == 0);
    mUndoStack = undoStack;
    mUndoStack->beginMacro(text);
}

void WorldChanger::endUndoMacro()
{
    Q_ASSERT(mUndoStack != 0);
    mUndoStack->endMacro();
    mUndoStack = 0;
}

void WorldChanger::undo()
{
    foreach (WorldChange *c, mChangesReversed)
        c->undo();
}

void WorldChanger::addChange(WorldChange *change)
{
    if (mUndoStack != 0) {
        mUndoStack->push(new WorldChangeUndoCommand(change));
        return;
    }
    mChanges += change;
    mChangesReversed.insert(0, change);
    change->redo();
}
