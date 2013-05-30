#include "pathundoredo.h"

#include "undoredo.h"
#include "worldchanger.h"

WorldChangeUndoCommand::WorldChangeUndoCommand(WorldChange *change) :
    QUndoCommand(change->text()),
    mChange(change)
{
}

WorldChangeUndoCommand::~WorldChangeUndoCommand()
{
    delete mChange;
}

void WorldChangeUndoCommand::redo()
{
    mChange->redo();
}

bool WorldChangeUndoCommand::mergeWith(const QUndoCommand *other)
{
    if (other->id() == id()) {
        WorldChangeUndoCommand *o = (WorldChangeUndoCommand*) other;
        if (mChange->id() != -1 && mChange->id() == o->mChange->id())
            return mChange->merge(o->mChange);
    }
    return false;
}

void WorldChangeUndoCommand::undo()
{
    mChange->undo();
}

int WorldChangeUndoCommand::id() const
{
    return UndoCmd_WorldChange;
}

