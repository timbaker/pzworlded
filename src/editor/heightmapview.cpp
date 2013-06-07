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
    mDisplayStyle(MeshStyle),
    mTextureID(0),
    mTileset(0)
{
    mDisplayStyle = Preferences::instance()->heightMapDisplayStyle()
            ? FlatStyle : MeshStyle;

    setFlag(ItemUsesExtendedStyleOption);

    if (mScene->mapComposite()) {
        foreach (Tiled::Tileset *ts, mScene->mapComposite()->map()->tilesets()) {
            if (ts->name() == QLatin1String("blends_natural_01")) {
                mTileset = ts;
                break;
            }
        }
    }
}

QRectF HeightMapItem::boundingRect() const
{
    return mScene->boundingRect(mScene->worldBounds());
}

void HeightMapItem::loadGLTextures()
{
    QImage t;
    QImage b;

    if (b.load(QLatin1String("C:/Programming/heightmap/tex_blends_natural_01.png"))) {
        QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
        QPainter painter(&fixedImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(fixedImage.rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage( 0, 0, b);
        painter.end();
        b = fixedImage;

        t = QGLWidget::convertToGLFormat( b );
        GLuint texture;
        glGenTextures( 1, &texture );
        glBindTexture( GL_TEXTURE_2D, texture );
        glTexImage2D( GL_TEXTURE_2D, 0, 4, t.width(), t.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, t.bits() );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

        mTextureID = texture;
    }
}

QVector3D HeightMapItem::vertexAt(int x, int y)
{
    QPointF p = mScene->toScene(x, y);
    HeightMap *hm = mScene->hm();
    if (hm->contains(x, y))
        p.ry() -= hm->heightAt(x, y);
    return QVector3D(p);
}

class DrawElements
{
public:
    void clear()
    {
        indices.resize(0);
        vertices.resize(0);
        texcoords.resize(0);
        textureids.resize(0);
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3, const QVector2D &uv4,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4)
    {
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        texcoords << uv1 << uv2 << uv3 << uv4;
        vertices << v1 << v2 << v3 << v4;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void flush()
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
        for (int i = 0; i < textureids.size(); i++) {
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices.constData() + i * 4);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        clear();
    }

    QVector<GLushort> indices;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLuint> textureids;

};

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

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_FLAT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (!mTextureID) {
        loadGLTextures();
    }

    MapComposite *mc = mScene->mapComposite();
    CompositeLayerGroup *lg = mc ? mc->layerGroupForLevel(0) : 0;

    if (mTextureID && mTileset && lg && columns * rows < 100000) {

        DrawElements de;
        float tw = 256.0f, th = 320.0f;
        int cellX = mScene->document()->cell()->pos().x() * 300;
        int cellY = mScene->document()->cell()->pos().y() * 300;
        static QVector<const Tiled::Cell*> cells(40);
        mc->bmpBlender()->flush(QRect(minX-cellX, minY-cellY, columns, rows));
//        lg->prepareDrawing();
        /*if (lg->needsSynch()) */lg->synch();
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < columns; c++) {
                if (!QRect(0,0,300,300).contains(minX+c-cellX,minY+r-cellY)) continue;
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(minX+c-cellX,minY+r-cellY), cells);
                foreach (const Tiled::Cell *cell, cells) {
                    if (!cell->tile || cell->tile->tileset() != mTileset) continue;
                    int tx = cell->tile->id() % mTileset->columnCount();
                    int ty = cell->tile->id() / mTileset->columnCount();
                    de.add(mTextureID,
                           QVector2D((tx*32)/tw, 1-(ty*32.0)/th),
                           QVector2D(((tx+1)*32)/tw, 1-(ty*32)/th),
                           QVector2D(((tx+1)*32)/tw, 1-((ty+1)*32.0)/th),
                           QVector2D((tx*32)/tw, 1-((ty+1)*32.0)/th),
                           vertexAt(minX+c,minY+r), vertexAt(minX+c+1,minY+r),
                           vertexAt(minX+c+1,minY+r+1), vertexAt(minX+c,minY+r+1));
                }
            }
        }
        de.flush();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

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


    MapInfo *mMapInfo = 0;
    if (document()->cell()->mapFilePath().isEmpty())
        mMapInfo = MapManager::instance()->getEmptyMap();
    else {
        mMapInfo = MapManager::instance()->loadMap(document()->cell()->mapFilePath());
        if (!mMapInfo) {
            mMapInfo = MapManager::instance()->getPlaceholderMap(document()->cell()->mapFilePath(), 300, 300);
        }
    }
    if (!mMapInfo) {
        return;
    }
    mMapComposite = new MapComposite(mMapInfo, Tiled::Map::LevelIsometric);

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
