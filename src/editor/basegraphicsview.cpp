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

#include "basegraphicsview.h"

#include "basegraphicsscene.h"
#include "preferences.h"
#include "zoomable.h"

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QScrollBar>

BaseGraphicsView::BaseGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
    , mHandScrolling(false)
    , mZoomable(new Zoomable(this))
    , mMousePressed(false)
    , mScrollTimer(this)
    , mMiniMap(0)
{
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
//    setDragMode(QGraphicsView::ScrollHandDrag);

    /* Since Qt 4.5, setting this attribute yields significant repaint
     * reduction when the view is being resized. */
    viewport()->setAttribute(Qt::WA_StaticContents);

    /* Since Qt 4.6, mouse tracking is disabled when no graphics item uses
     * hover events. We need to set it since our scene wants the events. */
    viewport()->setMouseTracking(true);

    // Adjustment for antialiasing is done by the items that need it
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));

//    mScrollTimer.setSingleShot(true);
    mScrollTimer.setInterval(30);
    connect(&mScrollTimer, SIGNAL(timeout()), SLOT(autoScrollTimeout()));

    mMiniMap = new MiniMap(this);

    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(showMiniMapChanged(bool)), mMiniMap, SLOT(setVisible(bool)));
}

void BaseGraphicsView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}

void BaseGraphicsView::autoScrollTimeout()
{
    if(mScrollDirection & ScrollLeft) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value()-mScrollMagnitude);
    }
    if(mScrollDirection & ScrollRight) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value()+mScrollMagnitude);
    }
    if(mScrollDirection & ScrollUp) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()-mScrollMagnitude);
    }
    if(mScrollDirection & ScrollDown) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()+mScrollMagnitude);
    }
#if 0
    if(rubberBand->isVisible()) { // update the rubber band
        QPoint mouseDownView = mapFromScene(mouseDownPos);
        QPoint diff = (lastMouseViewPos-mouseDownView);
        rubberBand->setGeometry(qMin(lastMouseViewPos.x(), mouseDownView.x()), qMin(lastMouseViewPos.y(), mouseDownView.y()), qAbs(diff.x()), qAbs(diff.y()));
    }
#endif
}

void BaseGraphicsView::startAutoScroll()
{
    Q_ASSERT(mMousePressed);
    if (!mScrollTimer.isActive())
        mScrollTimer.start();
}

void BaseGraphicsView::stopAutoScroll()
{
    if (mScrollTimer.isActive())
        mScrollTimer.stop();
}

void BaseGraphicsView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
    setInteractive(!mHandScrolling);

    if (mHandScrolling) {
        mLastMouseGlobalPos = QCursor::pos();
        // FIXME: the cursor changes to arrow during drag
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        viewport()->grabMouse();
    } else {
        viewport()->releaseMouse();
        QApplication::restoreOverrideCursor();
    }
}

void BaseGraphicsView::hideEvent(QHideEvent *event)
{
    // Disable hand scrolling when the view gets hidden in any way
    setHandScrolling(false);
    QGraphicsView::hideEvent(event);
}

void BaseGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(event->delta());

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMouseGlobalPos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void BaseGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        mMousePressed = true;
    }

    mScene->setEventView(this);
    QGraphicsView::mousePressEvent(event);
}

void BaseGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (mHandScrolling) {
        QScrollBar *hBar = horizontalScrollBar();
        QScrollBar *vBar = verticalScrollBar();
        const QPoint d = event->globalPos() - mLastMouseGlobalPos;
        hBar->setValue(hBar->value() + (isRightToLeft() ? d.x() : -d.x()));
        vBar->setValue(vBar->value() - d.y());

        mLastMouseGlobalPos = event->globalPos();
        return;
    }

    // If the Progress dialog pops up during the mousePressEvent it seems
    // the mouseReleaseEvent never gets sent.  This happened with the PasteCellsTool.
    if (mMousePressed && !event->buttons()) {
        qDebug() << "BaseGraphicsView::mouseMoveEvent un-pressing mouse";
        mMousePressed = false;
        stopAutoScroll();
    }

#if 1
    if (mMousePressed) {
        QPoint pos = event->pos();
        int distance = 0;
        mScrollDirection = ScrollNone;
        mScrollMagnitude = 0;
#define SCROLL_DISTANCE 64
        // determine the direction of automatic scrolling
        if(pos.x() < SCROLL_DISTANCE) {
            mScrollDirection = ScrollLeft;
            distance = pos.x();
        }
        else if(width()-pos.x() < SCROLL_DISTANCE) {
            mScrollDirection = ScrollRight;
            distance = width()-pos.x();
        }
        if(pos.y() < SCROLL_DISTANCE) {
            mScrollDirection += ScrollUp;
            distance = pos.y();
        }
        else if(height()-pos.y() < SCROLL_DISTANCE) {
            mScrollDirection += ScrollDown;
            distance = height()-pos.y();
        }
        if(mScrollDirection) {
            mScrollMagnitude = qRound((SCROLL_DISTANCE-distance)/8);
            startAutoScroll();
        } else
            stopAutoScroll();
    }
#endif

    mScene->setEventView(this);
    QGraphicsView::mouseMoveEvent(event);
    mLastMouseGlobalPos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));
}

void BaseGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(false);
        return;
    }

    mMousePressed = false;
    stopAutoScroll();

    mScene->setEventView(this);
    QGraphicsView::mouseReleaseEvent(event);
}

void BaseGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    mMiniMap->viewRectChanged();
}

void BaseGraphicsView::setScene(BaseGraphicsScene *scene)
{
    mScene = scene;
    QGraphicsView::setScene(scene);

    mMiniMap->setScene(scene);
}

void BaseGraphicsView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx, dy);
    mMiniMap->viewRectChanged();
}

void BaseGraphicsView::addMiniMapItem(QGraphicsItem *item)
{
    mMiniMap->addItem(item);
}

// Wrapper around QGraphicsView::ensureVisible.  In ensureVisible, when the rectangle to
// view is larger than the viewport, the final position is often undesirable with multiple
// scrolls taking place.  In this implementation, when the rectangle to view (plus margins)
// does not fit in the current view or is not partially visible already, the view is centered
// on the rectangle's center.
void BaseGraphicsView::ensureRectVisible(const QRectF &rect, int xmargin, int ymargin)
{
    QRect rectToView = mapFromScene(rect).boundingRect();
    rectToView.adjust(-xmargin, -ymargin, xmargin, ymargin);
    QRect viewportRect = viewport()->rect(); // includes scrollbars?
    if (viewportRect.contains(rectToView, true))
        return;
    if (rectToView.width() > viewportRect.width() ||
            rectToView.height() > viewportRect.height() ||
            !viewportRect.intersects(rectToView)) {
        centerOn(rect.center());
    } else {
        ensureVisible(rect, xmargin, ymargin);
    }
}

/////

#include <QGraphicsPolygonItem>

MiniMap::MiniMap(BaseGraphicsView *parent)
    : QGraphicsView(parent)
    , mParentView(parent)
    , mViewportItem(0)
    , mExtraItem(0)
{
    setFrameStyle(NoFrame);

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::gray);
    QGraphicsView::setScene(scene);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    mViewportItem = new QGraphicsPolygonItem();
    mViewportItem->setPen(QPen(Qt::white));
    mViewportItem->setZValue(100);
    scene->addItem(mViewportItem);

    setGeometry(20, 20, 220, 220);

    // When visible, the MiniMap obscures part of the scene, slowing down scrolling. :-{
//    setVisible(false);
}

void MiniMap::setScene(BaseGraphicsScene *scene)
{
    mScene = scene;
    sceneRectChanged(mScene->sceneRect());
    connect(mScene, SIGNAL(sceneRectChanged(QRectF)), SLOT(sceneRectChanged(QRectF)));
}

void MiniMap::viewRectChanged()
{
    QRect rect = mParentView->rect();

    int hsbh = mParentView->horizontalScrollBar()->isVisible() ? mParentView->horizontalScrollBar()->height() : 0;
    int vsbw = mParentView->verticalScrollBar()->isVisible() ? mParentView->verticalScrollBar()->width() : 0;
    rect.adjust(0, 0, -vsbw, -hsbh);

    QPolygonF polygon = mParentView->mapToScene(rect);
    mViewportItem->setPolygon(polygon);
    mViewportItem->setScale(scale());
}

void MiniMap::addItem(QGraphicsItem *item)
{
    mExtraItem = item;
    mExtraItem->setScale(scale());
    scene()->addItem(mExtraItem);
}

void MiniMap::sceneRectChanged(const QRectF &sceneRect)
{
    qreal scale = this->scale();
    QSizeF size = sceneRect.size();
    // No idea where the extra 3 pixels is coming from...
    setGeometry(20, 20, std::ceil(size.width() * scale) + 3, std::ceil(size.height() * scale) + 3);
#if 1
    // The sceneRect may not start at 0,0.
    scene()->setSceneRect(QRectF(sceneRect.topLeft() * scale, sceneRect.size() * scale));
#else
    scene()->setSceneRect(rect().adjusted(0,0,-3,-3));
#endif
    viewRectChanged();

    if (mExtraItem)
        mExtraItem->setScale(scale);
}

void MiniMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mParentView->centerOn(mapToScene(event->pos()) / scale());
}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        mParentView->centerOn(mapToScene(event->pos()) / scale());
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

qreal MiniMap::scale()
{
    QRectF sceneRect = mScene->sceneRect();
    QSizeF size = sceneRect.size();
    if (size.isEmpty())
        return 1.0;
    return (size.width() > size.height()) ? 256.0 / size.width() : 256.0 / size.height();
}
