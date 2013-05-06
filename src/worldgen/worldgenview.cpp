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

#include <QFile>
#include <QTextStream>

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
    mView(view)
{
    setBackgroundBrush(Qt::black);

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

/////

WorldGenView::WorldGenView(QWidget *parent) :
    QGraphicsView(parent)
{
    mScene = new WorldGenScene(this);
    setScene(mScene);
}

bool WorldGenView::LoadFile(const QString &fileName)
{
    return mScene->LoadFile(fileName);
}
