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

#include "bmpblender.h"
#include "heightmap.h"
#include "heightmapdocument.h"
#include "heightmaptools.h"
#include "mapcomposite.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "preferences.h"
#include "texturemanager.h"
#include "virtualtileset.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <qmath.h>
#include <QGraphicsSceneMouseEvent>
#include <QtOpenGL>
#include <QStyleOptionGraphicsItem>
#include <QVector3D>

/////

HeightMapItem::HeightMapItem(HeightMapScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mDisplayStyle(MeshStyle)
{
    mDisplayStyle = Preferences::instance()->heightMapDisplayStyle()
            ? FlatStyle : MeshStyle;

    setFlag(ItemUsesExtendedStyleOption);
}

QRectF HeightMapItem::boundingRect() const
{
    return mScene->boundingRect(mScene->worldBounds());
}

void HeightMapItem::loadGLTextures()
{
    if (!mScene->mapComposite()) return;

    QImage t;
    QImage b;

    QMap<QString,GLuint> textureNameToID;
    foreach (Tiled::Internal::TextureInfo *tex, Tiled::Internal::TextureMgr::instance().textures()) {
        QString imageSource = tex->fileName();
        if (QDir::isRelativePath(imageSource))
            imageSource = QDir(Preferences::instance()->texturesDirectory()).filePath(imageSource);

        QImageReader reader(imageSource);
        if (!reader.size().isValid()) {
            qDebug() << "Error getting size of texture image" << tex->name();
            continue;
        }
#if 0
        if ((reader.size().width() / tex->tileWidth() < ts->columnCount()) ||
                (reader.size().height() / tex->tileHeight() < ts->tileCount() / ts->columnCount())) {
            qDebug() << "Texture image is too small for tileset" << ts->name();
            if (ts->name() != QLatin1String("walls_exterior_house_01"))
                continue;
        }
#endif
        if (b.load(imageSource)) {
            QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
            QPainter painter(&fixedImage);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(fixedImage.rect(), Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage( 0, 0, b);
            painter.end();
            b = fixedImage;

            t = QGLWidget::convertToGLFormat( b );
            GLuint textureID;
            glGenTextures( 1, &textureID );
            glBindTexture( GL_TEXTURE_2D, textureID );
            glTexImage2D( GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits() );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

            textureNameToID[tex->name()] = textureID;
        }
    }

    foreach (Tiled::Tileset *ts, mScene->mapComposite()->usedTilesets()) {
        Tiled::Internal::VirtualTileset *vts = Tiled::Internal::VirtualTilesetMgr::instance().tileset(ts->name());
        if (!vts)
            continue;
        mVirtualTileset[ts] = vts;
        for (int id = 0; id < ts->tileCount(); id++) {
            if (Tiled::Internal::VirtualTile *vtile = vts->tileAt(id)) {
                if (vtile->shape() && textureNameToID.contains(vtile->imageSource())) {
                    unsigned int textureID = textureNameToID[vtile->imageSource()];
                    mTextureID[ts->tileAt(id)] = textureID;
                    mTextureInfo[textureID] = Tiled::Internal::TextureMgr::instance().texture(vtile->imageSource());
                }
            }
        }
    }
}

namespace {

class DrawElements
{
public:
    void clear()
    {
        counts.resize(0);
        indices.resize(0);
        vertices.resize(0);
        texcoords.resize(0);
        textureids.resize(0);
        colors.resize(0);
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3, const QVector2D &uv4,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4,
             const QVector3D &color = QVector3D(1, 1, 1))
    {
        counts += 4;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        colors += color;
#if 1
        QVector2D _uv1 = uv1, _uv2 = uv2, _uv3 = uv3, _uv4 = uv4;
        _uv1.setY(1 - uv1.y()), _uv2.setY(1 - uv2.y()), _uv3.setY(1 - uv3.y()), _uv4.setY(1 - uv4.y());
#endif
        texcoords << _uv1 << _uv2 << _uv3 << _uv4;
        vertices << v1 << v2 << v3 << v4;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3,
             const QVector3D &color = QVector3D(1, 1, 1))
    {
        counts += 3;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2;
        textureids += textureid;
        colors += color;
#if 1
        QVector2D _uv1 = uv1, _uv2 = uv2, _uv3 = uv3;
        _uv1.setY(1 - uv1.y()), _uv2.setY(1 - uv2.y()), _uv3.setY(1 - uv3.y());
#endif
        texcoords << _uv1 << _uv2 << _uv3;
        vertices << v1 << v2 << v3;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void color(float r, float g, float b)
    {
        colors.last() = QVector3D(r, g, b);
    }

    void flush()
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
        const GLushort *ind = indices.constData();
        for (int i = 0; i < textureids.size(); i++) {
            glColor3f(colors[i].x(), colors[i].y(), colors[i].z());
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            if (counts[i] == 4)
                glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, ind);
            else if (counts[i] == 3)
                glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, ind);
            ind += counts[i];
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        clear();
    }

    QVector<char> counts;
    QVector<GLushort> indices;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLuint> textureids;
    QVector<QVector3D> colors;

};

} // namespace anon

void HeightMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (mDisplayStyle == FlatStyle) {
        paint2(painter, option);
        return;
    }

    QPointF tl = mScene->toWorld(option->exposedRect.topLeft());
    QPointF tr = mScene->toWorld(option->exposedRect.topRight());
    QPointF br = mScene->toWorld(option->exposedRect.bottomRight());
    QPointF bl = mScene->toWorld(option->exposedRect.bottomLeft());

    int minX = tl.x(), minY = tr.y(), maxX = qCeil(br.x()), maxY = qCeil(bl.y());
    minX = qBound(mScene->worldOrigin().x(), minX, mScene->worldBounds().right());
    minY = qBound(mScene->worldOrigin().y(), minY, mScene->worldBounds().bottom());
    maxX = qBound(mScene->worldOrigin().x(), maxX, mScene->worldBounds().right());
    maxY = qBound(mScene->worldOrigin().y(), maxY, mScene->worldBounds().bottom());

    int columns = maxX - minX + 1;
    int rows = maxY - minY + 1;

    painter->beginNativePainting();

#if 1 // vts
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // http://www.java-gaming.org/index.php?PHPSESSID=911v5td96tpd7t3ufscsujuls1&/topic,10237.msg82081.html#msg82081
#if 0
    GLfloat m[16] = { 1.0, 0.0,  -1.0, 0.0, // x
                      0.5, 1.0,   0.5, 0.0, // y
                      0.0, -0.05, 0.0, 0.0, // depth
                      0.0, 0.0,   0.0, 0.0 };
    // below is transpose of above matrix (OpenGL works with transposed matrices)
#endif
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2], height = viewport[3];
    GLfloat ratio = width / GLfloat(height);
    GLfloat m[16] = { 1.0F,  0.5F * ratio, 0.0F,   0.0F,
                      0.0F,  1.0F * ratio, -0.05F, 0.0F,
                      -1.0F, 0.5F * ratio, 0.0F,   0.0F,
                      0.0F,  0.0F, 0.0F,   1.0F };
    glLoadMatrixf(m);
    glRotatef(-90,1,0,0);
    glRotatef(90,0,0,1); // x-right, y-to-camera, z-up same as TileShapeEditor

    GLfloat scale = mScene->document()->view()->zoomable()->scale();
    scale /= (width / 64.0);
    glScalef(scale, scale, scale);

    {
    QPointF tilePos = mScene->toWorld(option->exposedRect.center());
    int cellX = mScene->document()->cell()->pos().x() * 300;
    int cellY = mScene->document()->cell()->pos().y() * 300;
    glTranslatef(-(tilePos.x()-cellX),-(tilePos.y()-cellY),0);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#endif

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if QT_VERSION_XXX >= 0x050000
#else
    glShadeModel(GL_FLAT);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
#if QT_VERSION_XXX >= 0x050000
#else
    glClearDepth(1.0f);
#endif
    glClear(GL_DEPTH_BUFFER_BIT);

    if (mTextureID.isEmpty()) {
        loadGLTextures();
    }

    MapComposite *mc = mScene->mapComposite();
    CompositeLayerGroup *lg = mc ? mc->layerGroupForLevel(0) : 0;

    if (lg && columns * rows < 100000) {
        int cellX = mScene->document()->cell()->pos().x() * 300;
        int cellY = mScene->document()->cell()->pos().y() * 300;
        static QVector<const Tiled::Cell*> cells(40);
        HeightMap *hm = mScene->hm();
        foreach (lg, mc->layerGroups()) {
            DrawElements de;
            //        mc->bmpBlender()->flush(QRect(minX-cellX, minY-cellY, columns, rows));
            lg->prepareDrawing2();
            /*if (lg->needsSynch()) lg->synch();*/
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < columns; c++) {
                    QPoint tilePos(minX+c-cellX,minY+r-cellY);
                    if (!QRect(0,0,300,300).contains(tilePos)) continue;
                    cells.resize(0);
                    lg->orderedCellsAt2(tilePos, cells);

                    qreal hmOffset = 0;
                    if (hm->contains(minX + c, minY + r))
                        hmOffset = hm->heightAt(minX + c, minY + r) / 64.0;

                    foreach (const Tiled::Cell *cell, cells) {
                        if (!cell->tile || !mTextureID.contains(cell->tile)) continue;
                        GLuint textureID = mTextureID[cell->tile];
                        Tiled::Internal::TextureInfo *tex = mTextureInfo[textureID];
                        Tiled::Internal::VirtualTileset *vts = mVirtualTileset[cell->tile->tileset()];
                        Tiled::Internal::VirtualTile *vtile = vts->tileAt(cell->tile->id());
                        if (!vtile || !vtile->shape()) continue;
                        int tw = tex->tileWidth(), th = tex->tileHeight();
                        float texWid = tex->columnCount() * tw, texHgt = tex->rowCount() * th;

#define toUV(uv) QVector2D((vtile->srcX() * tw)/qreal(texWid) + uv.x() * tw/qreal(texWid), (vtile->srcY() * th)/qreal(texHgt) + uv.y() * th/qreal(texHgt))
#define toGL(v) QVector3D(tilePos.x()+v.x(), tilePos.y()+v.y(), hmOffset + lg->level() * 3 + v.z())
                        foreach (Tiled::Internal::TileShapeFace e, vtile->shape()->mFaces) {
                            QVector3D color(1, 1, 1);
                            QVector3D cross = QVector3D::normal(e.mGeom[0], e.mGeom[1], e.mGeom[2]);
                            if (cross.x() > 0 && cross.y() == 0 /*cross == QVector3D(1, 0, 0)*/)
                                color = QVector3D(0.8f,0.8f,0.8f);
                            if (e.mGeom.size() == 4)
                                de.add(textureID,
                                       toUV(e.mUV[0]), toUV(e.mUV[1]), toUV(e.mUV[2]), toUV(e.mUV[3]),
                                        toGL(e.mGeom[0]), toGL(e.mGeom[1]), toGL(e.mGeom[2]), toGL(e.mGeom[3]),
                                        color);
                            else if (e.mGeom.size() == 3)
                                de.add(textureID,
                                       toUV(e.mUV[0]), toUV(e.mUV[1]), toUV(e.mUV[2]),
                                        toGL(e.mGeom[0]), toGL(e.mGeom[1]), toGL(e.mGeom[2]),
                                        color);

                        }
                    }
                }
            }
            de.flush();
        }
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

#if 1
    glBegin(GL_LINES);

    glColor3d(1.0, 0.0, 0.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(1.0, 0.0, 0.0);

    glColor3d(0.0, 1.0, 0.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 1.0, 0.0);

    glColor3d(0.0, 0.0, 1.0);
    glVertex3d(0.0, 0.0, 0.0);
    glVertex3d(0.0, 0.0, 1.0);

    glEnd();
#endif

    painter->endNativePainting();
}

void HeightMapItem::paint2(QPainter *painter, const QStyleOptionGraphicsItem *option)
{
    HeightMap *hm = mScene->hm();

    painter->setBrush(Qt::gray);

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
    QPoint rowItr = QPoint((int) qFloor(tilePos.x()),
                           (int) qFloor(tilePos.y()));
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
                painter->setBrush(Qt::gray);
                painter->drawPolygon(poly);
                if (h > 0) {
                    QPolygonF side1;
                    side1 << poly[3] << poly[2] << poly[2] + QPointF(0, h) << poly[3] + QPointF(0, h) << poly[3];
                    painter->setBrush(Qt::lightGray);
                    painter->drawPolygon(side1);

                    QPolygonF side2;
                    side2 << poly[2] + QPointF(0, h) << poly[2] << poly[1] << poly[1] + QPointF(0, h) << poly[2] + QPointF(0, h);
                    painter->setBrush(Qt::darkGray);
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
}

/////

HMMiniMapItem::HMMiniMapItem(HeightMapScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(scene)
    , mCell(scene->document()->cell())
    , mMapImage(0)
{
    setAcceptedMouseButtons(0);

    updateCellImage();

    mLotImages.resize(mCell->lots().size());
    for (int i = 0; i < mCell->lots().size(); i++)
        updateLotImage(i);

    updateBoundingRect();

    connect(MapImageManager::instance(), SIGNAL(mapImageChanged(MapImage*)),
            SLOT(mapImageChanged(MapImage*)));
    connect(mScene, SIGNAL(sceneRectChanged(QRectF)), SLOT(sceneRectChanged(QRectF)));

    WorldDocument *worldDoc = mScene->document()->worldDocument();
    connect(worldDoc, SIGNAL(cellContentsAboutToChange(WorldCell*)), SLOT(cellContentsAboutToChange(WorldCell*)));
    connect(worldDoc, SIGNAL(cellContentsChanged(WorldCell*)), SLOT(cellContentsChanged(WorldCell*)));

    connect(worldDoc, SIGNAL(cellLotAdded(WorldCell*,int)), SLOT(lotAdded(WorldCell*,int)));
    connect(worldDoc, SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)), SLOT(lotRemoved(WorldCell*,int)));
    connect(worldDoc, SIGNAL(cellLotMoved(WorldCellLot*)), SLOT(lotMoved(WorldCellLot*)));
}

QRectF HMMiniMapItem::boundingRect() const
{
    return mBoundingRect;
}

void HMMiniMapItem::paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *)
{
    Q_UNUSED(option)

    if (mMapImage) {
        QRectF target = mMapImageBounds;
        QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
        painter->drawImage(target, mMapImage->image(), source);
    }

    foreach (const LotImage &lotImage, mLotImages) {
        if (!lotImage.mMapImage) continue;
        QRectF target = lotImage.mBounds;
        QRectF source = QRect(QPoint(0, 0), lotImage.mMapImage->image().size());
        painter->drawImage(target, lotImage.mMapImage->image(), source);
    }
}

void HMMiniMapItem::updateCellImage()
{
    mMapImage = 0;
    mMapImageBounds = QRect();

    if (!mCell->mapFilePath().isEmpty()) {
        mMapImage = MapImageManager::instance()->getMapImage(mCell->mapFilePath());
        if (mMapImage) {
            QPointF offset = mMapImage->tileToImageCoords(0, 0) / mMapImage->scale();
            mMapImageBounds = QRectF(mScene->toScene(mCell->x() * 300, mCell->y() * 300) - offset,
                                     mMapImage->image().size() / mMapImage->scale());
        }
    }
}

void HMMiniMapItem::updateLotImage(int index)
{
    WorldCellLot *lot = mCell->lots().at(index);
    MapImage *mapImage = MapImageManager::instance()->getMapImage(lot->mapName()/*, mapFilePath()*/);
    if (mapImage) {
        QPointF offset = mapImage->tileToImageCoords(0, 0) / mapImage->scale();
        QRectF bounds = QRectF(mScene->toScene(mCell->x() * 300 + lot->x(),
                                               mCell->y() * 300 + lot->y()/*, lot->level()*/) - offset,
                               mapImage->image().size() / mapImage->scale());
        mLotImages[index].mBounds = bounds;
        mLotImages[index].mMapImage = mapImage;
    } else {
        mLotImages[index].mBounds = QRectF();
        mLotImages[index].mMapImage = 0;
    }
}

void HMMiniMapItem::updateBoundingRect()
{
    QRectF bounds = mScene->boundingRect(QRect(mCell->pos() * 300, QSize(300,300)));

    if (!mMapImageBounds.isEmpty())
        bounds |= mMapImageBounds;

    foreach (LotImage lotImage, mLotImages) {
        if (!lotImage.mBounds.isEmpty())
            bounds |= lotImage.mBounds;
    }

    if (mBoundingRect != bounds) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void HMMiniMapItem::lotAdded(WorldCell *cell, int index)
{
    if (cell != mCell) return;
    mLotImages.insert(index, LotImage());
    updateLotImage(index);
    updateBoundingRect();
    update();
}

void HMMiniMapItem::lotRemoved(WorldCell *cell, int index)
{
    if (cell != mCell) return;
    mLotImages.remove(index);
    updateBoundingRect();
    update();
}

void HMMiniMapItem::lotMoved(WorldCellLot *lot)
{
    if (lot->cell() != mCell) return;
    updateLotImage(mCell->indexOf(lot));
    updateBoundingRect();
    update();
}

void HMMiniMapItem::cellContentsAboutToChange(WorldCell *cell)
{
    if (cell != mCell) return;
    mLotImages.clear();
}

void HMMiniMapItem::cellContentsChanged(WorldCell *cell)
{
    if (cell != mCell) return;

    updateCellImage();

    mLotImages.resize(mCell->lots().size());
    for (int i = 0; i < mCell->lots().size(); i++)
        updateLotImage(i);

    updateBoundingRect();
    update();
}

void HMMiniMapItem::sceneRectChanged(const QRectF &sceneRect)
{
    Q_UNUSED(sceneRect)
    updateCellImage();
    for (int i = 0; i < mLotImages.size(); i++)
        updateLotImage(i);
}

void HMMiniMapItem::mapImageChanged(MapImage *mapImage)
{
    if (mapImage == mMapImage) {
        update();
        return;
    }
    foreach (const LotImage &lotImage, mLotImages) {
        if (mapImage == lotImage.mMapImage) {
            update();
            return;
        }
    }
}

/////

HeightMapScene::HeightMapScene(HeightMapDocument *hmDoc, QObject *parent) :
    BaseGraphicsScene(HeightMapSceneType, parent),
    mDocument(hmDoc),
    mHeightMap(new HeightMap(hmDoc->hmFile())),
    mActiveTool(0),
    mMapComposite(0)
{

    setBackgroundBrush(Qt::darkGray);

    setCenter(mDocument->cell()->x() * 300 + 300 / 2,
              mDocument->cell()->y() * 300 + 300 / 2);

    connect(mDocument->worldDocument(), SIGNAL(heightMapPainted(QRegion)),
            SLOT(heightMapPainted(QRegion)));

    connect(Preferences::instance(), SIGNAL(heightMapDisplayStyleChanged(int)),
            SLOT(heightMapDisplayStyleChanged(int)));


    MapInfo *mapInfo = 0;
    if (document()->cell()->mapFilePath().isEmpty())
        mapInfo = MapManager::instance()->getEmptyMap();
    else {
        mapInfo = MapManager::instance()->loadMap(document()->cell()->mapFilePath());
        if (!mapInfo)
            mapInfo = MapManager::instance()->getPlaceholderMap(
                        document()->cell()->mapFilePath(), 300, 300);
    }
    if (!mapInfo)
        return;
    mMapComposite = new MapComposite(mapInfo, Tiled::Map::LevelIsometric);

    LotsLoader lotLoader;
    foreach (WorldCellLot *lot, document()->cell()->lots())
        lotLoader.loadLot(lot);

    while (mMapComposite->waitingForMapsToLoad() || lotLoader.waitingForMapsToLoad())
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    foreach (LotsLoader::LoadInfo info, lotLoader.loadedLots())
        mMapComposite->addMap(info.mMapInfo, info.mLot->pos(), info.mLot->level());

    // Do this to ensure all the blender-related textures are loaded
    mMapComposite->bmpBlender()->flush(QRect(0,0,300,300));

    mHeightMapItem = new HeightMapItem(this);
    addItem(mHeightMapItem);
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

QPolygonF HeightMapScene::toSceneF(const QRectF &rect) const
{
    QPolygonF polygon;
    polygon += toScene(rect.topLeft());
    polygon += toScene(rect.topRight());
    polygon += toScene(rect.bottomRight());
    polygon += toScene(rect.bottomLeft());
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

void HeightMapScene::heightMapPainted(const QRegion &region)
{
    foreach (QRect r, region.rects()) {
        QRectF sceneRect = boundingRect(r);
        update(sceneRect);
    }
}

void HeightMapScene::heightMapDisplayStyleChanged(int style)
{
    mHeightMapItem->setDisplayStyle(style
                                    ? HeightMapItem::FlatStyle
                                    : HeightMapItem::MeshStyle);
}

/////

HeightMapView::HeightMapView(QWidget *parent) :
    BaseGraphicsView(AlwaysGL, parent),
    mRecenterScheduled(false)
{
    QVector<qreal> zf = zoomable()->zoomFactors();
    zf += 8.0;
    zoomable()->setZoomFactors(zf);
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

void HeightMapView::setScene(HeightMapScene *scene)
{
    BaseGraphicsView::setScene(scene);

    HMMiniMapItem *mMiniMapItem = new HMMiniMapItem(scene);
    addMiniMapItem(mMiniMapItem);
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

/////

LotsLoader::LotsLoader(QObject *parent) :
    QObject(parent)
{
    connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
            SLOT(mapLoaded(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
            SLOT(mapFailedToLoad(MapInfo*)));
}

LotsLoader::~LotsLoader()
{
    foreach (LoadInfo info, mLoaded)
        MapManager::instance()->removeReferenceToMap(info.mMapInfo);
}

void LotsLoader::loadLot(WorldCellLot *lot)
{
    LoadInfo info;
    info.mLot = lot;
    info.mMapInfo = MapManager::instance()->loadMap(
                lot->mapName(), QString(), true, MapManager::PriorityLow);
    if (info.mMapInfo) {
        if (info.mMapInfo->isLoading())
            mLoading += info;
        else {
            Q_ASSERT(info.mMapInfo->map() != 0);
            mLoaded += info;
            MapManager::instance()->addReferenceToMap(info.mMapInfo);
            emit lotLoaded(lot, info.mMapInfo);
        }
    } else {
        emit lotFailedToLoad(lot);
    }
}

void LotsLoader::mapLoaded(MapInfo *mapInfo)
{
    for (int i = 0; i < mLoading.size(); i++) {
        LoadInfo info = mLoading[i];
        if (mapInfo == info.mMapInfo) {
            mLoaded += info;
            mLoading.takeAt(i);
            --i;
            MapManager::instance()->addReferenceToMap(info.mMapInfo);
            emit lotLoaded(info.mLot, info.mMapInfo);
        }
    }
}

void LotsLoader::mapFailedToLoad(MapInfo *mapInfo)
{
    for (int i = 0; i < mLoading.size(); i++) {
        LoadInfo info = mLoading[i];
        if (mapInfo == info.mMapInfo) {
            mLoading.takeAt(i);
            --i;
            emit lotFailedToLoad(info.mLot);
        }
    }
}
