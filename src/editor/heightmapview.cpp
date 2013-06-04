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

#include "heightmapview.h"

#include "heightmap.h"
#include "heightmapdocument.h"
#include "heightmaptools.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <qmath.h>
#include <QGraphicsSceneMouseEvent>
#include <QtOpenGL>
#include <QStyleOptionGraphicsItem>
#include <QVector3D>

/////

HeightMapItem::HeightMapItem(HeightMapScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mShaderInit(false)
{
    setFlag(ItemUsesExtendedStyleOption);
}

QRectF HeightMapItem::boundingRect() const
{
    return mScene->boundingRect(mScene->worldBounds());
}

const char * tri_vert_string =
"/*                                                                            \n"
"	Input: The vertex position and vertex attributes p1_3d and p2_3d which       \n"
"	are the positions of neighbouring vertices.                                  \n"
"                                                                              \n"
"	Output:   dist a vector of distances from the vertex to the three edges of   \n"
"	the triangle. Clearly only one of these distance is non-zero. For vertex 0   \n"
"	in a triangle dist = (distance to opposite edge, 0, 0) on exit. The distance \n"
"	is multiplied by w. This is to negate perspective correction.                \n"
"*/                                                                            \n"
"uniform vec2 WIN_SCALE;                                                       \n"
"attribute vec4 p1_3d;                                                         \n"
"attribute vec4 p2_3d;                                                         \n"
"                                                                              \n"
"varying vec3 dist;                                                            \n"
"void main(void)                                                               \n"
"{                                                                             \n"
"	 // We store the vertex id (0,1, or 2) in the w coord of the vertex          \n"
"	 // which then has to be restored to w=1.                                    \n"
"	 float swizz = gl_Vertex.w;                                                  \n"
"	 vec4 pos = gl_Vertex;                                                       \n"
"	 pos.w = 1.0;                                                                \n"
"                                                                              \n"
"	 // Compute the vertex position in the usual fashion.                        \n"
"   gl_Position = gl_ModelViewProjectionMatrix * pos;                          \n"
"	                                                                             \n"
"	 // p0 is the 2D position of the current vertex.                             \n"
"	 vec2 p0 = gl_Position.xy/gl_Position.w;                                     \n"
"                                                                              \n"
"	 // Project p1 and p2 and compute the vectors v1 = p1-p0                     \n"
"	 // and v2 = p2-p0                                                           \n"
"	 vec4 p1_3d_ = gl_ModelViewProjectionMatrix * p1_3d;                         \n"
"	 vec2 v1 = WIN_SCALE*(p1_3d_.xy / p1_3d_.w - p0);                            \n"
"                                                                              \n"
"	 vec4 p2_3d_ = gl_ModelViewProjectionMatrix * p2_3d;                         \n"
"	 vec2 v2 = WIN_SCALE*(p2_3d_.xy / p2_3d_.w - p0);                            \n"
"                                                                              \n"
"	 // Compute 2D area of triangle.                                             \n"
"	 float area2 = abs(v1.x*v2.y - v1.y * v2.x);                                 \n"
"                                                                              \n"
"   // Compute distance from vertex to line in 2D coords                       \n"
"   float h = area2/length(v1-v2);                                             \n"
"                                                                              \n"
"   // ---                                                                     \n"
"   // The swizz variable tells us which of the three vertices                 \n"
"   // we are dealing with. The ugly comparisons would not be needed if 	     \n"
"   // swizz was an int.                                                     	 \n"
"                                                                              \n"
"   if(swizz<0.1)                                                              \n"
"      dist = vec3(h,0,0);                                                     \n"
"   else if(swizz<1.1)                                                         \n"
"      dist = vec3(0,h,0);                                                     \n"
"   else                                                                       \n"
"      dist = vec3(0,0,h);                                                     \n"
"                                                                              \n"
"   // ----                                                                    \n"
"   // Quick fix to defy perspective correction                                \n"
"                                                                              \n"
"   dist *= gl_Position.w;                                                     \n"
"}                                                                             \n";

const char * tri_frag_string =
"                                                                              \n"
"uniform vec3 WIRE_COL;                                                        \n"
"uniform vec3 FILL_COL;                                                        \n"
"                                                                              \n"
"varying vec3 dist;                                                            \n"
"                                                                              \n"
"void main(void)                                                               \n"
"{                                                                             \n"
"   // Undo perspective correction.                                            \n"
"	  vec3 dist_vec = dist * gl_FragCoord.w;                                     \n"
"                                                                              \n"
"   // Compute the shortest distance to the edge                               \n"
"	  float d =min(dist_vec[0],min(dist_vec[1],dist_vec[2]));                    \n"
"                                                                              \n"
"	  // Compute line intensity and then fragment color                          \n"
" 	float I = exp2(-2.0*d*d);                                                  \n"
" 	gl_FragColor.xyz = I*WIRE_COL + (1.0 - I)*FILL_COL;                        \n"
"}                                                                             \n";

void HeightMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    QPointF tl = mScene->toWorld(option->exposedRect.topLeft());
    QPointF tr = mScene->toWorld(option->exposedRect.topRight());
    QPointF br = mScene->toWorld(option->exposedRect.bottomRight());
    QPointF bl = mScene->toWorld(option->exposedRect.bottomLeft());

    HeightMap *hm = mScene->hm();

    int minX = tl.x(), minY = tr.y(), maxX = qCeil(br.x()), maxY = qCeil(bl.y());
//    minX -= mScene->worldOrigin().x(), maxX -= mScene->worldOrigin().x();
//    minY -= mScene->worldOrigin().y(), maxY -= mScene->worldOrigin().y();
    minX = qBound(mScene->worldOrigin().x(), minX, hm->width());
    minY = qBound(mScene->worldOrigin().y(), minY, hm->height());
    maxX = qBound(mScene->worldOrigin().x(), maxX, hm->width());
    maxY = qBound(mScene->worldOrigin().y(), maxY, hm->height());


    int columns = maxX - minX + 1;
    int rows = maxY - minY + 1;

#if 1
    painter->beginNativePainting();

    if (!mShaderInit) {
        mShaderProgram.addShaderFromSourceCode(QGLShader::Vertex, tri_vert_string);
        mShaderProgram.addShaderFromSourceCode(QGLShader::Fragment, tri_frag_string);
        mShaderOK = mShaderProgram.link();
        if (mShaderOK) {
            mShaderProgram.bind();
            p1_attrib = mShaderProgram.attributeLocation("p1_3d");
            p2_attrib = mShaderProgram.attributeLocation("p2_3d");
            mShaderProgram.setUniformValue("WIN_SCALE", 800.0, 800.0); // affects line thickness
            mShaderProgram.setUniformValue("WIRE_COL", 0.8f, 0.3f, 0.1f);
            mShaderProgram.setUniformValue("FILL_COL", 0.7f, 0.8f, 0.9f);
            mShaderProgram.release();
        }
        mShaderInit = true;
    }

    if (mShaderInit && mShaderOK) {
        QVector<QVector3D> vertices;
        vertices.resize(columns * rows);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < columns; c++) {
                int index = r * columns + c;
                int h = 0;
                if (hm->contains(minX + c, minY + r))
                    h = hm->heightAt(minX + c, minY + r);
                QPointF p = mScene->toScene(minX + c, minY + r);
                p.ry() -= h;
                vertices[index] = QVector3D(p.x(), p.y(), 0.0);
            }
        }

        QVector<GLushort> indices;
        indices.reserve(columns * rows * 2);
        for (int r = 0;  r < rows - 1; r++) {
            indices += r * columns;
            for (int c = 0; c < columns; c++) {
                indices += r * columns + c;
                indices += (r + 1) * columns + c;
            }
            indices += (r + 1) * columns + (columns - 1);
        }

        mShaderProgram.bind();

        GLuint mesh_list = glGenLists(1);
        glNewList(mesh_list, GL_COMPILE);
        glBegin(GL_TRIANGLES);

        for (int i = 0; i < indices.size() - 2; i += 1) {
            draw_triangle(vertices[indices[i]],
                    vertices[indices[i+1]],
                    vertices[indices[i+2]]);
        }

        glEnd();
        glEndList();

        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCallList(mesh_list);

        mShaderProgram.release();
        glDeleteLists(mesh_list, 1);
        glDisable(GL_DEPTH_TEST);
    }

    painter->endNativePainting();
#elif 0
    int verticesCount = columns * rows;
    int indicesCount = columns * rows + (columns - 1) * (rows - 2);

    QVector<QVector3D> vertices(verticesCount);
    int i = 0;
    for ( int row=0; row<rows; row++ ) {
        for ( int col=0; col<columns; col++ ) {
            int h = 0;
            if (hm->contains(minX + col, minY + row))
                h = hm->heightAt(minX + col, minY + row);
            QPointF p = mScene->toScene(minX + col, minY + row);
            p.ry() -= h;
            vertices[i++] = QVector3D(p.x(), p.y(), 0.0);
        }
    }

    QVector<GLushort> indices;
    indices.reserve(indicesCount);
    for ( int row=0; row<rows-1; row++ ) {
        if ( (row&1)==0 ) { // even rows
            for ( int col=0; col<columns; col++ ) {
                indices += col + row * columns;
                indices += col + (row+1) * columns;
            }
        } else { // odd rows
            for ( int col=columns-1; col>0; col-- ) {
                indices += col + (row+1) * columns;
                indices += col - 1 + row * columns;
            }
        }
    }
    if ( (rows&1) && rows>2 ) {
        indices += (rows-1) * columns;
    }
#else
    QVector<QVector3D> vertices;
    vertices.resize(columns * rows);
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
            int index = r * columns + c;
            int h = 0;
            if (hm->contains(minX + c, minY + r))
                h = hm->heightAt(minX + c, minY + r);
            QPointF p = mScene->toScene(minX + c, minY + r);
            p.ry() -= h;
            vertices[index] = QVector3D(p.x(), p.y(), 0.0);
        }
    }

    QVector<GLushort> indices;
    indices.reserve(columns * rows * 2);
    for (int r = 0;  r < rows - 1; r++) {
        indices += r * columns;
        for (int c = 0; c < columns; c++) {
            indices += r * columns + c;
            indices += (r + 1) * columns + c;
        }
        indices += (r + 1) * columns + (columns - 1);
    }

    painter->beginNativePainting();

    glEnableClientState( GL_VERTEX_ARRAY );
//    glEnableClientState( GL_INDEX_ARRAY );
//    glDisable(GL_TEXTURE_2D);
    glVertexPointer( 3, GL_FLOAT, 0, vertices.constData() );

    glColor3f(0.5,0.5,0.5);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawElements( GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_SHORT, indices.constData() );

//    glDisable(GL_DEPTH_TEST);
    glColor3f(0,0,0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements( GL_TRIANGLE_STRIP, indices.size(), GL_UNSIGNED_SHORT, indices.constData() );

    glDisableClientState( GL_VERTEX_ARRAY );
//    glDisableClientState( GL_INDEX_ARRAY );
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    painter->endNativePainting();
#endif
#if 0
    HeightMap *hm = mScene->hm();

    painter->setBrush(Qt::lightGray);

    const int tileWidth = mScene->tileWidth();
    const int tileHeight = mScene->tileHeight();

    if (tileWidth <= 0 || tileHeight <= 1)
        return;

    QRect rect = option->exposedRect.toAlignedRect();
//    if (rect.isNull())
//        rect = layerGroup->boundingRect(this).toAlignedRect();

    QMargins drawMargins/* = layerGroup->drawMargins();
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth)*/;

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = mScene->toWorld(rect.x(), rect.y());
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = mScene->toScene(rowItr);
    startPos.rx() -= tileWidth / 2;
    startPos.ry() += tileHeight;

    /* Determine in which half of the tile the top-left corner of the area we
     * need to draw is. If we're in the upper half, we need to start one row
     * up due to those tiles being visible as well. How we go up one row
     * depends on whether we're in the left or right half of the tile.
     */
    const bool inUpperHalf = startPos.y() - rect.y() > tileHeight / 2;
    const bool inLeftHalf = rect.x() - startPos.x() < tileWidth / 2;

    if (inUpperHalf) {
        if (inLeftHalf) {
            --rowItr.rx();
            startPos.rx() -= tileWidth / 2;
        } else {
            --rowItr.ry();
            startPos.rx() += tileWidth / 2;
        }
        startPos.ry() -= tileHeight / 2;
    }

    // Determine whether the current row is shifted half a tile to the right
    bool shifted = inUpperHalf ^ inLeftHalf;

    for (int y = startPos.y(); y - tileHeight < rect.bottom(); y += tileHeight / 2) {
        QPoint columnItr = rowItr;
        for (int x = startPos.x(); x < rect.right(); x += tileWidth) {
            if (hm->contains(columnItr.x(), columnItr.y())) {
                int h = hm->heightAt(columnItr.x(), columnItr.y());
                QPolygonF poly = mScene->toScene(QRect(columnItr.x(), columnItr.y(), 1, 1));
                poly.translate(0, -h);
                painter->drawPolygon(poly);
                if (h > 0) {
                    QPolygonF side1;
                    side1 << poly[3] << poly[2] << poly[2] + QPointF(0, h) << poly[3] + QPointF(0, h) << poly[3];
                    painter->drawPolygon(side1);

                    QPolygonF side2;
                    side2 << poly[2] + QPointF(0, h) << poly[2] << poly[1] << poly[1] + QPointF(0, h) << poly[2] + QPointF(0, h);
                    painter->drawPolygon(side2);
                }
            }

            // Advance to the next column
            ++columnItr.rx();
            --columnItr.ry();
        }

        // Advance to the next row
        if (!shifted) {
            ++rowItr.rx();
            startPos.rx() += tileWidth / 2;
            shifted = true;
        } else {
            ++rowItr.ry();
            startPos.rx() -= tileWidth / 2;
            shifted = false;
        }
    }
#endif
}

void HeightMapItem::draw_triangle(const QVector3D &v0, const QVector3D &v1, const QVector3D &v2)
{
    mShaderProgram.setAttributeValue(p1_attrib, v1.x(), v1.y(), v1.z(), 1);
    mShaderProgram.setAttributeValue(p2_attrib, v2.x(), v2.y(), v2.z(), 1);
    glVertex4f(v0.x(), v0.y(), v0.z(), 0);

    mShaderProgram.setAttributeValue(p1_attrib, v2.x(), v2.y(), v2.z(), 1);
    mShaderProgram.setAttributeValue(p2_attrib, v0.x(), v0.y(), v0.z(), 1);
    glVertex4f(v1.x(), v1.y(), v1.z(), 1);

    mShaderProgram.setAttributeValue(p1_attrib, v0.x(), v0.y(), v0.z(), 1);
    mShaderProgram.setAttributeValue(p2_attrib, v1.x(), v1.y(), v1.z(), 1);
    glVertex4f(v2.x(), v2.y(), v2.z(), 2);
}

/////

HeightMapScene::HeightMapScene(HeightMapDocument *hmDoc, QObject *parent) :
    BaseGraphicsScene(HeightMapSceneType, parent),
    mDocument(hmDoc),
    mHeightMap(new HeightMap(hmDoc->hmFile())),
    mHeightMapItem(new HeightMapItem(this)),
    mActiveTool(0)
{
    addItem(mHeightMapItem);

    setBackgroundBrush(Qt::darkGray);

    setCenter(worldOrigin().x() + 300 / 2,
              worldOrigin().y() + 300 / 2);
}

HeightMapScene::~HeightMapScene()
{
    delete mHeightMap;
}

void HeightMapScene::setTool(AbstractTool *tool)
{
    BaseHeightMapTool *hmTool = tool ? tool->asHeightMapTool() : 0;

    if (mActiveTool == hmTool)
        return;

    if (mActiveTool) {
        mActiveTool->deactivate();
    }

    mActiveTool = hmTool;

    if (mActiveTool) {
        mActiveTool->activate();
    }
}

QPointF HeightMapScene::toScene(const QPointF &worldPos) const
{
    return toScene(worldPos.x(), worldPos.y());
}

QPointF HeightMapScene::toScene(qreal x, qreal y) const
{
    x -= worldOrigin().x(), y -= worldOrigin().y();
    const int originX = (300 * 3) * tileWidth() / 2; // top-left corner
    const int originY = 0;
    return QPointF((x - y) * tileWidth() / 2 + originX,
                   (x + y) * tileHeight() / 2 + originY);
}

QPolygonF HeightMapScene::toScene(const QRect &rect) const
{
    QPolygonF polygon;
    polygon += toScene(rect.topLeft());
    polygon += toScene(rect.topRight() + QPoint(1, 0));
    polygon += toScene(rect.bottomRight() + QPoint(1, 1));
    polygon += toScene(rect.bottomLeft() + QPoint(0, 1));
    polygon += polygon.first();
    return polygon;
}

QRect HeightMapScene::boundingRect(const QRect &_rect) const
{
    QRect rect = _rect.translated(-worldOrigin());
    const int originX = (300 * 3) * tileWidth() / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth() / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight() / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth() / 2,
                     side * tileHeight() / 2);

    return QRect(pos, size);
}

QPointF HeightMapScene::toWorld(const QPointF &scenePos) const
{
    return toWorld(scenePos.x(), scenePos.y());
}

QPointF HeightMapScene::toWorld(qreal x, qreal y) const
{
    const qreal ratio = (qreal) tileWidth() / tileHeight();

    x -= (300 * 3) * tileWidth() / 2;
    y -= 0/*map()->cellsPerLevel().y() * (maxLevel() - level) * tileHeight*/;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight(),
                   my / tileHeight()) + worldOrigin();
}

QPoint HeightMapScene::toWorldInt(const QPointF &scenePos) const
{
    QPointF p = toWorld(scenePos);
    return QPoint(p.x(), p.y()); // don't use toPoint() it rounds up
}

void HeightMapScene::setCenter(int x, int y)
{
    mHeightMap->setCenter(x, y);
}

QPoint HeightMapScene::worldOrigin() const
{
    int x = qBound(0, mDocument->cell()->x() - 1, mDocument->worldDocument()->world()->width() - 1);
    int y = qBound(0, mDocument->cell()->y() - 1, mDocument->worldDocument()->world()->height() - 1);

    return QPoint(x * 300, y * 300);
}

QRect HeightMapScene::worldBounds() const
{
    int x = qBound(0, mDocument->cell()->x() + 1, mDocument->worldDocument()->world()->width() - 1);
    int y = qBound(0, mDocument->cell()->y() + 1, mDocument->worldDocument()->world()->height() - 1);
    return QRect(worldOrigin(), QPoint((x + 1) * 300, (y + 1) * 300));
}

void HeightMapScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mousePressEvent(event);
}

void HeightMapScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mouseMoveEvent(event);
}

/////

HeightMapView::HeightMapView(QWidget *parent) :
    BaseGraphicsView(AlwaysGL, parent),
    mRecenterScheduled(false)
{
}

void HeightMapView::mouseMoveEvent(QMouseEvent *event)
{
    QPoint tilePos = scene()->toWorldInt(mapToScene(event->pos()));
    emit statusBarCoordinatesChanged(tilePos.x(), tilePos.y());

    BaseGraphicsView::mouseMoveEvent(event);
}

QRectF HeightMapView::sceneRectForMiniMap() const
{
    if (!scene())
        return QRectF();
    return scene()->boundingRect(
                QRect(scene()->document()->cell()->pos() * 300, QSize(300,300)));
}

void HeightMapView::scrollContentsBy(int dx, int dy)
{
    BaseGraphicsView::scrollContentsBy(dx, dy);

#if 0
    // scrollContentsBy() gets called twice, once for the horizontal scrollbar
    // and once for the vertical scrollbar.  When changing scale, the dx/dy
    // values are quite large.  So, don't update the heightmap until both
    // calls have completed.
    if (!mRecenterScheduled) {
        QMetaObject::invokeMethod(this, "recenter", Qt::QueuedConnection);
        mRecenterScheduled = true;
    }
#endif
}

void HeightMapView::recenter()
{
    mRecenterScheduled = false;

    QPointF viewPos = viewport()->rect().center();
    QPointF scenePos = mapToScene(viewPos.toPoint());
    QPointF worldPos = scene()->toWorld(scenePos.x(), scenePos.y());

    scene()->setCenter(worldPos.x(), worldPos.y());
}
