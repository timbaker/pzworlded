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

#include "path.h"

#ifdef Q_OS_WIN
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

/////

WorldNode::WorldNode() :
    layer(0),
    id(InvalidId)
{
}

WorldNode::WorldNode(id_t id, qreal x, qreal y) :
    layer(0),
    id(id),
    p(x, y)
{
}

WorldNode::WorldNode(id_t id, const QPointF &p) :
    layer(0),
    id(id),
    p(p)
{
}

WorldNode::~WorldNode()
{
}

WorldNode *WorldNode::clone() const
{
    WorldNode *clone = new WorldNode(id, p.x(), p.y());
    return clone;
}

/////

WorldPath::WorldPath() :
    id(InvalidId),
    mOwner(0)
{
}

WorldPath::WorldPath(id_t id) :
    id(id),
    mOwner(0)
{
}

WorldPath::~WorldPath()
{
}

void WorldPath::insertNode(int index, WorldNode *node)
{
    mPolygon.clear();
    mBounds = QRectF();
    nodes.insert(index, node);
}

WorldNode *WorldPath::removeNode(int index)
{
    mPolygon.clear();
    mBounds = QRectF();
    return nodes.takeAt(index);
}

bool WorldPath::isClosed() const
{
    return (nodes.size() > 1) && (nodes.first() == nodes.last());
}

QRectF WorldPath::bounds()
{
    if (mBounds.isNull()) {
        mBounds = polygon().boundingRect();
    }
    return mBounds;
}

QPolygonF WorldPath::polygon()
{
    mPolygon.resize(0);
    foreach (WorldNode *node, nodes)
        mPolygon += node->p;
    return mPolygon;


    if (mPolygon.isEmpty()) {
        foreach (WorldNode *node, nodes)
            mPolygon += node->p;
    }
    return mPolygon;
}

QRegion WorldPath::region()
{
    if (!isClosed() || nodes.size() < 3)
        return QRegion();

    return polygonRegion(polygon());
}

WorldPath *WorldPath::clone(WorldPathLayer *owner) const
{
    WorldPath *clone = new WorldPath(id);
    foreach (WorldNode *node, nodes)
        if (WorldNode *cloneNode = owner->node(node->id))
            clone->nodes += cloneNode;
    clone->tags = tags;
    return clone;
}

/////

WorldPathLayer::WorldPathLayer() :
    mLevel(0)
{
}

WorldPathLayer::WorldPathLayer(const QString &name, int level) :
    mName(name),
    mLevel(level)
{
}

WorldPathLayer::~WorldPathLayer()
{
    qDeleteAll(mPaths);
    qDeleteAll(mNodes);
}

void WorldPathLayer::insertNode(int index, WorldNode *node)
{
    mNodes.insert(index, node);
    mNodeByID[node->id] = node;
    node->layer = this;
}

WorldNode *WorldPathLayer::removeNode(int index)
{
    WorldNode *node = mNodes.takeAt(index);
    mNodeByID.remove(node->id);
    node->layer = 0;
    return node;
}

WorldNode *WorldPathLayer::node(id_t id)
{
    if (mNodeByID.contains(id))
        return mNodeByID[id];
    return 0;
}

void WorldPathLayer::insertPath(int index, WorldPath *path)
{
    Q_ASSERT(path->owner() == 0);
    path->setOwner(this);
    mPaths.insert(index, path);
    mPathByID[path->id] = path;
}

WorldPath *WorldPathLayer::removePath(int index)
{
    WorldPath *path = mPaths.takeAt(index);
    mPathByID.remove(path->id);
    Q_ASSERT(path->owner() == this);
    path->setOwner(0);
    return path;
}

WorldPath *WorldPathLayer::path(id_t id)
{
    if (mPathByID.contains(id))
        return mPathByID[id];
    return 0;
}

WorldPathLayer *WorldPathLayer::clone() const
{
    WorldPathLayer *clone = new WorldPathLayer(mName, mLevel);
    foreach (WorldNode *node, nodes())
        clone->insertNode(clone->nodeCount(), node->clone());
    foreach (WorldPath *path, paths())
        clone->insertPath(clone->pathCount(), path->clone(clone));
    return clone;
}

#if 0
QList<WorldPath *> WorldPathLayer::paths(QRectF &bounds)
{
    return mPaths;
}

QList<WorldPath*> WorldPathLayer::paths(qreal x, qreal y, qreal width, qreal height)
{
    return mPaths; // TODO: spatial partitioning
}
#endif


#if 1
#include <qmath.h>
class PathStroke
{
public:
    struct v2_t
    {
        v2_t()
        {
        }
        v2_t(qreal x, qreal y) :
            x(x),
            y(y)
        {
        }
        v2_t(QPointF &p) :
            x(p.x()),
            y(p.y())
        {
        }

        v2_t operator+=(const v2_t &other)
        {
            return v2_t(x + other.x, y + other.y);
        }

        qreal x, y;
    };

    struct strokeseg_t
    {
        strokeseg_t()
            : joinStart(-1)
            , joinEnd(-1)
        {
        }
        v2_t start, end; // Start- and end-points of the line segment
        qreal len; // Length of the segment
        int joinStart; // Index into strokeseg_t vector
        int joinEnd; // Index into strokeseg_t vector
    };

    enum VGPathSegment {
        VG_CLOSE_PATH,
        VG_MOVE_TO,
        VG_LINE_TO
    };

#ifndef VG_MAX_ENUM
#define VG_MAX_ENUM 0x7FFFFFFF
#endif

    typedef enum {
      VG_CAP_BUTT                                 = 0x1700,
      VG_CAP_ROUND                                = 0x1701,
      VG_CAP_SQUARE                               = 0x1702,

      VG_CAP_STYLE_FORCE_SIZE                     = VG_MAX_ENUM
    } VGCapStyle;

    typedef enum {
      VG_JOIN_MITER                               = 0x1800,
      VG_JOIN_ROUND                               = 0x1801,
      VG_JOIN_BEVEL                               = 0x1802,

      VG_JOIN_STYLE_FORCE_SIZE                    = VG_MAX_ENUM
    } VGJoinStyle;

    void build(WorldPath *path, qreal stroke_width, QVector<v2_t> &outlineFwd, QVector<v2_t> &outlineBkwd)
    {
        _strokeVertices.clear();

        QVector<v2_t> _fcoords;
        foreach (QPointF p, path->polygon()) {
            _fcoords += v2_t(p);
        }
//        if (path->isClosed())
//            _fcoords += _fcoords.first();
        if (path->isClosed()) _fcoords.remove(_fcoords.size() - 1);

        int coordsIter = 0;
        v2_t coords(0, 0);
        v2_t prev(0, 0);
        v2_t closeTo(0, 0);

        QVector<strokeseg_t> segments;
        int firstSeg = -1, prevSeg = -1;

        QVector<quint8> _segments;
        _segments += VG_MOVE_TO;
        for (int i = 1; i < _fcoords.size(); i++)
            _segments += VG_LINE_TO;
        if (path->isClosed())
            _segments += VG_CLOSE_PATH;

        foreach (quint8 segment, _segments) {
//            int numCoords = segmentToNumCoordinates( static_cast<VGPathSegment>( segment ) );
            bool isRelative = false;
            switch (segment) {
            case VG_CLOSE_PATH: {
                prevSeg = addStrokeSegment(segments, coords, closeTo, prevSeg);
                if ( prevSeg == -1 && firstSeg != -1 ) // failed to add zero-length segment
                    prevSeg = segments.size() - 1;
                if ( prevSeg != -1 ) {
                    segments[prevSeg].joinEnd = firstSeg;
                    segments[firstSeg].joinStart = prevSeg;
                }
                prevSeg = firstSeg = -1;
                coords = closeTo;
                break;
            }
            case VG_MOVE_TO: {
                v2_t c = _fcoords[coordsIter++];
                if (isRelative) {
                    c += coords;
                }
                prev = closeTo = coords = c;
                prevSeg = firstSeg = -1;
                break;
            }
            case VG_LINE_TO: {
                prev = coords;
                coords =  _fcoords[coordsIter++];
                if (isRelative)
                    coords += prev;
                prevSeg = addStrokeSegment(segments, prev, coords, prevSeg);
                if (firstSeg == -1)
                    firstSeg = prevSeg;
                break;
            }
            }
        }

        VGCapStyle capStyle = VG_CAP_BUTT;
        VGJoinStyle joinStyle = VG_JOIN_MITER;

        qreal strokeRadius = stroke_width / 2;

        firstSeg = 0;
        while (firstSeg < segments.size()) {
            unsigned int lastSeg = firstSeg;
//            bool closed = segments[firstSeg].joinStart != -1;

            outlineFwd.clear();

            // Generate stroke outline going forward for one contour.
            for (int i = firstSeg; i < segments.size(); i++) {
                strokeseg_t &seg = segments[i];
                if ( seg.joinStart == -1 ) {
                    // add cap verts
                    calc_cap( outlineFwd, seg.start, seg.end, seg.len, capStyle, stroke_width / 2 );
                }
                if ( seg.joinEnd == -1 ) {
                    // add cap verts
                    calc_cap( outlineFwd, seg.end, seg.start, seg.len, capStyle, stroke_width / 2 );
                } else {
                    // add join verts
                    joinStrokeSegments( outlineFwd, seg, segments[seg.joinEnd], false, strokeRadius, joinStyle );
                }
                lastSeg = i;

                // Reached the end of this contour.
                if ( seg.joinEnd == -1 || seg.joinEnd < i )
                    break;
            }

            outlineBkwd.clear();

            // Generate stroke outline going backward for the contour.
            for ( int i = lastSeg; i >= firstSeg; i-- ) {
                strokeseg_t& seg = segments[i];
                if ( seg.joinStart == -1 ) {
                } else {
                    joinStrokeSegments( outlineBkwd, seg, segments[seg.joinStart], true, strokeRadius, joinStyle );
                }
            }

            firstSeg = lastSeg + 1;
        }
    }

    int addStrokeSegment( QVector<strokeseg_t>& segments, const v2_t& p0, const v2_t& p1, int prev ) {

        if ( (p0.x == p1.x) && (p0.y == p1.y) ) {
            return -1;
        }

        segments.push_back( strokeseg_t() );
        strokeseg_t& seg = segments.back();
        seg.start = p0;
        seg.end = p1;
        seg.len = calc_distance( p0.x, p0.y, p1.x, p1.y );

        seg.joinStart = prev;
        if ( prev != -1 )
            segments[prev].joinEnd = segments.size() - 1;

        return segments.size() - 1;
    }

    static qreal calc_distance( qreal x1, qreal y1, qreal x2, qreal y2 ) {
        qreal dx = x2-x1;
        qreal dy = y2-y1;
        return sqrt(dx * dx + dy * dy);
    }

    bool calc_intersection( qreal ax, qreal ay, qreal bx, qreal by,
                            qreal cx, qreal cy, qreal dx, qreal dy,
                            qreal* x, qreal* y ) {
        qreal num = (ay-cy) * (dx-cx) - (ax-cx) * (dy-cy);
        qreal den = (bx-ax) * (dy-cy) - (by-ay) * (dx-cx);
        if (fabs(den) < 1.0e-7) return false;
        qreal r = num / den;
        *x = ax + r * (bx-ax);
        *y = ay + r * (by-ay);
        return true;
    }

    void calc_arc(QVector<v2_t>& verts,
                  qreal x, qreal y,
                  qreal dx1, qreal dy1,
                  qreal dx2, qreal dy2,
                  qreal width,
                  qreal approximation_scale) {
        Q_UNUSED(verts)
        Q_UNUSED(x)
        Q_UNUSED(y)
        Q_UNUSED(dx1)
        Q_UNUSED(dy1)
        Q_UNUSED(dx2)
        Q_UNUSED(dy2)
        Q_UNUSED(width)
        Q_UNUSED(approximation_scale)
    }

    void calc_miter( QVector<v2_t>& verts, v2_t v0, v2_t v1, v2_t v2, qreal dx1, qreal dy1, qreal dx2, qreal dy2, qreal strokeRadius, VGJoinStyle joinStyle, qreal miter_limit ) {

        qreal xi = v1.x;
        qreal yi = v1.y;
        bool miter_limit_exceeded = true; // Assume the worst
        v2_t result(0, 0);

        if(calc_intersection(v0.x + dx1, v0.y - dy1,
                             v1.x + dx1, v1.y - dy1,
                             v1.x + dx2, v1.y - dy2,
                             v2.x + dx2, v2.y - dy2,
                             &xi, &yi))
        {
#if 0
            qreal d1 = calc_distance(v1.x, v1.y, xi, yi);
            qreal lim = strokeRadius * miter_limit;
            if (d1 <= lim)
#endif
            {
                // Inside the miter limit
                //---------------------
                result.x = xi, result.y = yi;
                miter_limit_exceeded = false;
            }
        }
        else
        {
            // Calculation of the intersection failed, most probably
            // the three points lie one straight line.
            // First check if v0 and v2 lie on the opposite sides of vector:
            // (v1.x, v1.y) -> (v1.x+dx1, v1.y-dy1), that is, the perpendicular
            // to the line determined by vertices v0 and v1.
            // This condition determines whether the next line segments continues
            // the previous one or goes back.
            //----------------
            qreal x2 = v1.x + dx1;
            qreal y2 = v1.y - dy1;
            if(((x2 - v0.x)*dy1 - (v0.y - y2)*dx1 < 0.0) !=
                    ((x2 - v2.x)*dy1 - (v2.y - y2)*dx1 < 0.0))
            {
                // This case means that the next segment continues
                // the previous one (straight line)
                //-----------------
                result.x = v1.x + dx1, result.y = v1.y - dy1;
                miter_limit_exceeded = false;
            }
        }

        v2_t vert;
        switch ( joinStyle ) {
        case VG_JOIN_BEVEL:
            vert.x = v1.x + dx1; vert.y = v1.y - dy1; verts.push_back( vert );
            vert.x = v1.x + dx2; vert.y = v1.y - dy2; verts.push_back( vert );
            break;
        case VG_JOIN_MITER:
            if ( miter_limit_exceeded ) { // same as bevel
                vert.x = v1.x + dx1; vert.y = v1.y - dy1; verts.push_back( vert );
                vert.x = v1.x + dx2; vert.y = v1.y - dy2; verts.push_back( vert );
            } else {
                verts.push_back( result ); // miter point
            }
            break;
        case VG_JOIN_ROUND: {
            qreal approximation_scale = 0.25; // increase up to 1.0 for smoother
            calc_arc(verts, v1.x, v1.y, dx1, -dy1, dx2, -dy2, strokeRadius, approximation_scale );
        } break;
        }
    }

    void joinStrokeSegments(QVector<v2_t> &verts, strokeseg_t &seg0, strokeseg_t& seg1, bool reverse, qreal strokeRadius, VGJoinStyle joinStyle)
    {
        v2_t v0, v1, v2;
        if ( reverse ) {
            v0 = seg0.end;
            v1 = seg0.start;
            v2 = seg1.start;
        } else {
            v0 = seg0.start;
            v1 = seg0.end;
            v2 = seg1.end;
        }

        calc_join( verts, v0, v1, v2, seg0.len, seg1.len, strokeRadius, joinStyle, 666 );
    }

    void calc_join( QVector<v2_t>& verts, v2_t v0, v2_t v1, v2_t v2, qreal len1, qreal len2, qreal stroke_width, VGJoinStyle joinStyle, qreal miter_limit ) {

        qreal dx1, dy1, dx2, dy2;
        dx1 = stroke_width * (v1.y - v0.y) / len1;
        dy1 = stroke_width * (v1.x - v0.x) / len1;

        dx2 = stroke_width * (v2.y - v1.y) / len2;
        dy2 = stroke_width * (v2.x - v1.x) / len2;

#define calc_point_location(x1,y1,x2,y2,x,y) ((x - x2) * (y2 - y1) - (y - y2) * (x2 - x1))
        if ( calc_point_location(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y) > 0) {
            // Inner
            switch ( joinStyle ) {
            case VG_JOIN_BEVEL:
            case VG_JOIN_MITER:
            case VG_JOIN_ROUND:
                calc_miter(verts, v0, v1, v2, dx1, dy1, dx2, dy2, stroke_width, VG_JOIN_MITER, 1.01f); // always MITER
                break;
            }
        } else {
            // Outer
            switch ( joinStyle ) {
            case VG_JOIN_BEVEL:
            case VG_JOIN_MITER:
            case VG_JOIN_ROUND:
                calc_miter(verts, v0, v1, v2, dx1, dy1, dx2, dy2, stroke_width, joinStyle, 4.0f);
                break;
            }
        }
    }

    void calc_cap(QVector<v2_t> &verts, v2_t v0, v2_t v1, qreal len, VGCapStyle capStyle, qreal strokeRadius)
    {
        qreal dx1 = (v1.y - v0.y) / len;
        qreal dy1 = (v1.x - v0.x) / len;
        qreal dx2 = 0;
        qreal dy2 = 0;

        dx1 *= strokeRadius;
        dy1 *= strokeRadius;

        switch (capStyle) {
        case VG_CAP_BUTT:
            verts.push_back( v2_t(v0.x - dx1 - dx2, v0.y + dy1 - dy2) );
            verts.push_back( v2_t(v0.x + dx1 - dx2, v0.y - dy1 - dy2) );
            break;
        case VG_CAP_ROUND: {
            qreal approximation_scale = 0.25; // increase up to 1.0 for smoother
            qreal a1 = atan2(dy1, -dx1);
            qreal a2 = a1 + M_PI;
            qreal da = acos(strokeRadius / (strokeRadius + 0.125 / approximation_scale)) * 2;
            verts.push_back( v2_t(v0.x - dx1, v0.y + dy1) );
            a1 += da;
            a2 -= da/4;
            while(a1 < a2)
            {
                verts.push_back( v2_t(v0.x + cos(a1) * strokeRadius, v0.y + sin(a1) * strokeRadius) );
                a1 += da;
            }
            verts.push_back( v2_t(v0.x + dx1, v0.y - dy1) );
        }
            break;
        case VG_CAP_SQUARE:
            dx2 = dy1;
            dy2 = dx1;
            verts.push_back( v2_t(v0.x - dx1 - dx2, v0.y + dy1 - dy2) );
            verts.push_back( v2_t(v0.x + dx1 - dx2, v0.y - dy1 - dy2) );
            break;
        }
    }

    QVector<v2_t> _strokeVertices;
};
#endif

QPolygonF strokePath(WorldPath *path, qreal thickness)
{
    PathStroke stroke;
    QVector<PathStroke::v2_t> fwd, bwd;
    stroke.build(path, thickness, fwd, bwd);
    QPolygonF ret;
    foreach (PathStroke::v2_t v, fwd)
        ret += QPointF(v.x, v.y);
    if (path->isClosed())
        return ret; // FIXME: which outline is wanted?
    foreach (PathStroke::v2_t v, bwd)
        ret += QPointF(v.x, v.y);
    return ret;
}

QPolygonF offsetPath(WorldPath *path, qreal offset)
{
    PathStroke stroke;
    QVector<PathStroke::v2_t> fwd, bwd;
    stroke.build(path, offset * 2, fwd, bwd);
    QPolygonF ret;
    if (offset >= 0) {
        // Strip off the 2 cap vertices
        foreach (PathStroke::v2_t v, fwd.mid(1,fwd.size()-2))
            ret += QPointF(v.x, v.y);
    } else {
        foreach (PathStroke::v2_t v, bwd)
            ret += QPointF(v.x, v.y);
    }
    return ret;
}

QRegion polygonRegion(const QPolygonF &poly)
{
    QRect r = poly.boundingRect().toAlignedRect();
    QRegion ret;

    int IMAGE_LEFT = r.left();
    int IMAGE_RIGHT = r.right() + 1;
    int IMAGE_TOP = r.top();
    int IMAGE_BOT = r.bottom() + 1;
    const int MAX_POLY_CORNERS = 10000;
    int polyCorners = poly.size();

    // public-domain code by Darel Rex Finley, 2007
    // http://alienryderflex.com/polygon_fill/

    int nodes, pixelY, i, j;
    double nodeX[MAX_POLY_CORNERS];

    //  Loop through the rows of the image.
    for (pixelY = IMAGE_TOP; pixelY < IMAGE_BOT; pixelY++) {

        double testY = pixelY + 0.5; // test tile centers

        //  Build a list of nodes.
        nodes = 0;
        j = polyCorners - 1;
        for (i = 0; i < polyCorners; i++) {
            if ((poly[i].y() < testY && poly[j].y() >= testY)
                    || (poly[j].y() < testY && poly[i].y() >= testY)) {
                nodeX[nodes++] =
                        (poly[i].x() + (testY - poly[i].y()) / (poly[j].y() - poly[i].y())
                         * (poly[j].x() - poly[i].x()));
            }
            j = i;
        }

        //  Sort the nodes, via a simple “Bubble” sort.
        i = 0;
        while (i < nodes - 1) {
            if (nodeX[i] > nodeX[i+1]) {
                double swap = nodeX[i];
                nodeX[i] = nodeX[i+1];
                nodeX[i + 1]=swap;
                if (i) i--;
            }  else {
                i++;
            }
        }

        //  Fill the pixels between node pairs.
        for (i = 0; i < nodes; i += 2) {
            int x1 = nodeX[i], x2 = nodeX[i+1];
            if (nodeX[i] - x1 > 0.5) ++x1; // test tile centers
            if (nodeX[i+1] - x2 < 0.5) --x2; // test tile centers

            if (x1 >= IMAGE_RIGHT) break;
            if (x2 >= IMAGE_LEFT) {
                if (x1 < IMAGE_LEFT) x1 = IMAGE_LEFT;
                if (x2 > IMAGE_RIGHT) x2 = IMAGE_RIGHT;
                ret += QRect(x1 /*- IMAGE_LEFT*/, pixelY /*- IMAGE_TOP*/, x2 - x1 + 1, 1);
            }
        }
    }

    return ret;
}
