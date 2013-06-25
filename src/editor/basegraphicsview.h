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

#ifndef BASEGRAPHICSVIEW_H
#define BASEGRAPHICSVIEW_H

#include <QGraphicsView>
#include <QTimer>

class BaseGraphicsScene;
class BaseGraphicsView;
class Zoomable;

class QToolButton;

class MiniMap : public QGraphicsView
{
    Q_OBJECT
public:
    MiniMap(BaseGraphicsView *parent);

    void setScene(BaseGraphicsScene *scene);
    void viewRectChanged();
    qreal scale();
    void addItem(QGraphicsItem *item);
    void removeItem(QGraphicsItem *item);

public slots:
    void sceneRectChanged(const QRectF &sceneRect);
    void bigger();
    void smaller();
    void widthChanged(int width);

private:
    bool event(QEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    BaseGraphicsView *mParentView;
    BaseGraphicsScene *mScene;
    QGraphicsPolygonItem *mViewportItem;
    QList<QGraphicsItem*> mExtraItems;
    QFrame *mButtons;
    QToolButton *mBiggerButton;
    QToolButton *mSmallerButton;
    int mWidth;
};

class BaseGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    enum AllowOpenGL
    {
        NeverGL,
        PreferenceGL,
        AlwaysGL
    };

    explicit BaseGraphicsView(AllowOpenGL openGL, QWidget *parent = 0);
    
    void setHandScrolling(bool handScrolling);

    void hideEvent(QHideEvent *event);
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void resizeEvent(QResizeEvent *event);

    Zoomable *zoomable() const { return mZoomable; }

    void setScene(BaseGraphicsScene *scene);
    BaseGraphicsScene *scene() const { return mScene; }

    void scrollContentsBy(int dx, int dy);

    void addMiniMapItem(QGraphicsItem *item);
    void removeMiniMapItem(QGraphicsItem *item);
    virtual QRectF sceneRectForMiniMap() const;

    void ensureRectVisible(const QRectF &rect, int xmargin = 50, int ymargin = 50);

signals:
    void statusBarCoordinatesChanged(int x, int y);

public slots:
    void adjustScale(qreal scale);

private slots:
    void setUseOpenGL(bool useOpenGL);

protected:
    bool mHandScrolling;
    QPoint mLastMouseGlobalPos;
    QPointF mLastMouseScenePos;
    Zoomable *mZoomable;

    enum ScrollDirection {
        ScrollNone = 0,
        ScrollLeft = 0x01,
        ScrollUp = 0x02,
        ScrollRight = 0x04,
        ScrollDown = 0x08
    };

private slots:
    void autoScrollTimeout();

protected:
    void startAutoScroll();
    void stopAutoScroll();

    bool mMousePressed;
    QTimer mScrollTimer;
    int mScrollDirection;
    int mScrollMagnitude;
    BaseGraphicsScene *mScene;
    MiniMap *mMiniMap;
};

#endif // BASEGRAPHICSVIEW_H
