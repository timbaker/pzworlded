#include "pathundoredo.h"

#include "pathdocument.h"

#include <QUndoStack>

using namespace PathUndoRedo;

void MoveNode::push(PathDocument *doc, WorldNode *node, const QPointF &pos)
{
    doc->undoStack()->push(new MoveNode(doc, node, pos));
}

MoveNode::MoveNode(PathDocument *doc, WorldNode *node, const QPointF &pos) :
    mDocument(doc),
    mNode(node),
    mPos(pos)
{
}

void MoveNode::swap()
{
    mPos = mDocument->moveNode(mNode, mPos);
}

/////
