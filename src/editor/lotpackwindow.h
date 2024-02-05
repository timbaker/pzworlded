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

#ifndef LOTPACKWINDOW_H
#define LOTPACKWINDOW_H

#include <QMainWindow>

#include "basegraphicsscene.h"
#include "basegraphicsview.h"

#include "tilelayer.h"
#include "ztilelayergroup.h"

#include <QGraphicsItem>

class IsoWorld;

namespace Tiled {
class Cell;
class Map;
class MapRenderer;
class Tile;
}

namespace Ui {
class LotPackWindow;
}

class LotPackScene;
class LotPackLayerGroup : public Tiled::ZTileLayerGroup
{
public:
    LotPackLayerGroup(IsoWorld *world, Tiled::Map *map, int level);

    QRect bounds() const;
    QMargins drawMargins() const;

    bool orderedCellsAt(const QPoint &point, QVector<const Tiled::Cell*>& cells,
                        QVector<qreal> &opacities) const;

    void prepareDrawing(const Tiled::MapRenderer *renderer, const QRect &rect);

    IsoWorld *mWorld;
    QVector<Tiled::SparseTileGrid*> mGrids;
    LotPackScene *mScene;
};

class LotPackLayerGroupItem : public QGraphicsItem
{
public:
    LotPackLayerGroupItem(LotPackLayerGroup *lg, Tiled::MapRenderer *renderer);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    int level() const { return mLayerGroup->level(); }

    QRectF mBoundingRect;
    LotPackLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
};

/**
  * Item that draws the grid-lines in a LotPackScene.
  */
class IsoWorldGridItem : public QGraphicsItem
{
public:
    IsoWorldGridItem(LotPackScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void setWorld(IsoWorld *world);

private:
    LotPackScene *mScene;
    QRectF mBoundingRect;
};

class LotPackMiniMapItem : public QGraphicsItem
{
public:
    LotPackMiniMapItem(LotPackScene *scene);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setWorld(IsoWorld *world);

    LotPackScene *mScene;
    QRectF mBoundingRect;
    IsoWorldGridItem *mGridItem;
};

class LotHeader;
#include <QSet>
class LotPackScene : public BaseGraphicsScene
{
    Q_OBJECT
public:
    LotPackScene(QWidget *parent = 0);
    virtual void setTool(AbstractTool *tool) { Q_UNUSED(tool) }

    void setWorld(IsoWorld *world);
    IsoWorld *world() const { return mWorld; }

    Tiled::MapRenderer *renderer() const { return mRenderer; }

    void setMaxLevel(int max);

    QMap<QString,Tiled::Tile*> mTileByName;
    QSet<LotHeader*> mHeadersExamined;

public slots:
    void showRoomDefs(bool show);
    void highlightCurrentLevel();
    void levelAbove();
    void levelBelow();

private:
    IsoWorld *mWorld;
    Tiled::Map *mMap;
    Tiled::MapRenderer *mRenderer;
    QList<LotPackLayerGroup*> mLayerGroups;
    QVector<LotPackLayerGroupItem*> mLayerGroupItems;
    QVector<QGraphicsItem*> mRoomDefGroups;
    bool mShowRoomDefs;
    QGraphicsRectItem *mDarkRectangle;
    int mCurrentLevel;
};

class LotPackView : public BaseGraphicsView
{
    Q_OBJECT
public:
    LotPackView(QWidget *parent = 0);
    ~LotPackView();

    LotPackScene *scene() const
    { return mScene; }

    void setWorld(IsoWorld *world);

    void scrollContentsBy(int dx, int dy);

    bool viewportEvent(QEvent *event);

signals:
    void tilePositionChanged(const QPoint &tilePos);

private slots:
    void recenter();

private:
    LotPackScene *mScene;
    IsoWorld *mWorld;
    LotPackMiniMapItem *mMiniMapItem;
    QPoint mTilePos;
    bool mRecenterScheduled;
};

class LotPackWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit LotPackWindow(QWidget *parent = 0);
    ~LotPackWindow();

    void addRecentDirectory(const QString &f);
    void setRecentMenu();
    
    void closeEvent(QCloseEvent *e);

private slots:
    void open();
    void openRecent();
    void open(const QString &directory);
    void closeWorld();
    void zoomIn();
    void zoomOut();
    void zoomNormal();
    void updateZoom();

    void tilePositionChanged(const QPoint &tilePos);

private:
    Ui::LotPackWindow *ui;
    LotPackView *mView;
    IsoWorld *mWorld;
};

#endif // LOTPACKWINDOW_H
