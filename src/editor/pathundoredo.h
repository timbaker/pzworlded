#ifndef PATHUNDOREDO_H
#define PATHUNDOREDO_H

#include <QPointF>
#include <QUndoCommand>

class PathDocument;
class WorldNode;

namespace PathUndoRedo {

class SwapUndoCommand : public QUndoCommand
{
public:
    void undo() { swap(); }
    void redo() { swap(); }
    virtual void swap() = 0;
};

class MoveNode : public SwapUndoCommand
{
public:
    static void push(PathDocument *doc, WorldNode *node, const QPointF &pos);
    MoveNode(PathDocument *doc, WorldNode *node, const QPointF &pos);
    void swap();

    PathDocument *mDocument;
    WorldNode *mNode;
    QPointF mPos;
};

} // namespace PathUndoRedo

#endif // PATHUNDOREDO_H
