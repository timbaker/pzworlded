#ifndef PATHUNDOREDO_H
#define PATHUNDOREDO_H

#include <QUndoCommand>

class WorldChange;

class WorldChangeUndoCommand: public QUndoCommand
{
public:
    WorldChangeUndoCommand(WorldChange *change);
    ~WorldChangeUndoCommand();

    void undo();
    void redo();
    // bool mergeWith(const QUndoCommand *other);

private:
    WorldChange *mChange;
};

#endif // PATHUNDOREDO_H
