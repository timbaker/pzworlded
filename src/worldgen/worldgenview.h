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

class Zoomable;

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
    static qreal Increment(qreal init, qreal inc);
    static qreal Decrement(qreal init, qreal inc);

    qreal m_fAngle;
    qreal m_fStepSize;
    qreal m_fInitAngle;
    qreal m_fSegment;
    int m_iMaxDepth;
    int m_iSegments;

    bool m_bCalculating;

    QString m_pszName;

    QVector<_lstate> m_sStack;

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

class Object {
public:
    float					x;
    float					y;
    float					width;
    float					height;

                            Object( float x, float y, float width, float height );
};

class Quadtree {
public:
                        Quadtree(float x, float y, float width, float height, int level, int maxLevel);

                        ~Quadtree();

    void					AddObject(Object *object);
    QVector<Object*>				GetObjectsAt(float x, float y);
    QVector<Object*> GetObjectsAt(QRectF r);
    void					Clear();

private:
    float					x;
    float					y;
    float					width;
    float					height;
    int					level;
    int					maxLevel;
    QVector<Object*>				objects;

    Quadtree *				parent;
    Quadtree *				NW;
    Quadtree *				NE;
    Quadtree *				SW;
    Quadtree *				SE;

    bool					contains(Quadtree *child, Object *object);
};

class NYGridItem;

class WorldGenScene : public QGraphicsScene
{
    Q_OBJECT
public:
    WorldGenScene(WorldGenView *view);

    bool LoadFile(const QString &fileName);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void depthIncr();
    void depthDecr();

    void showGrids(bool show);

private:
    class Road
    {
    public:
        Road(QPointF start, QPointF end, qreal width, bool terminal = false) :
            line(start, end), width(width), terminal(terminal)
        {}
        Road() : width(0) {}

        QLineF line;
        qreal width;
        bool terminal;
    };

    Road tryRoadSegment(QPointF start, qreal angle, qreal length, bool recurse = false);
    void addRoad(int depth, QPointF start, qreal angle, qreal length);
    int mMaxRoadDepth;
    QGraphicsItemGroup *mRoadGroup;
    QList<QPointF> mIntersections;
    Quadtree mPartition;

private:
    WorldGenView *mView;
    LSystem *mLSystem;
    int mMaxDepth;
    LSystemItem *mLSystemItem;
    QImage mImage;
    NYGridItem *mGridItem;
    NYGridItem *mGridItem2;

    struct Branch {
        Branch(QPointF start, qreal angle) :
            start(start), angle(angle)
        {}
        QPointF start;
        qreal angle;
    };
    QMap<int,QList<Branch> > mBranchByDepth;
};

class WorldGenView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit WorldGenView(QWidget *parent = 0);
    
    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    bool LoadFile(const QString &fileName);

signals:
    
public slots:
    void adjustScale(qreal scale);

private:
    WorldGenScene *mScene;

    QPoint mLastMouseGlobalPos;
    QPointF mLastMouseScenePos;
    Zoomable *mZoomable;
};

} // namespace WorldGen

#endif // WORLDGENVIEW_H
