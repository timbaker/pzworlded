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

namespace WorldGen {

class NYGridItem : public QGraphicsItem
{
public:
    NYGridItem(int x, int y, int w, int h, int gw, int gh) :
        QGraphicsItem(),
        x(x), y(y), w(w), h(h), gw(gw), gh(gh)
    {
        setTransformOriginPoint(x, y);
    }

    QRectF boundingRect() const
    {
        return QRectF(x, y, w * gw, h * gh);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
    {
        QPen pen(QColor(128,128,128,128), 1);
        painter->setPen(pen);

        for (int y = 0; y <= h * gh; y += gh)
            painter->drawLine(x, this->y + y, x + w * gw, this->y + y);
        for (int x = 0; x <= w * gw; x += gw)
            painter->drawLine(this->x + x, this->y, this->x + x, this->y + h * gh);
    }

    QPointF nearestGridPoint(const QPointF &scenePos, bool local = false)
    {
        QPointF localP = mapFromScene(scenePos);
        int x = localP.x() - this->x;
        int y = localP.y() - this->y;
        x = (x + gw / 2) / gw;
        y = (y + gh / 2) / gh;
        QPointF gridPt(this->x + x * gw, this->y + y * gh);
        return local ? gridPt : mapToScene(gridPt);
    }

    bool containsScenePt(const QPointF &pt)
    {
        return QRectF(x, y, w * gw, h * gh).contains(mapFromScene(pt));
    }

    int x, y, w, h, gw, gh;
};

} // namespace WorldGen

/////

#define GRID_WIDTH (35/3)
#define GRID_HEIGHT (20/3)

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

    addItem(mGridItem = new NYGridItem(mImage.width() / 2, 66,
                                       (mImage.width() / 2 + GRID_WIDTH - 1) / GRID_WIDTH,
                                       (mImage.height() + GRID_HEIGHT - 1) / GRID_HEIGHT,
                                       GRID_WIDTH, GRID_HEIGHT));
    mGridItem->setRotation(5);
    addItem(mGridItem2 = new NYGridItem(150, 100,
                                        15,
                                        10,
                                        GRID_WIDTH, GRID_HEIGHT));
    mGridItem2->setRotation(-5);


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

bool pointsClose(const QPointF &p1, const QPointF &p2)
{
    qreal epsilon = 0.1;
    return QLineF(p1,p2).length() < epsilon;
}

WorldGenScene::Road WorldGenScene::tryRoadSegment(QPointF start, qreal angle, qreal length, bool recursing)
{    
    QLineF line(start, start + QPointF(1,0)); line.setLength(length); line.setAngle(angle);
    QPointF end = line.p2();
    angle = line.angle();

#if 1
    // globalGoals: align to new-york-style grid
    foreach (NYGridItem *gi, QList<NYGridItem*>() << mGridItem << mGridItem2) {
        if (gi->containsScenePt(end)) {
            QPointF gridPt = gi->nearestGridPoint(end, true); // in grid coords
            QPointF gridPt2(gridPt.x() + gi->gw, gridPt.y()); // in grid coords
            QLineF gridHLine(gridPt, gridPt2); // horizontal line to align to
            QLineF roadInGridCoords(gi->mapFromScene(start), gi->mapFromScene(end));
            // Align horizontal-ish roads with the nearest horizontal grid line
            qreal localAngle = roadInGridCoords.angleTo(gridHLine);
            if ((localAngle < 45 || localAngle >= 315) || (localAngle >= 135 && localAngle < 225)) {
                qreal d = (gridPt.y() - roadInGridCoords.p2().y()) / 4;
                if (d < 0 && d > -1) d = -qMin(1.0, roadInGridCoords.p2().y() - gridPt.y());
                else if (d > 0 && d < 1) d = qMin(1.0, gridPt.y() - roadInGridCoords.p2().y());
                end.setY(gi->mapToScene(roadInGridCoords.p2().x(),
                                        roadInGridCoords.p2().y() + d).y());

                // Align vertical-ish roads with the nearest vertical grid line
            } else {
                qreal d = (gridPt.x() - roadInGridCoords.p2().x()) / 4;
                if (d < 0 && d > -1) d = -qMin(1.0,roadInGridCoords.p2().x() - gridPt.x());
                else if (d > 0 && d < 1) d = qMin(1.0, gridPt.x() - roadInGridCoords.p2().x());
                end.setX(gi->mapToScene(roadInGridCoords.p2().x() + d,
                                        roadInGridCoords.p2().y()).x());
            }
            break;
        }
    }
#endif

#if 0
    qreal endX = start.x() + (length * cos(_deg2rad(angle)));
    qreal endY = start.y() + (length * sin(_deg2rad(angle)));
    QPointF end(endX, endY);
#endif

    // end point close to existing intersection? connect to it
    QRectF closeRect(end.x()-2,end.y()-2,4,4);
    QVector<Object*> closest = mPartition.GetObjectsAt(closeRect);
    foreach (Object *o, closest) {
        if ((QRectF(o->x,o->y,o->width,o->height).intersects(closeRect))) {
            new QGraphicsRectItem(QRectF(o->x-1,o->y-1,2,2), mRoadGroup);
            return Road(start, QPointF(o->x,o->y), 1, true);
        }
    }

    // if any point along the new segment is close to another segment's start/end,
    // join it

    // segment crosses existing road? truncate and form intersection
    qreal epsilon = 0.5;
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
                li->setPen(QColor(Qt::green));
                return Road(start, p, 1, true);

            }
        }
    }

    foreach (QGraphicsItem *item, items(QRectF(start, end).normalized().adjusted(-1,-1,1,1))) {
        if (QGraphicsLineItem *li = dynamic_cast<QGraphicsLineItem*>(item)) {
            QLineF line1(start, end);
            QLineF line2 = li->line();
            // Don't intersect with the line we're branching from
            if (start == line2.p1() || start == line2.p2()) continue;

            // Join roads that hit end-on-end
            QLineF norm(end,start); norm = norm.normalVector(); norm.setLength(epsilon);
            QLineF normRev(norm); normRev.setLength(-epsilon);
            QLineF line3(norm.p2(),normRev.p2());
            QPointF p;
            if (line2.intersect(line3, &p) == QLineF::BoundedIntersection) {
                QGraphicsRectItem *ri = new QGraphicsRectItem(QRectF(p.x()-epsilon,p.y()-epsilon,epsilon*2,epsilon*2), mRoadGroup);
                ri->setBrush(QColor(128,128,128,128));
                li->setPen(QColor(Qt::red));
                return Road(start, p, 1, true);
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
                    return Road(start, p, 1, true);
                }
            }
        }
    }


    if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
            && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
        return Road(start, end, 1);
    }

    // pendulum out by a few degrees till we are in bounds / not in water
    if (recursing) return Road();
    for (int i = 5; i <= 90; i += 5) {
        int sign = (qrand() & 1) ? 1 : -1;

        // +/-
#if 1
        line.setAngle((sign > 0) ? angle + i : angle - i);
        end = line.p2();
#else
        qreal angle2 = (sign > 0) ? LSystem::Increment(angle, i)
                                  : LSystem::Decrement(angle, i);
        endX = start.x() + (length * cos(_deg2rad(angle2)));
        endY = start.y() + (length * sin(_deg2rad(angle2)));
        end = QPointF(endX, endY);
#endif
        if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
                && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
            return tryRoadSegment(start, line.angle(), length, true);
//            return Road(start, end, angle2, 1);
        }

        // -/+
#if 1
        line.setAngle((sign > 0) ? angle - i : angle + i);
        end = line.p2();
#else
        angle2 = (sign > 0) ? LSystem::Decrement(angle, i)
                            : LSystem::Increment(angle, i);
        endX = start.x() + (length * cos(_deg2rad(angle2)));
        endY = start.y() + (length * sin(_deg2rad(angle2)));
        end = QPointF(endX, endY);
#endif

        if (QRect(QPoint(),mImage.size()).contains(end.toPoint())
                && mImage.pixel(end.toPoint()) == qRgb(255,255,255)) {
            return tryRoadSegment(start, line.angle(), length, true);
//            return Road(start, end, angle2, 1);
        }
    }

    return Road();
}

#include <QCoreApplication>
void WorldGenScene::addRoad(int depth, QPointF start, qreal angle, qreal length)
{
    int branch = qMax(2, qrand() % 6);
    struct Branch {
        Branch(QPointF start, qreal angle) :
            start(start), angle(angle)
        {}
        QPointF start;
        qreal angle;
    };
    QList<Branch> branches;

    qreal width = 0.25; //(mMaxRoadDepth - depth);
    QPen pen1(Qt::black, width);
    QPen pen2(Qt::black, 3);

    QList<QGraphicsLineItem*> items;

    int segments = (20 + (qrand() % 40));
    for (int i = 0; i < segments; i++) {
        Road road = tryRoadSegment(start, angle, length);
        if (road.width == 0 || road.line.length() < 1) break;

        QGraphicsLineItem *item = new QGraphicsLineItem(road.line);
        item->setPen(pen2);
        item->setGroup(mRoadGroup);
//        addItem(item);
        items += item;
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

        if (road.terminal) break;

        start = road.line.p2();
        angle = road.line.angle();
#if 1 // curve the road
        if (!mGridItem->containsScenePt(start) && !mGridItem2->containsScenePt(start)) {
            if (qrand() & 1)
                angle = LSystem::Increment(angle, qrand() % 10);
            else
                angle = LSystem::Decrement(angle, qrand() % 10);
        }
#endif
        // branch every so often or at the end of roads
        if ((depth < mMaxRoadDepth) && (!(--branch) || (i == segments-1))) {
#if 0
            // NY grid: branch only at grid points
            bool insideGridZone = true;
            if (insideGridZone) {
                QPointF end = road.line.p2();
                QPointF gridPt = end;
                gridPt.setX(int((gridPt.x() + GRID_WIDTH/2)/GRID_WIDTH)*GRID_WIDTH);
                gridPt.setY(int((gridPt.y() + GRID_HEIGHT/2)/GRID_HEIGHT)*GRID_HEIGHT);
            }
#endif
            int sign = (qrand() % 2) ? 1 : -1;
            branches += Branch(road.line.p2(), (sign == 1) ? LSystem::Increment(angle, 90)
                                                           : LSystem::Decrement(angle, 90));
            // sometimes branch in both directions
            if (qrand() % 3) {
                branches += Branch(road.line.p2(), (sign == 1) ? LSystem::Decrement(angle, 90)
                                                               : LSystem::Increment(angle, 90));
            }
            branch = qMax(2, qrand() % 6);
        }
    }

    foreach (QGraphicsLineItem *item, items)
        item->setPen(pen1);

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

    viewport()->setMouseTracking(true);

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
                  mZoomable->smoothTransform() && false);
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
