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
#define PZ_OPENGL_WIDGET 1
#if PZ_OPENGL_WIDGET
#include <QOpenGLWidget>
#else
#include <QGLWidget>
#endif
#include <QMouseEvent>
#include <QScrollBar>

BaseGraphicsView::BaseGraphicsView(AllowOpenGL openGL, QWidget *parent)
    : QGraphicsView(parent)
    , mHandScrolling(false)
    , mZoomable(new Zoomable(this))
    , mMousePressed(false)
    , mScrollTimer(this)
    , mScene(0)
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

#ifndef QT_NO_OPENGL
    if (openGL == PreferenceGL) {
        Preferences *prefs = Preferences::instance();
        setUseOpenGL(prefs->useOpenGL());
        connect(prefs, SIGNAL(useOpenGLChanged(bool)), SLOT(setUseOpenGL(bool)));
    } else if (openGL == AlwaysGL) {
        setUseOpenGL(true);
    }
#endif
}

void BaseGraphicsView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());

    if (mScene)
        mScene->viewTransformChanged(this);
}

// I put this in BaseGraphicsView so WorldScene could use it, but the OpenGL
// backend chokes on overly-large BMP images.  It uses QCache in the
// QGLTextureCache::insert() method which *deletes the texture* and other
// code doesn't test for that.
void BaseGraphicsView::setUseOpenGL(bool useOpenGL)
{
#ifndef QT_NO_OPENGL
    QWidget *oldViewport = viewport();
    QWidget *newViewport = viewport();
#if PZ_OPENGL_WIDGET
    if (useOpenGL) {
        if (!qobject_cast<QOpenGLWidget*>(viewport())) {
            QSurfaceFormat format = QSurfaceFormat::defaultFormat();
            format.setDepthBufferSize(0);
            QOpenGLWidget *openGLWidget = new QOpenGLWidget();
//            openGLWidget->setFormat(format);
            newViewport = openGLWidget;
        }
    } else {
        if (qobject_cast<QOpenGLWidget*>(viewport())) {
            newViewport = nullptr;
        }
    }
#else
    if (useOpenGL && QGLFormat::hasOpenGL()) {
        if (!qobject_cast<QGLWidget*>(viewport())) {
            QGLFormat format = QGLFormat::defaultFormat();
            format.setDepth(false); // No need for a depth buffer
            format.setSampleBuffers(true); // Enable anti-aliasing
            newViewport = new QGLWidget(format);
        }
    } else {
        if (qobject_cast<QGLWidget*>(viewport()))
            newViewport = 0;
    }
#endif
    if (newViewport != oldViewport) {
        if (mMiniMap) {
            mMiniMap->setVisible(false);
            mMiniMap->setParent(static_cast<QWidget*>(parent()));
        }
        setViewport(newViewport);
        if (mMiniMap) {
            mMiniMap->setParent(this);
            mMiniMap->setVisible(Preferences::instance()->showMiniMap());
            if (scene())
                mMiniMap->sceneRectChanged(scene()->sceneRect());
        }
    }

    QWidget *v = viewport();
    v->setAttribute(Qt::WA_StaticContents);
    v->setMouseTracking(true);
#endif
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

    if (mMousePressed) {
        QPoint pos = event->pos();
        int distance = 0;
        mScrollDirection = ScrollNone;
        mScrollMagnitude = 0;
#define SCROLL_DISTANCE 64
        // determine the direction of automatic scrolling
        if (pos.x() < SCROLL_DISTANCE) {
            mScrollDirection = ScrollLeft;
            distance = pos.x();
        }
        else if (width() - pos.x() < SCROLL_DISTANCE) {
            mScrollDirection = ScrollRight;
            distance = width()-pos.x();
        }
        if (pos.y() < SCROLL_DISTANCE) {
            mScrollDirection += ScrollUp;
            distance = pos.y();
        }
        else if(height() - pos.y() < SCROLL_DISTANCE) {
            mScrollDirection += ScrollDown;
            distance = height()-pos.y();
        }
        if(mScrollDirection) {
            mScrollMagnitude = qRound(float(SCROLL_DISTANCE-distance)/8);
            startAutoScroll();
        } else
            stopAutoScroll();
    }

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

void BaseGraphicsView::removeMiniMapItem(QGraphicsItem *item)
{
    mMiniMap->removeItem(item);
}

QRectF BaseGraphicsView::sceneRectForMiniMap() const
{
    return mScene->sceneRect();
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
#include <QHBoxLayout>
#include <QToolButton>
#include <cmath>

MiniMap::MiniMap(BaseGraphicsView *parent)
    : QGraphicsView(parent)
    , mParentView(parent)
    , mViewportItem(0)
    , mButtons(new QFrame(this))
    , mBiggerButton(new QToolButton(mButtons))
    , mSmallerButton(new QToolButton(mButtons))
{
    setFrameStyle(NoFrame);

    // For the smaller/bigger buttons
    setMouseTracking(true);

    Preferences *prefs = Preferences::instance();
    setVisible(prefs->showMiniMap());
    mWidth = prefs->miniMapWidth();
    connect(prefs, SIGNAL(showMiniMapChanged(bool)), SLOT(setVisible(bool)));
    connect(prefs, SIGNAL(miniMapWidthChanged(int)), SLOT(widthChanged(int)));

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::gray);
    QGraphicsView::setScene(scene);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    mViewportItem = new QGraphicsPolygonItem();
    QPen pen(Qt::white);
    pen.setCosmetic(true);
    mViewportItem->setPen(pen);
    mViewportItem->setZValue(100);
    scene->addItem(mViewportItem);

    QHBoxLayout *layout = new QHBoxLayout(mButtons);
    layout->setContentsMargins(2, 2, 0, 0);
    layout->setSpacing(2);
    mButtons->setLayout(layout);
    mButtons->setVisible(false);

    QToolButton *button = mSmallerButton;
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/zoom-out.png")));
    button->setToolTip(tr("Make the MiniMap smaller"));
    connect(button, SIGNAL(clicked()), SLOT(smaller()));
    layout->addWidget(button);

    button = mBiggerButton;
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/zoom-in.png")));
    button->setToolTip(tr("Make the MiniMap larger"));
    connect(button, SIGNAL(clicked()), SLOT(bigger()));
    layout->addWidget(button);

#if 0
    button = new QToolButton(mButtons);
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/edit-redo.png")));
    button->setToolTip(tr("Refresh the MiniMap image"));
    connect(button, SIGNAL(clicked()), SLOT(updateImage()));
    layout->addWidget(button);
#endif

    setGeometry(20, 20, 220, 220);

    // When visible, the MiniMap obscures part of the scene, slowing down scrolling. :-{
//    setVisible(false);
}

void MiniMap::setScene(BaseGraphicsScene *scene)
{
    mScene = scene;
    widthChanged(mWidth);
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
    item->setScale(scale());
    scene()->addItem(item);
    mExtraItems += item;
}

void MiniMap::removeItem(QGraphicsItem *item)
{
    if (mExtraItems.contains(item)) {
        mExtraItems.removeAll(item);
        scene()->removeItem(item);
    }
}

void MiniMap::sceneRectChanged(const QRectF &_sceneRect)
{
    Q_UNUSED(_sceneRect)

    QRectF sceneRect = mParentView->sceneRectForMiniMap();

    qreal scale = this->scale();
    QSizeF size = sceneRect.size();
    // No idea where the extra 3 pixels is coming from...
    setGeometry(20, 20, std::ceil(size.width() * scale) + 3, std::ceil(size.height() * scale) + 3);

    // The sceneRect may not start at 0,0.
    scene()->setSceneRect(QRectF(sceneRect.topLeft() * scale, sceneRect.size() * scale));

    viewRectChanged();

    foreach (QGraphicsItem *item, mExtraItems)
        item->setScale(scale);
}

void MiniMap::bigger()
{
    Preferences::instance()->setMiniMapWidth(qMin(mWidth + 32, MINIMAP_WIDTH_MAX));
}

void MiniMap::smaller()
{
    Preferences::instance()->setMiniMapWidth(qMax(mWidth - 32, MINIMAP_WIDTH_MIN));
}

void MiniMap::widthChanged(int width)
{
    mWidth = width;
    sceneRectChanged(mScene->sceneRect());

    mSmallerButton->setEnabled(mWidth > MINIMAP_WIDTH_MIN);
    mBiggerButton->setEnabled(mWidth < MINIMAP_WIDTH_MAX);
}

bool MiniMap::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter:
        break;
    case QEvent::Leave:
        mButtons->setVisible(false);
        break;
    default:
        break;
    }

    return QGraphicsView::event(event);
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
    else {
        QRect hotSpot = mButtons->rect().adjusted(0, 0, 12, 12); //(0, 0, 64, 32);
        mButtons->setVisible(hotSpot.contains(event->pos()));
    }
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

qreal MiniMap::scale()
{
    QRectF sceneRect = mParentView->sceneRectForMiniMap();
    QSizeF size = sceneRect.size();
    if (size.isEmpty())
        return 1.0;
    return (size.width() > size.height()) ? mWidth / size.width() : mWidth / size.height();
}
