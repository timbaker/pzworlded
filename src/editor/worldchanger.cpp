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
        mChanger->beforeRemovePath(mLayer, mIndex, mPath);
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

class ReorderPath : public WorldChange
{
public:
    ReorderPath(WorldChanger *changer, WorldPath *path, int newIndex) :
        WorldChange(changer),
        mLayer(path->layer()),
        mPath(path),
        mNewIndex(newIndex),
        mOldIndex(path->layer()->indexOf(path))
    {
    }

    void redo()
    {
        mLayer->removePath(mOldIndex);
        mLayer->insertPath(mNewIndex, mPath);
        mChanger->afterReorderPath(mPath, mOldIndex);
    }

    void undo()
    {
        mLayer->removePath(mNewIndex);
        mLayer->insertPath(mOldIndex, mPath);
        mChanger->afterReorderPath(mPath, mNewIndex);
    }

    QString text() const
    {
        return mChanger->tr((mNewIndex > mOldIndex) ? "Move Path Up" : "Move Path Down");
    }

    WorldPathLayer *mLayer;
    WorldPath *mPath;
    int mNewIndex;
    int mOldIndex;
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
        mChanger->beforeRemoveNodeFromPath(mPath, mIndex, mNode);
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

class AddPathLayer : public WorldChange
{
public:
    AddPathLayer(WorldChanger *changer, WorldLevel *wlevel, int index, WorldPathLayer *layer) :
        WorldChange(changer),
        mLevel(wlevel),
        mIndex(index),
        mLayer(layer)
    {
    }

    void redo()
    {
        mLevel->insertPathLayer(mIndex, mLayer);
        mChanger->afterAddPathLayer(mLevel, mIndex, mLayer);
    }

    void undo()
    {
        mChanger->beforeRemovePathLayer(mLevel, mIndex, mLayer);
        mLevel->removePathLayer(mIndex);
        mChanger->afterRemovePathLayer(mLevel, mIndex, mLayer);
    }

    QString text() const
    {
        return mChanger->tr("Add Path Layer");
    }

    WorldLevel *mLevel;
    int mIndex;
    WorldPathLayer *mLayer;
};

class RemovePathLayer : public AddPathLayer
{
public:
    RemovePathLayer(WorldChanger *changer, WorldLevel *wlevel, int index, WorldPathLayer *layer) :
        AddPathLayer(changer, wlevel, index, layer)
    { }
    void redo() { AddPathLayer::undo(); }
    void undo() { AddPathLayer::redo(); }
    QString text() const { return mChanger->tr("Remove Path Layer"); }
};

class ReorderPathLayer : public WorldChange
{
public:
    ReorderPathLayer(WorldChanger *changer, WorldLevel *wlevel, WorldPathLayer *layer, int newIndex) :
        WorldChange(changer),
        mLevel(wlevel),
        mNewIndex(newIndex),
        mOldIndex(wlevel->indexOf(layer)),
        mLayer(layer)
    {
    }

    void redo()
    {
        mLevel->removePathLayer(mOldIndex);
        mLevel->insertPathLayer(mNewIndex, mLayer);
        mChanger->afterReorderPathLayer(mLevel, mLayer, mOldIndex);
    }

    void undo()
    {
        mLevel->removePathLayer(mNewIndex);
        mLevel->insertPathLayer(mOldIndex, mLayer);
        mChanger->afterReorderPathLayer(mLevel, mLayer, mNewIndex);
    }

    QString text() const
    {
        return mChanger->tr((mNewIndex > mOldIndex) ? "Move Path Layer Up" : "Move Path Layer Down");
    }

    WorldLevel *mLevel;
    int mNewIndex;
    int mOldIndex;
    WorldPathLayer *mLayer;
};

class RenamePathLayer : public WorldChange
{
public:
    RenamePathLayer(WorldChanger *changer, WorldPathLayer *layer, const QString &name) :
        WorldChange(changer),
        mLayer(layer),
        mNewName(name),
        mOldName(layer->name())
    {
    }

    void redo()
    {
        mLayer->setName(mNewName);
        mChanger->afterRenamePathLayer(mLayer, mOldName);
    }

    void undo()
    {
        mLayer->setName(mOldName);
        mChanger->afterRenamePathLayer(mLayer, mNewName);
    }

    QString text() const
    {
        return mChanger->tr("Rename Path Layer");
    }

    WorldPathLayer *mLayer;
    QString mNewName;
    QString mOldName;
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

class SetPathClosed : public WorldChange
{
public:
    SetPathClosed(WorldChanger *changer, WorldPath *path, bool closed) :
        WorldChange(changer),
        mPath(path),
        mClosed(closed),
        mWasClosed(path->isClosed())
    {
    }

    void redo()
    {
        mPath->setClosed(mClosed);
        mChanger->afterSetPathClosed(mPath, mClosed);
    }

    void undo()
    {
        mPath->setClosed(mWasClosed);
        mChanger->afterSetPathClosed(mPath, mWasClosed);
    }

    QString text() const
    {
        return mChanger->tr(mClosed ? "Close Path" : "Open Path");
    }

    WorldPath *mPath;
    bool mClosed;
    bool mWasClosed;
};

class SetPathLayerVisible : public WorldChange
{
public:
    SetPathLayerVisible(WorldChanger *changer, WorldPathLayer *layer, bool visible) :
        WorldChange(changer),
        mLayer(layer),
        mVisible(visible),
        mWasVisible(layer->isVisible())
    {
    }

    void redo()
    {
        mLayer->setVisible(mVisible);
        mChanger->afterSetPathLayerVisible(mLayer, mVisible);
    }

    void undo()
    {
        mLayer->setVisible(mWasVisible);
        mChanger->afterSetPathLayerVisible(mLayer, mWasVisible);
    }

    QString text() const
    {
        return mChanger->tr(mVisible ? "Show PathLayer" : "Hide PathLayer");
    }

    WorldPathLayer *mLayer;
    bool mVisible;
    bool mWasVisible;
};

class SetPathLayersVisible : public WorldChange
{
public:
    SetPathLayersVisible(WorldChanger *changer, WorldLevel *wlevel, bool visible) :
        WorldChange(changer),
        mLevel(wlevel),
        mVisible(visible),
        mWasVisible(wlevel->isPathLayersVisible())
    {
    }

    void redo()
    {
        mLevel->setPathLayersVisible(mVisible);
        mChanger->afterSetPathLayersVisible(mLevel, mVisible);
    }

    void undo()
    {
        mLevel->setPathLayersVisible(mWasVisible);
        mChanger->afterSetPathLayersVisible(mLevel, mWasVisible);
    }

    QString text() const
    {
        return mChanger->tr(mVisible ? "Show Level (Paths)" : "Hide Level (Paths)");
    }

    WorldLevel *mLevel;
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

WorldChanger::WorldChanger(PathWorld *world) :
    mWorld(world),
    mUndoStack(0)
{
}

WorldChanger::~WorldChanger()
{
    qDeleteAll(mChanges);
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

void WorldChanger::beforeRemovePath(WorldPathLayer *layer, int index, WorldPath *path)
{
    emit beforeRemovePathSignal(layer, index, path);
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

void WorldChanger::beforeRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    emit beforeRemoveNodeFromPathSignal(path, index, node);
}

void WorldChanger::afterRemoveNodeFromPath(WorldPath *path, int index, WorldNode *node)
{
    emit afterRemoveNodeFromPathSignal(path, index, node);
}

void WorldChanger::doSetPathClosed(WorldPath *path, bool closed)
{
    addChange(new SetPathClosed(this, path, closed));
}

void WorldChanger::afterSetPathClosed(WorldPath *path, bool wasClosed)
{
    emit afterSetPathClosedSignal(path, wasClosed);
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

void WorldChanger::doAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    addChange(new AddPathLayer(this, wlevel, index, layer));
}

void WorldChanger::doRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    addChange(new RemovePathLayer(this, wlevel, index, layer));
}

void WorldChanger::afterAddPathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    emit afterAddPathLayerSignal(wlevel, index, layer);
}

void WorldChanger::beforeRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    emit beforeRemovePathLayerSignal(wlevel, index, layer);
}

void WorldChanger::afterRemovePathLayer(WorldLevel *wlevel, int index, WorldPathLayer *layer)
{
    emit afterRemovePathLayerSignal(wlevel, index, layer);
}

void WorldChanger::doReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int newIndex)
{
    addChange(new ReorderPathLayer(this, wlevel, layer, newIndex));
}

void WorldChanger::afterReorderPathLayer(WorldLevel *wlevel, WorldPathLayer *layer, int oldIndex)
{
    emit afterReorderPathLayerSignal(wlevel, layer, oldIndex);
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

void WorldChanger::doReorderPath(WorldPath *path, int newIndex)
{
    addChange(new ReorderPath(this, path, newIndex));
}

void WorldChanger::afterReorderPath(WorldPath *path, int oldIndex)
{
    emit afterReorderPathSignal(path, oldIndex);
}

void WorldChanger::doSetPathLayerVisible(WorldPathLayer *layer, bool visible)
{
    addChange(new SetPathLayerVisible(this, layer, visible));
}

void WorldChanger::afterSetPathLayerVisible(WorldPathLayer *layer, bool visible)
{
    emit afterSetPathLayerVisibleSignal(layer, visible);
}

void WorldChanger::doRenamePathLayer(WorldPathLayer *layer, const QString &name)
{
    addChange(new RenamePathLayer(this, layer, name));
}

void WorldChanger::afterRenamePathLayer(WorldPathLayer *layer, const QString &oldName)
{
    emit afterRenamePathLayerSignal(layer, oldName);
}

void WorldChanger::doSetPathLayersVisible(WorldLevel *wlevel, bool visible)
{
    addChange(new SetPathLayersVisible(this, wlevel, visible));
}

void WorldChanger::afterSetPathLayersVisible(WorldLevel *wlevel, bool visible)
{
    emit afterSetPathLayersVisibleSignal(wlevel, visible);
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

void WorldChanger::beginUndoCommand(QUndoStack *undoStack)
{
    Q_ASSERT(mUndoStack == 0);
    mUndoStack = undoStack;
}

void WorldChanger::endUndoCommand()
{
    Q_ASSERT(mUndoStack != 0);
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
