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

#include "worldgenview.h"

#include "../editor/zoomable.h"

#include <QFile>
#include <QTextStream>
#include <QWheelEvent>

using namespace WorldGen;

/////

/////
LSystem::LSystem() :
    m_fAngle(90),
    m_fInitAngle(0),
    m_fStepSize(0.65f),
    m_fSegment(6),
    m_iMaxDepth(4),
    m_pszName(QString()),
    m_sStack(100)
{
    InitRulesArray();

#if 0 // fern_leaf.lse
    m_pszName = QLatin1String("Fern Leaf #1");
    m_fAngle = 8;
    m_fInitAngle = 278;
    m_fStepSize = 0.5;
    m_iSegment = 100;
    m_iMaxDepth = 1;
    m_pszAxiom = QLatin1String("F");
    m_pszRules['F'-'A'] = QLatin1String("|[5+F][7-F]-|[4+F][6-F]-|[3+F][5-F]-|F");
#endif
}

QRectF LSystem::boundingRect()
{
    if (mBounds.isEmpty() && mPointPairs.size()) {
        QPointF p = mPointPairs.first();
        qreal minX = p.x(), maxX = p.x(), minY = p.y(), maxY = p.y();
        foreach (QPointF p, mPointPairs) {
            minX = qMin(minX, p.x());
            minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x());
            maxY = qMax(maxY, p.y());
        }
        mBounds = QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
    }
    return mBounds;
}

void LSystem::reset()
{
    currentPos = QPoint();
    mPointPairs.resize(0);
    mPointPairs.reserve(m_iSegments * 2);
    m_sStack.resize(0);
    DrawLSystems(m_pszAxiom, m_fInitAngle, 0);
    mBounds = QRectF();
}

bool LSystem::LoadFile(const QString &fileName)
{
    InitRulesArray();

    m_fAngle = 90;
    m_fInitAngle = 0;
    m_fStepSize = 0.65f;
    m_fSegment = 6;
    m_iMaxDepth = 4;
    m_pszName.clear();

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream ts(&file);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.isEmpty())
            continue;
        int n = line.indexOf(QLatin1Char('='));
        if (n == -1)
            continue;
        QString key = line.mid(0, n).toLower();
        QString value = line.mid(n + 1);
        if (key == QLatin1String("axiom")) { m_pszAxiom = value; }
        else if (key == QLatin1String("segment")) { m_fSegment = value.toDouble(); }
        else if (key == QLatin1String("maxdepth")) { m_iMaxDepth = value.toInt(); }
        else if (key == QLatin1String("stepsize")) { m_fStepSize = value.toDouble(); }
        else if (key == QLatin1String("angle")) { m_fAngle = value.toDouble(); }
        else if (key == QLatin1String("initangle")) { m_fInitAngle = value.toDouble(); }
        else if (key == QLatin1String("description")) { m_pszName = value; }
        else if (key.size() == 1){
            char rule = line.mid(0, n)[0].toLatin1();
            if (rule >= 'A' && rule <= 'Z')
                m_pszRules[rule-'A'] = value;
        }
    }

    return true;
}

#define _deg2rad(a) (a * 3.141592653589793238462643) / 180.0

void LSystem::DrawLSystems(QString lstring, qreal angle, int depth)
{
    char at;									// Cache the character
    int number = 0;								// Number support
    int	len = lstring.length();				// Cache the length

    QPointF start = currentPos;

    for (int i=0;i<len;i++) {
        at = lstring[i].toLatin1();

        if (depth < m_iMaxDepth && (at >= 'A' && at <= 'Z')) {
            qreal old = m_fSegment;
            m_fSegment = m_fSegment * m_fStepSize;
            DrawLSystems(Rule(at), angle, depth+1);
            m_fSegment = old;
        } else if (at == 'F' || at == '|') {
#if 1
            start.setX(start.x() + (m_fSegment*cos(_deg2rad(angle))));
            start.setY(start.y() + (m_fSegment*sin(_deg2rad(angle))));
#else
            int mx = Round(start.x() + (m_iSegment*cos(_deg2rad(angle))));
            int my = Round(start.y() + (m_iSegment*sin(_deg2rad(angle))));

            start.setX(mx);
            start.setY(my);
#endif

            if (!m_bCalculating) {
                mPointPairs += currentPos;
                mPointPairs += start;
                currentPos = start;
#if 0
                m_dcr += m_dr;
                m_dcg += m_dg;
                m_dcb += m_db;

                pen.GetLogPen(&logpen);
                logpen.lopnColor = RGB((int)m_dcr, (int)m_dcg, (int)m_dcb);
#endif
            } else {
                m_iSegments++;
            }
        } else if (m_bCalculating) {
            continue;
        } else if (at == 'f' || at == 'G') {
#if 1
            start.setX(start.x() + (m_fSegment*cos(_deg2rad(angle))));
            start.setY(start.y() + (m_fSegment*sin(_deg2rad(angle))));
#else
            int mx = Round(start.x() + (m_iSegment*cos(_deg2rad(angle))));
            int my = Round(start.y() + (m_iSegment*sin(_deg2rad(angle))));

            start.setX(mx);
            start.setY(my);
#endif

            currentPos = start;
        } else if (at == '+') {
            number = (number) ? number : 1;
            angle = Increment(angle, m_fAngle * number);
            number = 0;
        } else if (at == '-') {
            number = (number) ? number : 1;
            angle = Decrement(angle, m_fAngle * number);
            number = 0;
        } else if (at == '[') {
            number = 0;
            if (!PushState(angle, start)) break;
        } else if (at == ']') {
            number = 0;
            PopState(angle, start);
            currentPos = start;
        }/* else if (at == '|') {
            int mx = Round(start.x + (m_iSegment*cos(_deg2rad(angle))));
            int my = Round(start.y + (m_iSegment*sin(_deg2rad(angle))));

            start.x = mx;
            start.y = my;

            pDC->LineTo(start);
        } */else if (at >= '0' && at <= '9') {
            number = number * 10 + (at - '0');
        }
    }
}

bool LSystem::PushState(qreal angle, const QPointF &pos)
{
    _lstate branch;
    branch.angle = angle;
    branch.pos = pos;

    m_sStack.push_back(branch);

    return true;
}

void LSystem::PopState(qreal &angle, QPointF &point)
{
    if (m_sStack.isEmpty()) return;

    _lstate state = m_sStack.last();
    angle = state.angle;
    point = state.pos;
    m_sStack.pop_back();
}

int LSystem::Round(double x)
{
    return (int)floor(x + 0.5);
}

qreal LSystem::Increment(qreal init, qreal inc)
{
    init += inc;

    if (init >= 360)
        return init - 360;

    return init;
}

qreal LSystem::Decrement(qreal init, qreal inc)
{
    init -= inc;

    if (init < 0) return 360 + init;

    return init;
}

void LSystem::CalculateInfo()
{
    m_iSegments = 0;
    m_bCalculating = true;

    DrawLSystems(m_pszAxiom, m_fInitAngle, 0);

    if (m_iSegments == 0) {
        //		AfxMessageBox("Nothing will be drawn");
    }

    //	GenerateGradient(m_iSegments);

    m_bCalculating = false;
}

void LSystem::InitRulesArray()
{
    for (int i=0; i<26; i++) { m_pszRules[i].clear(); }
}

/////

LSystemItem::LSystemItem(LSystem *lsystem, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mLSystem(lsystem)
{

}

QRectF LSystemItem::boundingRect() const
{
    return mLSystem->boundingRect();
}

void LSystemItem::paint(QPainter *painter,
                        const QStyleOptionGraphicsItem *,
                        QWidget *)
{
    painter->setPen(Qt::white);
    painter->drawLines(mLSystem->mPointPairs);

#if 0
    painter->setPen(Qt::blue);
    painter->drawRect(boundingRect());
#endif
}

void LSystemItem::LSystemChanged()
{
    prepareGeometryChange();
    update();
}

/////

WorldGenScene::WorldGenScene(WorldGenView *view) :
    QGraphicsScene(view),
    mView(view),
    mPartition(0,0,1000,1000,0,4)
{
    setBackgroundBrush(Qt::black);

    mImage = QImage(QLatin1String("C:\\Programming\\Tiled\\PZWorldEd\\newyork.png"));
    mImage.convertToFormat(QImage::Format_ARGB32);
    QGraphicsPixmapItem *item = new QGraphicsPixmapItem(QPixmap::fromImage(mImage));
    addItem(item);

    mRoadGroup = new QGraphicsItemGroup();
    addItem(mRoadGroup);

    mLSystem = new LSystem();
#if 0
    mLSystem->LoadFile(QLatin1String("C:\\Programming\\WorldGen\\Lse\\Examples\\fern-leaf.lse"));
    mLSystem->CalculateInfo();
    mLSystem->reset();
    setSceneRect(mLSystem->boundingRect());
#endif

    mLSystemItem = new LSystemItem(mLSystem);
    addItem(mLSystemItem);

    LoadFile(QLatin1String("C:\\Programming\\WorldGen\\Lse\\Examples\\fern-leaf.lse"));

    setSceneRect(item->boundingRect());
}

bool WorldGenScene::LoadFile(const QString &fileName)
{
    mLSystem->LoadFile(fileName);
    mLSystem->CalculateInfo();
    mLSystem->reset();

    mMaxDepth = mLSystem->m_iMaxDepth;

    mLSystemItem->LSystemChanged();
    setSceneRect(mLSystem->boundingRect());

    return true;
}

#include <QGraphicsSceneMouseEvent>

#define SEGMENT_LENGTH 4

void WorldGenScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF start = event->scenePos();
        qreal angle = 0; //qrand() % 360;

        mMaxRoadDepth = 4;
        addRoad(0, start, angle, SEGMENT_LENGTH);
        addRoad(0, start, LSystem::Increment(angle, 180), SEGMENT_LENGTH);
    } else {
        qDeleteAll(mRoadGroup->children());
        mIntersections.clear();
        mPartition.Clear();
    }
}

void WorldGenScene::depthIncr()
{
    if (mLSystem->m_iMaxDepth < mMaxDepth) {
        mLSystem->m_iMaxDepth++;
        mLSystem->CalculateInfo();
        mLSystem->reset();

        mLSystemItem->LSystemChanged();
//        setSceneRect(mLSystem->boundingRect());
    }
}

void WorldGenScene::depthDecr()
{
    if (mLSystem->m_iMaxDepth > 0) {
        mLSystem->m_iMaxDepth--;
        mLSystem->CalculateInfo();
        mLSystem->reset();

        mLSystemItem->LSystemChanged();
//        setSceneRect(mLSystem->boundingRect());
    }
}


Object::Object( float _x, float _y, float _width, float _height ) :
    x		( _x ),
    y		( _y ),
    width	( _width ),
    height	( _height )
{
}


Quadtree::Quadtree(float _x, float _y, float _width, float _height, int _level, int _maxLevel) :
    x		(_x),
    y		(_y),
    width	(_width),
    height	(_height),
    level	(_level),
    maxLevel(_maxLevel)
{
    if (level == maxLevel)
        return;

    NW = new Quadtree(x, y, width / 2.0f, height / 2.0f, level+1, maxLevel);
    NE = new Quadtree(x + width / 2.0f, y, width / 2.0f, height / 2.0f, level+1, maxLevel);
    SW = new Quadtree(x, y + height / 2.0f, width / 2.0f, height / 2.0f, level+1, maxLevel);
    SE = new Quadtree(x + width / 2.0f, y + height / 2.0f, width / 2.0f, height / 2.0f, level+1, maxLevel);
}

Quadtree::~Quadtree()
{
    if (level == maxLevel)
        return;

    delete NW;
    delete NE;
    delete SW;
    delete SE;
}

void Quadtree::AddObject(Object *object) {
    if (level == maxLevel) {
        objects.push_back(object);
        return;
    }

    if (contains(NW, object)) {
        NW->AddObject(object); return;
    } else if (contains(NE, object)) {
        NE->AddObject(object); return;
    } else if (contains(SW, object)) {
        SW->AddObject(object); return;
    } else if (contains(SE, object)) {
        SE->AddObject(object); return;
    }
    if (contains(this, object)) {
        objects.push_back(object);
    }
}

QVector<Object *> Quadtree::GetObjectsAt(float _x, float _y) {
    if (level == maxLevel)
        return objects;

    QVector<Object*> returnObjects, childReturnObjects;
    if (!objects.empty()) {
        returnObjects = objects;
    }
    if (_x > x + width / 2.0f && _x < x + width) {
        if (_y > y + height / 2.0f && _y < y + height) {
            childReturnObjects = SE->GetObjectsAt(_x, _y);
            returnObjects += childReturnObjects;
//            returnObjects.insert(returnObjects.end(), childReturnObjects.begin(), childReturnObjects.end());
            return returnObjects;
        } else if (_y > y && _y <= y + height / 2.0f) {
            childReturnObjects = NE->GetObjectsAt(_x, _y);
            returnObjects += childReturnObjects;
//            returnObjects.insert(returnObjects.end(), childReturnObjects.begin(), childReturnObjects.end());
            return returnObjects;
        }
    } else if (_x > x && _x <= x + width / 2.0f) {
        if (_y > y + height / 2.0f && _y < y + height) {
            childReturnObjects = SW->GetObjectsAt(_x, _y);
            returnObjects += childReturnObjects;
//            returnObjects.insert(returnObjects.end(), childReturnObjects.begin(), childReturnObjects.end());
            return returnObjects;
        } else if (_y > y && _y <= y + height / 2.0f) {
            childReturnObjects = NW->GetObjectsAt(_x, _y);
            returnObjects += childReturnObjects;
//            returnObjects.insert(returnObjects.end(), childReturnObjects.begin(), childReturnObjects.end());
            return returnObjects;
        }
    }

    return returnObjects;
}

QVector<Object *> Quadtree::GetObjectsAt(QRectF r)
{
    if (level == maxLevel)
        return objects;

    QVector<Object*> returnObjects, childReturnObjects;
    if (!objects.empty()) {
        returnObjects = objects;
    }
    if (r.intersects(QRectF(x,y,width/2,height/2)))
        return objects + NW->GetObjectsAt(r);
    if (r.intersects(QRectF(x+width/2,y,width-width/2,height/2)))
        return objects + NE->GetObjectsAt(r);
    if (r.intersects(QRectF(x,y+height/2,width/2,height-height/2)))
        return objects + SW->GetObjectsAt(r);
    if (r.intersects(QRectF(x+width/2,y+height/2,width-width/2,height-height/2)))
        return objects + SE->GetObjectsAt(r);

    return returnObjects;
}

void Quadtree::Clear() {
    if (level == maxLevel) {
        objects.clear();
        return;
    } else {
        NW->Clear();
        NE->Clear();
        SW->Clear();
        SE->Clear();
    }

    if (!objects.empty()) {
        objects.clear();
    }
}

bool Quadtree::contains(Quadtree *child, Object *object) {
    return QRectF(child->x, child->y, child->width, child->height)
            .intersects(QRectF(object->x,object->y,object->width,object->height));
#if 0
    return	 !(object->x < child->x ||
           object->y < child->y ||
           object->x > child->x + child->width  ||
           object->y > child->y + child->height ||
           object->x + object->width < child->x ||
           object->y + object->height < child->y ||
           object->x + object->width > child->x + child->width ||
           object->y + object->height > child->y + child->height);
#endif
}

WorldGenScene::Road WorldGenScene::tryRoadSegment(QPointF start, qreal angle, qreal length, bool recursing)
{
    qreal endX = start.x() + (length * cos(_deg2rad(angle)));
    qreal endY = start.y() + (length * sin(_deg2rad(angle)));
    QPointF end(endX, endY);

    // end point close to existing intersection? connect to it
    QRectF closeRect(end.x()-2,end.y()-2,4,4);
    QVector<Object*> closest = mPartition.GetObjectsAt(closeRect);
    foreach (Object *o, closest) {
        if ((QRectF(o->x,o->y,o->width,o->height).intersects(closeRect))) {
            // FIXME: adjust the angle to reflect reality
            new QGraphicsRectItem(QRectF(o->x-1,o->y-1,2,2), mRoadGroup);
            return Road(start, QPointF(o->x,o->y), angle, 1, true);
        }
    }

    // if any point along the new segment is close to another segment's start/end,
    // join it

    // segment crosses existing road? truncate and form intersection
    qreal epsilon = 0.01;
    foreach (QGraphicsItem *item, items(QRectF(start, end).normalized().adjusted(-1,-1,1,1))) {
        if (QGraphicsLineItem *li = dynamic_cast<QGraphicsLineItem*>(item)) {
            QLineF line1(start, end);
#if 1
            QLineF line2 = li->line();
            // Don't intersect with the line we're branching from
            if (start == line2.p1() || start == line2.p2()) continue;
#else
            /////
            // Stretch the existing segment a bit
            QPointF p1 = li->line().p1(), p2 = li->line().p2();
            QLineF v = li->line().unitVector();
            p2 = QPointF(p1.x() + v.dx() * (li->line().length() + epsilon),
                         p1.y() + v.dy() * (li->line().length() + epsilon));
            p1 = QPointF(p1.x() + v.dx() * -epsilon, p1.y() + v.dy() * -epsilon);
            QLineF line2(p1, p2);
            /////
            // Don't intersect with the line we're branching from
            if (start == li->line().p1() || start == li->line().p2()) continue;
#endif
            QPointF p;
            if (line1.intersect(line2, &p) == QLineF::BoundedIntersection) {
                mPartition.AddObject(new Object(p.x(),p.y(),0.001f,0.001f));
                // This road was going along and hit another.  Sometime we'd
                // like to continue on through or start another segment on the
                // other side of the intersection.
                return Road(start, p, angle, 1, true);

            }
        }
    }

    // segment ends near to an existing road? stretch out a maxiumum distance
    if (0)
    {
        qreal endX = start.x() + (length * 2 * cos(_deg2rad(angle)));
        qreal endY = start.y() + (length * 2 * sin(_deg2rad(angle)));
        foreach (QGraphicsItem *item, items(QRectF(end.x()-length,end.y()-length,length*2,length*2).normalized())) {
            if (QGraphicsLineItem *li = dynamic_cast<QGraphicsLineItem*>(item)) {
                QLineF line1(start, QPointF(endX,endY));
                QLineF line2(li->line());
                QPointF p;
                if (line1.intersect(line2, &p) == QLineF::BoundedIntersection) {
                    mPartition.AddObject(new Object(p.x(),p.y(),0.001f,0.001f));
                    return Road(start, p, angle, 1, true);
                }
            }
        }
    }


    if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
            && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
        return Road(start, end, angle, 1);
    }

    // pendulum out by a few degrees till we are in bounds / not in water
    if (recursing) return Road();
    for (int i = 5; i <= 90; i += 5) {
        int sign = (qrand() & 1) ? 1 : -1;

        // +/-
        qreal angle2 = (sign > 0) ? LSystem::Increment(angle, i)
                                  : LSystem::Decrement(angle, i);
        endX = start.x() + (length * cos(_deg2rad(angle2)));
        endY = start.y() + (length * sin(_deg2rad(angle2)));
        end = QPointF(endX, endY);

        if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
                && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
            return tryRoadSegment(start, angle2, length, true);
//            return Road(start, end, angle2, 1);
        }

        // -/+
        angle2 = (sign > 0) ? LSystem::Decrement(angle, i)
                            : LSystem::Increment(angle, i);
        endX = start.x() + (length * cos(_deg2rad(angle2)));
        endY = start.y() + (length * sin(_deg2rad(angle2)));
        end = QPointF(endX, endY);

        if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
                && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
            return tryRoadSegment(start, angle2, length, true);
//            return Road(start, end, angle2, 1);
        }
    }

    return Road();
}

#include <QCoreApplication>
void WorldGenScene::addRoad(int depth, QPointF start, qreal angle, qreal length)
{
    int branch = qMax(2, qrand() % 4) * 4;
    struct Branch {
        Branch(QPointF start, qreal angle) :
            start(start), angle(angle)
        {}
        QPointF start;
        qreal angle;
    };
    QList<Branch> branches;

    qreal width = 0.25; //(mMaxRoadDepth - depth);
    QPen pen(Qt::black, width);

    int segments = 20 + (qrand() % 40);
    for (int i = 0; i < segments; i++) {
        Road road = tryRoadSegment(start, angle, length);
        if (road.width == 0 || QLineF(road.start, road.end).length() < 1) break;
        QGraphicsLineItem *item = new QGraphicsLineItem(QLineF(road.start, road.end));
        item->setPen(pen);
        item->setGroup(mRoadGroup);
//        addItem(item);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

        if (road.terminal) break;

        start = road.end;
#if 1 // curve the road
        if (qrand() & 1)
            angle = LSystem::Increment(road.angle, qrand() % 10);
        else
            angle = LSystem::Decrement(road.angle, qrand() % 10);
#endif
        // branch
        if ((depth < mMaxRoadDepth) && !(--branch)) {
//            mIntersections += road.end; // remove this
            int sign = (qrand() % 2) ? 1 : -1;
            branches += Branch(road.end, (qrand() % 2) ? LSystem::Increment(angle, 90) : LSystem::Decrement(angle, 90));
            branch = qMax(2, qrand() % 8);
        }
    }

    foreach (Branch b, branches)
        addRoad(depth + 1, b.start, b.angle, length);

}

/////

WorldGenView::WorldGenView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this))
{
    mScene = new WorldGenScene(this);
    setScene(mScene);
//    setTransform(QTransform::fromScale(1.75,1.75));

    mZoomable->setZoomFactors(QVector<qreal>() << 0.25 << 0.33 << 0.5 << 0.75 << 1.0 << 1.5 << 2.0 << 3.0 << 4.0 << 6.0 << 10.0);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

void WorldGenView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    mLastMouseGlobalPos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMouseGlobalPos));
}

void WorldGenView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}

bool WorldGenView::LoadFile(const QString &fileName)
{
    return mScene->LoadFile(fileName);
}

void WorldGenView::wheelEvent(QWheelEvent *event)
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
