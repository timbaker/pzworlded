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

#include "lotpackwindow.h"
#include "ui_lotpackwindow.h"

#include "chunkmap.h"
#include "preferences.h"
#include "progress.h"
#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "tilelayer.h"
#include "zlevelrenderer.h"

#include <qmath.h>
#include <QDebug>
#include <QFileDialog>
#include <QSettings>

using namespace Tiled;

/////

LotPackLayerGroup::LotPackLayerGroup(IsoWorld *world, Map *map, int level) :
    ZTileLayerGroup(map, level),
    mWorld(world)
{
    mGrids.resize(20); // FIXME: max number of tiles in any cell on this level
    for (int x = 0; x < mGrids.size(); x++)
        mGrids[x] = new SparseTileGrid(mWorld->CurrentCell->ChunkMap->getWidthInTiles(),
                                       mWorld->CurrentCell->ChunkMap->getWidthInTiles());
}

QRect LotPackLayerGroup::bounds() const
{
    return mWorld->tileBounds();
}

QMargins LotPackLayerGroup::drawMargins() const
{
    return QMargins(0, 128, 64, 0);
}

bool LotPackLayerGroup::orderedCellsAt(const Tiled::MapRenderer *renderer,
                                       const QPoint &point,
                                       QVector<const Cell *> &cells,
                                       QVector<qreal> &opacities) const
{
    cells.resize(0);
    opacities.resize(0);
    int x = point.x() - mWorld->CurrentCell->ChunkMap->getWorldXMinTiles();
    int y = point.y() - mWorld->CurrentCell->ChunkMap->getWorldYMinTiles();
    if (IsoGridSquare *sq = mWorld->CurrentCell->getGridSquare(point.x(), point.y(), level())) {
        foreach (QString tileName, sq->tiles) {
            if (!mScene->mTileByName.contains(tileName)) continue;
            if (Tile *tile = mScene->mTileByName[tileName]) {
                mGrids[cells.size()]->replace(x, y, Cell(tile));
                const Cell *cell = &mGrids[cells.size()]->at(x, y);
                cells += cell;
                opacities += 1.0;
            }
        }
    }
    return !cells.isEmpty();
}

static void sortForRendering(const MapRenderer *renderer, QVector<ZTileRenderInfo> &tileInfo)
{
    int size = tileInfo.size();
    QVector<ZTileRenderInfo> sorted(size);
    sorted.resize(0);

    // Floors Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::Floor:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // West Wall Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::WestWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // North Wall Lowest -> Highest
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::NorthWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // Non-walls
    for (int i = 0; i < size; i++) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::West:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // East Wall Highest -> lowest
    for (int i = size - 1; i >= 0; i--) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::EastWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    // South Wall Highest -> lowest
    for (int i = size - 1; i >= 0; i--) {
        const ZTileRenderInfo& tri = tileInfo[i];
        switch (tri.mOrder) {
        case ZTileRenderOrder::SouthWall:
            sorted += tri;
            break;
        default:
            break;
        }
    }

    tileInfo = sorted;
}

bool LotPackLayerGroup::orderedTilesAt(const MapRenderer *renderer, const QPoint &point, QVector<ZTileRenderInfo> &tileInfos) const
{
    QVector<const Cell *> cells(40);
    QVector<qreal> opacities(40);
    cells.resize(0);
    opacities.resize(0);
    orderedCellsAt(renderer, point, cells, opacities);

    tileInfos.clear();

    for (int i = 0; i < cells.size(); i++) {
        const Cell* cell = cells[i];
        if (cell->isEmpty())
            continue;
        int j = tileInfos.size();
        TileRotation::instance()->rotateTile(*cells[i], renderer->rotation(), tileInfos);
        for (; j < tileInfos.size(); j++) {
            tileInfos[j].mOpacity = opacities[i];
        }
    }

    sortForRendering(renderer, tileInfos);

    return !tileInfos.isEmpty();
}

void LotPackLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    Q_UNUSED(renderer)
    Q_UNUSED(rect)
}

/////

LotPackLayerGroupItem::LotPackLayerGroupItem(LotPackLayerGroup *lg, MapRenderer *renderer) :
    QGraphicsItem(),
    mLayerGroup(lg),
    mRenderer(renderer)
{
    setFlag(ItemUsesExtendedStyleOption);
    mBoundingRect = mRenderer->boundingRect(lg->bounds());
}

QRectF LotPackLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void LotPackLayerGroupItem::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget)
{
    Q_UNUSED(widget)
    mRenderer->drawTileLayerGroup(painter, mLayerGroup, option->exposedRect);
}

/////

LotPackMiniMapItem::LotPackMiniMapItem(LotPackScene *scene) :
    mScene(scene),
    mGridItem(0)
{
    setFlag(ItemHasNoContents);
    setWorld(scene->world());
}

QRectF LotPackMiniMapItem::boundingRect() const
{
    return mBoundingRect;
}

void LotPackMiniMapItem::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *option,
                               QWidget *widget)
{
    Q_UNUSED(painter)
    Q_UNUSED(option)
    Q_UNUSED(widget)
}

void LotPackMiniMapItem::setWorld(IsoWorld *world)
{
    QPen pen(Qt::blue);
    pen.setCosmetic(true);

    if (mGridItem)
        mGridItem->setParentItem(0);
    qDeleteAll(childItems());

    foreach (LotHeader *h, IsoLot::InfoHeaders) {
        foreach (BuildingDef *bdef, h->Buildings) {
            foreach (RoomDef *rdef, bdef->rooms) {
                if (rdef->level) continue;
                foreach (RoomRect *rr, rdef->rects) {
                    QPolygonF p;
                    p += mScene->renderer()->tileToPixelCoords(rr->x, rr->y, rdef->level);
                    p += mScene->renderer()->tileToPixelCoords(rr->x + rr->w, rr->y, rdef->level);
                    p += mScene->renderer()->tileToPixelCoords(rr->x + rr->w, rr->y + rr->h, rdef->level);
                    p += mScene->renderer()->tileToPixelCoords(rr->x, rr->y + rr->h, rdef->level);
                    QGraphicsPolygonItem *item = new QGraphicsPolygonItem(this);
                    item->setPen(pen);
                    item->setBrush(Qt::NoBrush);
                    item->setPolygon(p);
                }
            }
        }
    }

    if (!mGridItem) {
        mGridItem = new IsoWorldGridItem(mScene, this);
    } else {
        mGridItem->setWorld(world);
        mGridItem->setParentItem(this);
    }
}

///// ///// ///// ///// /////


IsoWorldGridItem::IsoWorldGridItem(LotPackScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    setWorld(mScene->world());
}

QRectF IsoWorldGridItem::boundingRect() const
{
    return mBoundingRect;
}

void IsoWorldGridItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *,
                             QWidget *)
{
#if 1
    QColor gridColor(Qt::black);
//    gridColor.setAlpha(128);

    QPen gridPen(gridColor);
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);

    int startX = mScene->world()->MetaGrid->minx;
    int endX = mScene->world()->MetaGrid->maxx + 1;
    int startY = mScene->world()->MetaGrid->miny;
    int endY = mScene->world()->MetaGrid->maxy + 1;


    for (int y = startY; y <= endY; ++y) {
        const QPointF start = mScene->renderer()->tileToPixelCoords(startX * 300, y * 300, 0);
        const QPointF end = mScene->renderer()->tileToPixelCoords(endX * 300, y * 300, 0);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        const QPointF start = mScene->renderer()->tileToPixelCoords(x * 300, startY * 300, 0);
        const QPointF end = mScene->renderer()->tileToPixelCoords(x * 300, endY * 300, 0);
        painter->drawLine(start, end);
    }
#else
    const int tileWidth = 64;
    const int tileHeight = 32;

    QRectF r = option->exposedRect;
    r.adjust(-tileWidth / 2, -tileHeight / 2,
             tileWidth / 2, tileHeight / 2);

    const int startX = qMax(qreal(0), mRenderer->pixelToTileCoords(r.topLeft()).x());
    const int startY = qMax(qreal(0), mRenderer->pixelToTileCoords(r.topRight()).y());
    const int endX = qMin(qreal(mWorld->getWidthInTiles()),
                          mRenderer->pixelToTileCoords(r.bottomRight()).x());
    const int endY = qMin(qreal(mWorld->getHeightInTiles()),
                          mRenderer->pixelToTileCoords(r.bottomLeft()).y());

    QColor gridColor(Qt::black);
//    gridColor.setAlpha(128);

    QPen gridPen(gridColor);
    painter->setPen(gridPen);

    for (int y = startY; y <= endY; ++y) {
        if (y % 300) continue;
        const QPointF start = mRenderer->pixelToTileCoords(startX, (qreal)y);
        const QPointF end = mRenderer->pixelToTileCoords(endX, (qreal)y);
        painter->drawLine(start, end);
    }
    for (int x = startX; x <= endX; ++x) {
        if (x % 300) continue;
        const QPointF start = mRenderer->pixelToTileCoords(x, (qreal)startY);
        const QPointF end = mRenderer->pixelToTileCoords(x, (qreal)endY);
        painter->drawLine(start, end);
    }
#endif
}

void IsoWorldGridItem::setWorld(IsoWorld *world)
{
    Q_UNUSED(world)

    prepareGeometryChange();
    mBoundingRect = mScene->renderer()->boundingRect(mScene->world()->tileBounds());
}

/////

LotPackScene::LotPackScene(QWidget *parent) :
    BaseGraphicsScene(LotPackSceneType, parent),
    mWorld(0),
    mMap(0),
    mRenderer(0),
    mShowRoomDefs(true),
    mDarkRectangle(new QGraphicsRectItem),
    mCurrentLevel(0)
{
    setBackgroundBrush(Qt::lightGray);

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(0.6);

    connect(Preferences::instance(), SIGNAL(highlightCurrentLevelChanged(bool)),
            SLOT(highlightCurrentLevel()));
}

void LotPackScene::setWorld(IsoWorld *world)
{
    if (mWorld) {
        removeItem(mDarkRectangle);
        clear();
        delete mRenderer;
        delete mMap;
        mRenderer = 0;
        mMap = 0;
        mCurrentLevel = 0;
        qDeleteAll(mLayerGroups);
        mLayerGroups.clear();
        mLayerGroupItems.clear();
        mRoomDefGroups.clear();
        setSceneRect(QRectF());
    }
    mWorld = world;

    if (!mWorld)
        return;

    mMap = new Map(Map::LevelIsometric,
                   mWorld->getWidthInTiles(),
                   mWorld->getHeightInTiles(),
                   64, 32);

    mRenderer = new ZLevelRenderer(mMap);

    for (int z = 0; z < IsoChunkMap::MaxLevels/*mWorld->CurrentCell->MaxHeight*/; z++) {
        LotPackLayerGroup *lg = new LotPackLayerGroup(mWorld, mMap, z);
        lg->mScene = this;
        LotPackLayerGroupItem *item = new LotPackLayerGroupItem(lg, mRenderer);
        item->setZValue(z);
        addItem(item);
        mLayerGroups += lg;
        mLayerGroupItems += item;

        QGraphicsRectItem *item2 = new QGraphicsRectItem();
        item2->setZValue(IsoChunkMap::MaxLevels + z);
        mRoomDefGroups += item2;
    }

    QVector<QColor> roomDefColors;
    roomDefColors << QColor(255, 128, 128, 128)
                  << QColor(128, 255, 255, 128)
                  << QColor(128, 255, 128, 128)
                  << QColor(255, 128, 255, 128);

    foreach (LotHeader *h, IsoLot::InfoHeaders) {
        foreach (BuildingDef *bdef, h->Buildings) {
            foreach (RoomDef *rdef, bdef->rooms) {
//                if (rdef->level) continue;
                foreach (RoomRect *rr, rdef->rects) {
                    QPolygonF p;
                    p += mRenderer->tileToPixelCoords(rr->x, rr->y, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x + rr->w, rr->y, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x + rr->w, rr->y + rr->h, rdef->level);
                    p += mRenderer->tileToPixelCoords(rr->x, rr->y + rr->h, rdef->level);
                    QGraphicsPolygonItem *item = new QGraphicsPolygonItem(mRoomDefGroups[rdef->level]);
                    item->setPolygon(p);
                    QColor color = roomDefColors[rdef->level % roomDefColors.size()];
                    if (rdef->name.isEmpty()
                            || rdef->name.contains(QLatin1String("newroom"))
                             || rdef->name.startsWith(QLatin1String("room")))
                        color = QColor(255, 0, 0, 200);
                    item->setBrush(color);

                }
            }
        }
    }

    foreach (QGraphicsItem *item, mRoomDefGroups)
        addItem(item);

    setSceneRect(mRenderer->boundingRect(mWorld->tileBounds()));

    mDarkRectangle->setRect(sceneRect());
    addItem(mDarkRectangle);

    IsoWorldGridItem *gridItem = new IsoWorldGridItem(this);
    gridItem->setZValue(IsoChunkMap::MaxLevels * 2);
    addItem(gridItem);

    mCurrentLevel = 0;
    highlightCurrentLevel();
}

void LotPackScene::setMaxLevel(int max)
{
    Q_UNUSED(max)
    highlightCurrentLevel();
}

void LotPackScene::showRoomDefs(bool show)
{
    if (mShowRoomDefs == show)
        return;
    mShowRoomDefs = show;
    highlightCurrentLevel();
}

void LotPackScene::highlightCurrentLevel()
{
    int max = mWorld->CurrentCell->MaxHeight;
    bool hi = Preferences::instance()->highlightCurrentLevel();
    if (hi)
        max = qMin(max, mCurrentLevel);
    mDarkRectangle->setVisible(hi);
    foreach (LotPackLayerGroupItem *item, mLayerGroupItems) {
        if (hi && item->level() == mCurrentLevel)
            mDarkRectangle->setZValue(item->zValue() - 0.1);
        item->setVisible(item->level() <= max);
    }
    for (int z = 0; z < mRoomDefGroups.size(); z++)
        mRoomDefGroups[z]->setVisible(mShowRoomDefs &&
                                      (!hi || (z == mCurrentLevel)));
}

void LotPackScene::levelAbove()
{
    if (mCurrentLevel < mWorld->CurrentCell->MaxHeight) {
        ++mCurrentLevel;
        highlightCurrentLevel();
    }
}

void LotPackScene::levelBelow()
{
    if (mCurrentLevel > 0) {
        --mCurrentLevel;
        highlightCurrentLevel();
    }
}

/////

LotPackView::LotPackView(QWidget *parent) :
    BaseGraphicsView(PreferenceGL, parent),
    mScene(new LotPackScene(this)),
    mWorld(0),
    mMiniMapItem(0),
    mRecenterScheduled(false)
{
    setScene(mScene);

    QVector<qreal> factors;
    factors << 0.12 << 0.25 << 0.33 << 0.5 << 0.75 << 1.0 << 1.5 << 2.0;
    zoomable()->setZoomFactors(factors);

    zoomable()->setScale(0.25);

}

LotPackView::~LotPackView()
{
}

void LotPackView::setWorld(IsoWorld *world)
{
    if (mWorld) {
    }

    mWorld = world;

    mScene->setWorld(mWorld);

    if (mWorld) {
        if (!mMiniMapItem) {
            mMiniMapItem = new LotPackMiniMapItem(mScene);
            addMiniMapItem(mMiniMapItem);
        } else {
            mMiniMapItem->setWorld(mWorld);
        }

        centerOn(mScene->sceneRect().center());
    } else {
        if (mMiniMapItem) {
            removeMiniMapItem(mMiniMapItem);
            delete mMiniMapItem;
            mMiniMapItem = 0;
        }
    }
}

void LotPackView::scrollContentsBy(int dx, int dy)
{
    if (!mRecenterScheduled) {
        QMetaObject::invokeMethod(this, "recenter", Qt::QueuedConnection);
        mRecenterScheduled = true;
    }

    BaseGraphicsView::scrollContentsBy(dx, dy);
}

#include <QMouseEvent>
bool LotPackView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::MouseMove && mScene && mScene->renderer()) {
        QMouseEvent *e = dynamic_cast<QMouseEvent*>(event);
        QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(mapToScene(e->pos()));
        if (tilePos != mTilePos) {
            mTilePos = tilePos;
            emit tilePositionChanged(mTilePos);
        }
    }

    return BaseGraphicsView::viewportEvent(event);
}

void LotPackView::recenter()
{
    mRecenterScheduled = false;
    if (!mWorld) return;

    QPointF p = mapToScene(viewport()->rect().center());
    QPoint tilePos = mScene->renderer()->pixelToTileCoordsInt(p);
    int wx = qFloor(qreal(tilePos.x()) / IsoChunkMap::ChunksPerWidth);
    int wy = qFloor(qreal(tilePos.y()) / IsoChunkMap::ChunksPerWidth);
    IsoChunkMap *cm = mWorld->CurrentCell->ChunkMap;
    wx = qBound(mWorld->MetaGrid->chunkBounds().left() + cm->ChunkGridWidth / 2, wx,
                mWorld->MetaGrid->chunkBounds().right() + 1 - cm->ChunkGridWidth / 2);
    wy = qBound(mWorld->MetaGrid->chunkBounds().top() + cm->ChunkGridWidth / 2, wy,
                mWorld->MetaGrid->chunkBounds().bottom() + 1 - cm->ChunkGridWidth / 2);
//    qDebug() << "LotPackView::scrollContentsBy" << wx << wy;
    if (wx != cm->WorldX || wy != cm->WorldY) {
        QRegion current = QRect(cm->getWorldXMin(), cm->getWorldYMin(),
                                cm->ChunkGridWidth, cm->ChunkGridWidth);
        QRegion updated = QRect(wx - cm->ChunkGridWidth / 2, wy - cm->ChunkGridWidth / 2,
                                cm->ChunkGridWidth, cm->ChunkGridWidth);

        // Discard old chunks.
        for (const QRect &r : (current - updated)) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (IsoChunk *c = cm->getChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin())) {
                        c->Save(false);
                        cm->setChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin(), 0);
                        delete c;
                    }
                }
            }
        }

        // Shift preserved chunks.
        QVector<IsoChunk*> preserved;
        for (const QRect &r : (current & updated)) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (IsoChunk *c = cm->getChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin())) {
//                        c->wx -= (wx - cm->WorldX);
//                        c->wy -= (wy - cm->WorldY);
                        cm->setChunk(x - cm->getWorldXMin(), y - cm->getWorldYMin(), 0);
                        preserved += c;
                    }
                }
            }
        }

        cm->WorldX = wx;
        cm->WorldY = wy;
        cm->XMinTiles = cm->YMinTiles = -1;

        foreach (IsoChunk *c, preserved) {
            cm->setChunk(c->wx - cm->getWorldXMin(), c->wy - cm->getWorldYMin(), c);
        }

        // Load new chunks;
        for (const QRect &r : (updated - current)) {
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
//                    if (x < mWorld->MetaGrid->minx || x > mWorld->MetaGrid->maxx) continue;
//                    if (y < mWorld->MetaGrid->miny || y > mWorld->MetaGrid->maxy) continue;
                    cm->LoadChunkForLater(x, y, x - cm->getWorldXMin(), y - cm->getWorldYMin());
                }
            }
        }

        cm->UpdateCellCache();

        for (int x = 0; x < cm->Chunks.size(); x++) {
            for (int y = 0; y < cm->Chunks[x].size(); y++) {
                if (IsoChunk *chunk = cm->Chunks[x][y]) {
                    if (chunk->lotheader && !mScene->mHeadersExamined.contains(chunk->lotheader)) {
                        mScene->mHeadersExamined += chunk->lotheader;
                        foreach (QString tileName, chunk->lotheader->tilesUsed) {
                            if (!mScene->mTileByName.contains(tileName)) {
                                if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName)) {
                                    mScene->mTileByName[tileName] = tile;
                                }
                            }
                        }
                    }
                }
            }
        }

        mScene->setMaxLevel(mWorld->CurrentCell->MaxHeight);
    }
}

/////

LotPackWindow::LotPackWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LotPackWindow),
    mWorld(0)
{
    ui->setupUi(this);

    mView = ui->view;

    Preferences *prefs = Preferences::instance();

    ui->actionCenterOnCell->setEnabled(false);
    ui->actionShowMiniMap->setChecked(prefs->showMiniMap());
    ui->actionShowRoomDefs->setChecked(true);
    ui->actionHighlightCurrentLevel->setChecked(prefs->highlightCurrentLevel());

    // Make sure Ctrl+= also works for zooming in
    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    keys += QKeySequence(tr("="));
    ui->actionZoomIn->setShortcuts(keys);
    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);

    mView->zoomable()->connectToComboBox(ui->scaleCombo);
    connect(mView->zoomable(), SIGNAL(scaleChanged(qreal)), SLOT(updateZoom()));

    connect(ui->actionOpen_World, SIGNAL(triggered()), SLOT(open()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

    connect(ui->actionZoomIn, SIGNAL(triggered()), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()), SLOT(zoomOut()));
    connect(ui->actionZoomNormal, SIGNAL(triggered()), SLOT(zoomNormal()));

    connect(ui->actionShowMiniMap, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowMiniMap(bool)));
    connect(ui->actionShowRoomDefs, SIGNAL(toggled(bool)),
            mView->scene(), SLOT(showRoomDefs(bool)));

    connect(ui->actionHighlightCurrentLevel, SIGNAL(toggled(bool)),
            prefs, SLOT(setHighlightCurrentLevel(bool)));
    connect(ui->actionLevelUp, SIGNAL(triggered()), mView->scene(), SLOT(levelAbove()));
    connect(ui->actionLevelDown, SIGNAL(triggered()), mView->scene(), SLOT(levelBelow()));

    connect(mView, SIGNAL(tilePositionChanged(QPoint)), SLOT(tilePositionChanged(QPoint)));

    ui->actionRecent->setVisible(false);
    setRecentMenu();

    TileMetaInfoMgr::instance()->loadTilesets();
}

LotPackWindow::~LotPackWindow()
{
    delete mWorld;
    delete ui;
}

void LotPackWindow::addRecentDirectory(const QString &f)
{
    QSettings settings;
    QStringList recentList = settings.value(QLatin1String("LotPackWindow/RecentList")).toStringList();
    QFileInfo info(f);
    QStringList newRecentList;
    foreach (QString recent, recentList) {
        QFileInfo recentInfo(recent);
        if (recentInfo != info && recentInfo.exists())
            newRecentList += recent;
    }
    newRecentList.prepend(f);
    settings.setValue(QLatin1String("LotPackWindow/RecentList"), newRecentList);
}

void LotPackWindow::setRecentMenu()
{
    QSettings settings;
    QStringList recentList = settings.value(QLatin1String("LotPackWindow/RecentList")).toStringList();

    bool sawRecent = false;
    QAction *separatorAfterRecent = 0;
    foreach (QAction *action, ui->menuFile->actions()) {
        if (action == ui->actionRecent)
            sawRecent = true;
        else if (sawRecent) {
            if (action->isSeparator()) {
                separatorAfterRecent = action;
                break;
            }
            delete action;
        }
    }

    foreach (QString recent, recentList) {
        QFileInfo info(recent);
        QAction *action = new QAction(info.fileName(), ui->menuFile);
        if (info.exists()) {
            action->setData(recent);
            connect(action, SIGNAL(triggered()), SLOT(openRecent()));
        } else
            action->setEnabled(false);
        ui->menuFile->insertAction(separatorAfterRecent, action);
    }
}

void LotPackWindow::closeEvent(QCloseEvent *e)
{
    closeWorld();
    e->accept();
}

void LotPackWindow::open()
{
#if 1
    QSettings settings;
    QString recent = settings.value(QLatin1String("LotPackWindow/LastOpenDirectory")).toString();
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose a world's directory"), recent);
    if (f.isEmpty())
        return;

    settings.setValue(QLatin1String("LotPackWindow/LastOpenDirectory"), f);

#else
    IsoWorld *world = new IsoWorld(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/Project Zomboid Latest/media/maps/Muldraugh, KY"));
#endif
    open(f);
}

void LotPackWindow::openRecent()
{
    QAction *sender = dynamic_cast<QAction*>(this->sender());
    if (sender)
        open(sender->data().toString());
}

void LotPackWindow::open(const QString &directory)
{
    PROGRESS progress(tr("Loading %1").arg(QFileInfo(directory).fileName()), this);

    qDeleteAll(IsoLot::InfoHeaders);
    IsoLot::InfoHeaders.clear();

    CellLoader::instance()->reset();

    IsoWorld *world = new IsoWorld(directory);
    world->init();
    mView->setWorld(world);
    if (mWorld)
        delete mWorld;
    mWorld = world;

    addRecentDirectory(directory);
    setRecentMenu();
}

void LotPackWindow::closeWorld()
{
    qDeleteAll(IsoLot::InfoHeaders);
    IsoLot::InfoHeaders.clear();

    CellLoader::instance()->reset();

    mView->setWorld(0);
    if (mWorld) {
        delete mWorld;
        mWorld = 0;
    }
}

void LotPackWindow::zoomIn()
{
    mView->zoomable()->zoomIn();
}

void LotPackWindow::zoomOut()
{
    mView->zoomable()->zoomOut();
}

void LotPackWindow::zoomNormal()
{
    mView->zoomable()->resetZoom();
}

void LotPackWindow::updateZoom()
{
    const qreal scale = mView->zoomable()->scale();

    ui->actionZoomIn->setEnabled(mView->zoomable()->canZoomIn());
    ui->actionZoomOut->setEnabled(mView->zoomable()->canZoomOut());
    ui->actionZoomNormal->setEnabled(scale != 1);
}

void LotPackWindow::tilePositionChanged(const QPoint &tilePos)
{
    if (mWorld->tileBounds().contains(tilePos)) {
        int x = qFloor(tilePos.x() / 300.0);
        int y = qFloor(tilePos.y() / 300.0);
        ui->coords->setText(tr("Cell %1,%2 World %3,%4").arg(x).arg(y).arg(tilePos.x()).arg(tilePos.y()));
    } else
        ui->coords->setText(QString());
}
