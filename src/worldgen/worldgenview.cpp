#include "worldgenview.h"

using namespace WorldGen;

/////

/////
LSystem::LSystem() :
    m_fAngle(90),
    m_fInitAngle(0),
    m_fStepSize(0.65f),
    m_iSegment(6),
    m_iMaxDepth(4),
    m_pszName(QString()),
    m_iSP(0)
{
    InitRulesArray();

#if 1 // fern_leaf.lse
    m_pszName = QLatin1String("Fern Leaf #1");
    m_fAngle = 8;
    m_fInitAngle = 278;
    m_fStepSize = 0.5;
    m_iSegment = 100;
    m_iMaxDepth = 4;
    m_pszAxiom = QLatin1String("F");
    m_pszRules['F'-'A'] = QLatin1String("|[5+F][7-F]-|[4+F][6-F]-|[3+F][5-F]-|F");
#endif
}

QRectF LSystem::boundingRect()
{
    if (mBounds.isEmpty() && mPointPairs.size()) {
        QRectF r(mPointPairs.first(), mPointPairs.first());
        foreach (QPointF p, mPointPairs) {
            r.setX(qMin(r.x(), p.x()));
            r.setY(qMin(r.y(), p.y()));
            r.setRight(qMax(r.x(), p.x()));
            r.setBottom(qMax(r.y(), p.y()));
        }
        mBounds = r;
    }
    return mBounds;
}

#define _deg2rad(a) (a * 3.141592653589793238462643) / 180.0

void LSystem::DrawLSystems(QString lstring, qreal angle, int depth)
{
    char at;									// Cache the character
    int number = 0;								// Number support
    int	len = lstring.length();				// Cache the length
    int old;									// Old segment length

    QPointF start = currentPos;

    for (int i=0;i<len;i++) {
        at = lstring[i].toLatin1();

        if (depth < m_iMaxDepth && (at >= 'A' && at <= 'Z')) {
            old = m_iSegment;
            m_iSegment = int(m_iSegment * m_fStepSize);
            if (m_iSegment % 2 != 0) m_iSegment--;
            DrawLSystems(Rule(at), angle, depth+1);
            m_iSegment = old;
        } else if (at == 'F' || at == '|') {
            int mx = Round(start.x() + (m_iSegment*cos(_deg2rad(angle))));
            int my = Round(start.y() + (m_iSegment*sin(_deg2rad(angle))));

            start.setX(mx);
            start.setY(my);

            if (!m_bCalculating) {
                mPointPairs += currentPos;
                mPointPairs += start;
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
            int mx = Round(start.x() + (m_iSegment*cos(_deg2rad(angle))));
            int my = Round(start.y() + (m_iSegment*sin(_deg2rad(angle))));

            start.setX(mx);
            start.setY(my);

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
    if (m_iSP++ > LSYS_STACK_SIZE) return false;

    _lstate branch;
    branch.angle = angle;
    branch.pos = pos;

    m_sStack[m_iSP] = branch;

    return true;
}

void LSystem::PopState(qreal &angle, QPointF &point)
{
    if (m_iSP == -1) return;

    _lstate state = m_sStack[m_iSP--];
    angle = state.angle;
    point = state.pos;
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

    painter->drawRect(boundingRect());
}

/////

WorldGenScene::WorldGenScene(WorldGenView *view) :
    QGraphicsScene(view),
    mView(view)
{
    setBackgroundBrush(Qt::black);

    mLSystem = new LSystem();
    mLSystem->CalculateInfo();
    mLSystem->m_iSP = 0;
    mLSystem->currentPos = QPoint();
    mLSystem->DrawLSystems(mLSystem->m_pszAxiom, mLSystem->m_fInitAngle, 0);
    setSceneRect(mLSystem->boundingRect());

    mLSystemItem = new LSystemItem(mLSystem);
    addItem(mLSystemItem);
}

/////

WorldGenView::WorldGenView(QWidget *parent) :
    QGraphicsView(parent)
{
    mScene = new WorldGenScene(this);
    setScene(mScene);
}
