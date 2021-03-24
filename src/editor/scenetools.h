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

#include "singleton.h"

#include <QIcon>
#include <QGraphicsPolygonItem>
#include <QKeySequence>
#include <QMetaType>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QTime>
#include <QTimer>

class BaseCellSceneTool;
class BaseWorldSceneTool;
class BaseGraphicsScene;
class BaseGraphicsView;
class CellScene;
class DnDItem;
class LightSwitchOverlay;
class MapComposite;
class ObjectItem;
class SpawnPointItem;
class SubMapItem;
class TrafficLines;
class WorldBMP;
class WorldBMPItem;
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

    virtual void activate() {}
    virtual void deactivate() {}

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
    virtual BaseGraphicsScene *scene() const = 0;

    virtual void beginClearScene() { Q_ASSERT(scene()); deactivate(); }
    virtual void endClearScene() { Q_ASSERT(scene()); activate(); }

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
    BaseGraphicsScene *scene() const;

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
class SelectMoveObjectTool : public BaseCellSceneTool
{
    Q_OBJECT

public:
    static SelectMoveObjectTool *instance();
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

    void showContextMenu(const QPointF &scenePos, const QPoint &screenPos);

private:
    Q_DISABLE_COPY(SelectMoveObjectTool)

    explicit SelectMoveObjectTool();
    ~SelectMoveObjectTool() {}

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
    static SelectMoveObjectTool *mInstance;
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

    void activate();
    void deactivate();

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

    void showContextMenu(const QPointF &scenePos, const QPoint &screenPos);

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    SubMapItem *topmostItemAt(const QPointF &scenePos);
    LightSwitchOverlay *topmostSwitchAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    SubMapItem *mClickedItem;
    QSet<SubMapItem*> mMovingItems;
    QList<DnDItem*> mDnDItems;
    QGraphicsPolygonItem *mMapHighlightItem;
    MapComposite *mHighlightedMap;
    static SubMapTool *mInstance;
};

/////

class SpawnPointCursorItem;

class SpawnPointTool : public BaseCellSceneTool, public Singleton<SpawnPointTool>
{
    Q_OBJECT
public:
    SpawnPointTool();

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
//    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

     bool affectsLots() const { return false; }
     bool affectsObjects() const { return true; }

    void languageChanged()
    {
        setName(tr("Create Spawn Point"));
        //setShortcut(QKeySequence(tr("S")));
    }

private:
    void showContextMenu(const QPointF &scenePos, const QPoint &screenPos);
    SpawnPointItem *topmostItemAt(const QPointF &scenePos);

private:
    bool mContextMenuVisible;
    QTime mContextMenuShown;
    SpawnPointCursorItem *mCursorItem;
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
    BaseGraphicsScene *scene() const;

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
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);

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

    void showContextMenu(const QPointF &scenePos, const QPoint &screenPos);

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

/**
  * This WorldScene tool selects and moves BMPToTMXImages.
  */
class WorldBMPTool : public BaseWorldSceneTool
{
    Q_OBJECT

public:
    static WorldBMPTool *instance();
    static void deleteInstance();

    explicit WorldBMPTool();
    ~WorldBMPTool();

    void setScene(BaseGraphicsScene *scene);

    void keyPressEvent(QKeyEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void languageChanged()
    {
        setName(tr("Select and Move BMP Images"));
        //setShortcut(QKeySequence(tr("S")));
    }

private slots:
    void bmpAboutToBeRemoved(int index);

private:
    Q_DISABLE_COPY(WorldBMPTool)

    void startSelecting();
    void updateSelection(QGraphicsSceneMouseEvent *event);

    void startMoving();
    void updateMovingItems(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);
    void cancelMoving();

    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    WorldBMPItem *topmostItemAt(const QPointF &scenePos);

    Mode mMode;
    bool mMousePressed;
    QPointF mStartScenePos;
    QPoint mDragOffset;
    WorldBMPItem *mClickedItem;
    QList<WorldBMP*> mMovingBMPs;
    QGraphicsPolygonItem *mSelectionRectItem;
    static WorldBMPTool *mInstance;
};

#endif // SCENETOOLS_H
