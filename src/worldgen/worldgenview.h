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

#ifndef WORLDGENVIEW_H
#define WORLDGENVIEW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace WorldGen {

class WorldGenView;

class LSystem
{
public:
    LSystem();
    QRectF boundingRect();
    void reset();
    bool LoadFile(const QString &fileName);

    QPointF currentPos;
    QVector<QPointF> mPointPairs;
    QRectF mBounds;

    /////

    typedef struct {
        qreal angle;
        QPointF pos;
    } _lstate;

    void DrawLSystems(QString lstring, qreal angle, int depth);

    bool PushState(qreal angle, const QPointF &pos);
    void PopState(qreal &angle, QPointF &point);

    QString Rule(char c) { return m_pszRules[c - 'A']; }

    inline int Round(double x);
    inline qreal Increment(qreal init, qreal inc);
    inline qreal Decrement(qreal init, qreal inc);

    qreal m_fAngle;
    qreal m_fStepSize;
    qreal m_fInitAngle;
    int m_iSegment;
    int m_iMaxDepth;
    int m_iSP;
    int m_iSegments;

    bool m_bCalculating;

    QString m_pszName;

    enum { LSYS_STACK_SIZE = 100 };
    _lstate	m_sStack[LSYS_STACK_SIZE];

    QPoint m_cStartPoint;
    QString m_pszAxiom;
    QString m_pszRules[26];

    /////

    void CalculateInfo();
    void InitRulesArray();
};

class LSystemItem : public QGraphicsItem
{
public:
    LSystemItem(LSystem *lsystem, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void LSystemChanged();

    LSystem *mLSystem;
};

class WorldGenScene : public QGraphicsScene
{
    Q_OBJECT
public:
    WorldGenScene(WorldGenView *view);

    bool LoadFile(const QString &fileName);

public slots:
    void depthIncr();
    void depthDecr();

private:
    WorldGenView *mView;
    LSystem *mLSystem;
    LSystemItem *mLSystemItem;
};

class WorldGenView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit WorldGenView(QWidget *parent = 0);
    
    bool LoadFile(const QString &fileName);

signals:
    
public slots:
    
private:
    WorldGenScene *mScene;
};

} // namespace WorldGen

#endif // WORLDGENVIEW_H
