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
class WorldPath;

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

    int level();

    WorldPath *pointOnSegment(const QPointF &scenePos, QPointF &ptOnLine);
    QPointF nextNodePos(QGraphicsSceneMouseEvent *event);

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
    void beforeRemoveNode(WorldPath *path, int index, WorldNode *node);

private:
    Q_DISABLE_COPY(SelectMoveNodeTool)
    static SelectMoveNodeTool *mInstance;
    explicit SelectMoveNodeTool();
    ~SelectMoveNodeTool();

    void startSelecting();
    void updateSelection(QGraphicsSceneMouseEvent *event);
    void startMoving();
    void updateMovingItems(QGraphicsSceneMouseEvent *event);
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
    QPointF mStartScreenPos;
    QPointF mDropWorldPos;
    WorldNode *mClickedNode;
    NodeSet mMovingNodes;
    QGraphicsRectItem *mSelectionRectItem;
};

class AddRemoveNodeTool : public BasePathTool
{
    Q_OBJECT

public:
    static AddRemoveNodeTool *instance();
    static void deleteInstance();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Add and Remove Nodes"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    Q_DISABLE_COPY(AddRemoveNodeTool)
    static AddRemoveNodeTool *mInstance;
    explicit AddRemoveNodeTool();
    ~AddRemoveNodeTool();

    void updateFakeNode(const QPointF &scenePos);
};

class SelectMovePathTool : public BasePathTool
{
    Q_OBJECT

public:
    static SelectMovePathTool *instance();
    static void deleteInstance();

    void setScene(BaseGraphicsScene *scene);

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Select and Move Path"));
        //setShortcut(QKeySequence(tr("S")));
    }

private slots:
    void beforeRemovePath(WorldPathLayer *layer, int index, WorldPath *path);

private:
    Q_DISABLE_COPY(SelectMovePathTool)
    static SelectMovePathTool *mInstance;
    explicit SelectMovePathTool();
    ~SelectMovePathTool();

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

    WorldPath *topmostPathAt(const QPointF &scenePos);

    NodeSet movingNodes();

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    QPointF mStartScreenPos;
    QPointF mDropWorldPos;
    WorldPath *mClickedPath;
    PathSet mMovingPaths;
    QGraphicsRectItem *mSelectionRectItem;
};

class CreatePathTool : public BasePathTool
{
    Q_OBJECT

public:
    static CreatePathTool *instance();
    static void deleteInstance();

    void activate();
    void deactivate();

    void setScene(BaseGraphicsScene *scene);

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Create Path"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void finishNewPath(bool close);
    void clearNewPath();

private:
    Q_DISABLE_COPY(CreatePathTool)
    static CreatePathTool *mInstance;
    explicit CreatePathTool();
    ~CreatePathTool();

    WorldPath *mNewPath;
    QPointF mStartScenePos;
#if 1
    QGraphicsLineItem *mSegmentItem;
#else
    QGraphicsPathItem *mPolyItem;
    QGraphicsEllipseItem *mFirstNodeItem;
    QPolygonF mPoly;
    NodeList mNodes;
#endif
};

class AddPathSegmentsTool : public BasePathTool
{
    Q_OBJECT

public:
    static AddPathSegmentsTool *instance();
    static void deleteInstance();

    void activate();
    void deactivate();

    void setScene(BaseGraphicsScene *scene);

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Add Segments To Path"));
        //setShortcut(QKeySequence(tr("S")));
    }

    WorldPath *currentPath();

    void finishNewPath(bool close);
    void clearNewPath();

private:
    Q_DISABLE_COPY(AddPathSegmentsTool)
    static AddPathSegmentsTool *mInstance;
    explicit AddPathSegmentsTool();
    ~AddPathSegmentsTool();

    WorldPath *mPath;
    bool mAppend;
    QGraphicsLineItem *mSegmentItem;
};

#endif // PATHTOOLS_H
