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

#ifndef PATHTOOLS_H
#define PATHTOOLS_H

#include "scenetools.h"

#include "global.h"

class BasePathScene;
class PathDocument;
class WorldNode;

class QUndoStack;

class BasePathTool : public AbstractTool
{
    Q_OBJECT
public:
    BasePathTool(const QString &name,
                 const QIcon &icon,
                 const QKeySequence &shortcut,
                 QObject *parent = 0);
    ~BasePathTool();

    void setScene(BaseGraphicsScene *scene);

    PathDocument *document();
    void beginUndoMacro(const QString &s);
    void endUndoMacro();

    virtual void keyPressEvent(QKeyEvent *event) { Q_UNUSED(event) }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }

public slots:
    void updateEnabledState();

protected:
    BasePathScene *mScene;
};

class SelectMoveNodeTool : public BasePathTool
{
    Q_OBJECT

public:
    static SelectMoveNodeTool *instance();
    static void deleteInstance();

    void setScene(BaseGraphicsScene *scene);

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Select and Move Nodes"));
        //setShortcut(QKeySequence(tr("S")));
    }

private slots:
    void nodeAboutToBeRemoved(WorldNode *node);

private:
    Q_DISABLE_COPY(SelectMoveNodeTool)
    static SelectMoveNodeTool *mInstance;
    explicit SelectMoveNodeTool();
    ~SelectMoveNodeTool();

    void startSelecting();
    void updateSelection(QGraphicsSceneMouseEvent *event);
    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);

    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    WorldNode *topmostNodeAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    QPointF mDropWorldPos;
    WorldNode *mClickedNode;
    NodeSet mMovingNodes;
    QGraphicsRectItem *mSelectionRectItem;
};

#endif // PATHTOOLS_H
