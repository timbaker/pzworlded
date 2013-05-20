#include "pathundoredo.h"

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

void WorldChangeUndoCommand::undo()
{
    mChange->undo();
}

