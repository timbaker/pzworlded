/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef SCENETOOLS_H
#define SCENETOOLS_H

#include <QIcon>
#include <QKeySequence>
#include <QMetaType>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QString>

class BaseCellSceneTool;
class BaseWorldSceneTool;
class BaseGraphicsScene;
class BaseGraphicsView;
class CellScene;
class DnDItem;
class ObjectItem;
class SubMapItem;
class WorldCellObject;

class QGraphicsScene;
class QGraphicsSceneMouseEvent;
class QKeyEvent;

/**
  * Base class for tools in CellScene and WorldScene.
  */
class AbstractTool : public QObject
{
    Q_OBJECT
public:
    enum ToolType {
        CellToolType,
        WorldToolType
    };

    AbstractTool(const QString &name,
                 const QIcon &icon,
                 const QKeySequence &shortcut,
                 ToolType type,
                 QObject *parent = 0);

    virtual ~AbstractTool() {}

    QString name() const { return mName; }
    void setName(const QString &name) { mName = name; }

    QIcon icon() const { return mIcon; }
    void setIcon(const QIcon &icon) { mIcon = icon; }

    QKeySequence shortcut() const { return mShortcut; }
    void setShortcut(const QKeySequence &shortcut) { mShortcut = shortcut; }

    QString statusInfo() const { return mStatusInfo; }
    void setStatusInfo(const QString &statusInfo);

    bool isEnabled() const { return mEnabled; }
    void setEnabled(bool enabled);

    bool isCurrent() const;

    bool isWorldTool() const { return mType == WorldToolType; }
    bool isCellTool() const { return mType == CellToolType; }

    BaseWorldSceneTool *asWorldTool();
    BaseCellSceneTool *asCellTool();

    /**
     * Called when the application language changed.
     */
    virtual void languageChanged() = 0;

    virtual void setScene(BaseGraphicsScene *scene) = 0;

signals:
    void statusInfoChanged(const QString &statusInfo);
    void enabledChanged(bool enabled);

public slots:
    virtual void updateEnabledState() = 0;

private:
    QString mName;
    QIcon mIcon;
    QKeySequence mShortcut;
    QString mStatusInfo;
    bool mEnabled;
    ToolType mType;
    BaseGraphicsScene *mScene;
};

// Needed for QVariant handling
Q_DECLARE_METATYPE(AbstractTool*)

/**
  * Base class for CellScene tools.
  */
class BaseCellSceneTool : public AbstractTool
{
    Q_OBJECT
public:
    BaseCellSceneTool(const QString &name,
                      const QIcon &icon,
                      const QKeySequence &shortcut,
                      QObject *parent = 0);
    ~BaseCellSceneTool();

    void setScene(BaseGraphicsScene *scene);

    virtual void activate();
    virtual void deactivate();

    virtual void keyPressEvent(QKeyEvent *event) { Q_UNUSED(event) }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }

    virtual bool affectsLots() const = 0;
    virtual bool affectsObjects() const = 0;

public slots:
    void updateEnabledState();

    void setEventView(BaseGraphicsView *view);
protected:
    CellScene *mScene;
    BaseGraphicsView *mEventView;
};

/////

#define MIN_OBJECT_SIZE (1.0)

/**
  * This CellScene tool creates new WorldCellObjects.
  */
class CreateObjectTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    static CreateObjectTool *instance();
    static void deleteInstance();

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

     bool affectsLots() const { return false; }
     bool affectsObjects() const { return true; }

    void languageChanged()
    {
        setName(tr("Create Object"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    Q_DISABLE_COPY(CreateObjectTool)

    explicit CreateObjectTool();
    ~CreateObjectTool() {}

    void startNewMapObject(const QPointF &pos);
    WorldCellObject *clearNewMapObjectItem();
    void cancelNewMapObject();
    void finishNewMapObject();

private:
    QPointF mAnchorPos;
    ObjectItem *mItem;
    static CreateObjectTool *mInstance;
};

/////

/**
  * This CellScene tool selects and moves WorldCellObjects.
  */
class ObjectTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    static ObjectTool *instance();
    static void deleteInstance();

    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    bool affectsLots() const { return false; }
    bool affectsObjects() const { return true; }

    void languageChanged()
    {
        setName(tr("Select and Move Objects"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void startSelecting();
    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);

private:
    Q_DISABLE_COPY(ObjectTool)

    explicit ObjectTool();
    ~ObjectTool() {}

    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    ObjectItem *topmostItemAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    ObjectItem *mClickedItem;
    QSet<ObjectItem*> mMovingItems;
    static ObjectTool *mInstance;
};

/////

/**
  * This CellScene tool selects and moves WorldCellLots.
  */
class SubMapTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    static SubMapTool *instance();
    static void deleteInstance();

    explicit SubMapTool();
    ~SubMapTool() {}

    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    bool affectsLots() const { return true; }
    bool affectsObjects() const { return false; }

    void languageChanged()
    {
        setName(tr("Select and Move Lots"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void startSelecting();
    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);
    void cancelMoving();

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    SubMapItem *topmostItemAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    SubMapItem *mClickedItem;
    QSet<SubMapItem*> mMovingItems;
    QList<DnDItem*> mDnDItems;
    static SubMapTool *mInstance;
};

/////

class WorldScene;

/**
  * Base class for WorldScene tools.
  */
class BaseWorldSceneTool : public AbstractTool
{
    Q_OBJECT
public:
    BaseWorldSceneTool(const QString &name,
                       const QIcon &icon,
                       const QKeySequence &shortcut,
                       QObject *parent = 0);
    ~BaseWorldSceneTool();

    void setScene(BaseGraphicsScene *scene);

    virtual void activate();
    virtual void deactivate();

    virtual void keyPressEvent(QKeyEvent *event) { Q_UNUSED(event) }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { Q_UNUSED(event) }

public slots:
    void updateEnabledState();

public:
    void setEventView(BaseGraphicsView *view);

protected:
    QPointF restrictDragging(const QVector<QPoint> &cellPositions, const QPointF &startScenePos,
                             const QPointF &currentScenePos);

protected:
    WorldScene *mScene;
    BaseGraphicsView *mEventView;
};

/////

class DragCellItem;
class WorldCellItem;
class WorldCell;

class QGraphicsView;

#include <QGraphicsPolygonItem>
#include <QTimer>

/**
  * This WorldScene tool selects and moves WorldCells.
  */
class WorldCellTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static WorldCellTool *instance();
    static void deleteInstance();

    explicit WorldCellTool();
    ~WorldCellTool();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Select and Move Cells"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void startSelecting();
    void updateSelection(QGraphicsSceneMouseEvent *event);
    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);
    void pushCellToMove(WorldCell *cell, const QPoint &offset);

    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    WorldCellItem *topmostItemAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    QPoint mDropTilePos;
    WorldCellItem *mClickedItem;
    QList<WorldCell*> mMovingCells;
    QList<WorldCell*> mOrderedMovingCells;
    QList<DragCellItem*> mDnDItems;
    QGraphicsPolygonItem *mSelectionRectItem;
    static WorldCellTool *mInstance;
};

/////

class PasteCellItem;

/**
  * This WorldScene tool pastes WorldCells from the clipboard.
  */
class PasteCellsTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static PasteCellsTool *instance();
    static void deleteInstance();

    explicit PasteCellsTool();
    ~PasteCellsTool();

    void activate();
    void deactivate();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Paste Cells"));
        //setShortcut(QKeySequence(tr("S")));
    }

    void setScene(BaseGraphicsScene *scene);

public slots:
    void updateEnabledState();

private:
    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void pasteCells(const QPointF &pos);
    void cancelMoving();

    QPointF mStartScenePos;
    QPoint mDropTilePos;
    QList<PasteCellItem*> mDnDItems;
    static PasteCellsTool *mInstance;
};

/////

class Road;
class RoadItem;

/**
  * This WorldScene tool creates Roads.
  */
class CreateRoadTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static CreateRoadTool *instance();
    static void deleteInstance();

    explicit CreateRoadTool();
    ~CreateRoadTool();

    void activate();
    void deactivate();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Create Roads"));
        //setShortcut(QKeySequence(tr("S")));
    }

    void setCurrentRoadWidth(int width)
    { mCurrentRoadWidth = width; }

    int curretRoadWidth() const
    { return mCurrentRoadWidth; }

private:
    Q_DISABLE_COPY(CreateRoadTool)

    void startNewRoadItem(const QPointF &scenePos);
    void clearNewRoadItem();
    void cancelNewRoadItem();
    void finishNewRoadItem();

    QPoint mStartRoadPos;
    Road *mRoad;
    RoadItem *mRoadItem;
    bool mCreating;
    int mCurrentRoadWidth;
    QGraphicsPolygonItem *mCursorItem;
    static CreateRoadTool *mInstance;
};

/////

/**
  * This WorldScene tool edits Road endpoints.
  */
class EditRoadTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static EditRoadTool *instance();
    static void deleteInstance();

    explicit EditRoadTool();
    ~EditRoadTool();

    void activate();
    void deactivate();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Edit Roads"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    Q_DISABLE_COPY(EditRoadTool)

    void startMoving(const QPointF &scenePos);
    void finishMoving();
    void cancelMoving();

    void updateHandles(Road *road);

    RoadItem *mSelectedRoadItem;
    Road *mRoad;
    RoadItem *mRoadItem;
    bool mMoving;
    bool mMovingStart;
    QGraphicsPolygonItem *mStartHandle, *mEndHandle;
    bool mHandlesVisible;

    static EditRoadTool *mInstance;
};

/////

/**
  * This WorldScene tool selects and moves Roads.
  */
class SelectMoveRoadTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static SelectMoveRoadTool *instance();
    static void deleteInstance();

    explicit SelectMoveRoadTool();
    ~SelectMoveRoadTool();

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Select and Move Roads"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    Q_DISABLE_COPY(SelectMoveRoadTool)

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

    RoadItem *topmostItemAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    QPoint mDropRoadPos;
    RoadItem *mClickedItem;
    QList<Road*> mMovingRoads;
    QGraphicsPolygonItem *mSelectionRectItem;
    static SelectMoveRoadTool *mInstance;
};

#endif // SCENETOOLS_H
