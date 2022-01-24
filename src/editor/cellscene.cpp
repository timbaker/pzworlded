/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "cellscene.h"

#include "bmpblender.h"
#include "celldocument.h"
#include "cellview.h"
#include "mainwindow.h"
#include "mapbuildings.h"
#include "mapcomposite.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "preferences.h"
#include "progress.h"
#include "scenetools.h"
#include "tilesetmanager.h"
#include "undoredo.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "InGameMap/ingamemapscene.h"

#include "isometricrenderer.h"
#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"
#include "zlevelrenderer.h"

#include "BuildingEditor/buildingtmx.h"

#include <qmath.h>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsSceneEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QOpenGLFunctions>
#include <QStyleOptionGraphicsItem>
#include <QUrl>
#include <QUndoStack>

using namespace Tiled;

///// ///// ///// ///// /////

class CellGridItem : public QGraphicsItem
{
public:
    CellGridItem(CellScene *scene, QGraphicsItem *parent = 0)
        : QGraphicsItem(parent)
        , mScene(scene)
    {
        setAcceptedMouseButtons(Qt::MouseButton::NoButton);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
        updateBoundingRect();
    }

    QRectF boundingRect() const
    {
        return mBoundingRect;
    }

    void paint(QPainter *painter,
                         const QStyleOptionGraphicsItem *option,
                         QWidget *)
    {
        QColor gridColor = Preferences::instance()->gridColor();
        mScene->renderer()->drawGrid(painter, option->exposedRect, gridColor,
                                     mScene->document()->currentLevel());
    }

    void updateBoundingRect()
    {
        QRectF boundsF;
        if (mScene->renderer()) {
            QRect bounds(QPoint(), mScene->map()->size());
            boundsF = mScene->renderer()->boundingRect(bounds, mScene->document()->currentLevel());
        }
        if (boundsF != mBoundingRect) {
            prepareGeometryChange();
            mBoundingRect = boundsF;
        }
    }

private:
    CellScene *mScene;
    QRectF mBoundingRect;
};

///// ///// ///// ///// /////

CellMiniMapItem::CellMiniMapItem(CellScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(scene)
    , mCell(scene->cell())
    , mMapImage(0)
{
    setAcceptedMouseButtons(Qt::MouseButton::NoButton);

    updateCellImage();

    mLotImages.resize(mCell->lots().size());
    for (int i = 0; i < mCell->lots().size(); i++)
        updateLotImage(i);

    updateBoundingRect();

    connect(MapImageManager::instance(), SIGNAL(mapImageChanged(MapImage*)),
            SLOT(mapImageChanged(MapImage*)));
    connect(mScene, SIGNAL(sceneRectChanged(QRectF)), SLOT(sceneRectChanged(QRectF)));
}

QRectF CellMiniMapItem::boundingRect() const
{
    return mBoundingRect;
}

void CellMiniMapItem::paint(QPainter *painter,
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

void CellMiniMapItem::updateCellImage()
{
    mMapImage = 0;
    mMapImageBounds = QRect();

    if (!mCell->mapFilePath().isEmpty()) {
        mMapImage = MapImageManager::instance()->getMapImage(mCell->mapFilePath());
        if (mMapImage) {
            qreal tileScale = mScene->renderer()->boundingRect(QRect(0,0,1,1)).width() / (qreal)mMapImage->tileSize().width();
            QPointF offset = mMapImage->tileToImageCoords(0, 0) / mMapImage->scale() * tileScale;
            mMapImageBounds = QRectF(mScene->renderer()->tileToPixelCoords(0.0, 0.0) - offset,
                                     mMapImage->image().size() / mMapImage->scale() * tileScale);
        }
    }
}

void CellMiniMapItem::updateLotImage(int index)
{
    WorldCellLot *lot = mCell->lots().at(index);
    MapImage *mapImage = MapImageManager::instance()->getMapImage(lot->mapName()/*, mapFilePath()*/);
    if (mapImage) {
        qreal tileScale = mScene->renderer()->boundingRect(QRect(0,0,1,1)).width() / (qreal)mapImage->tileSize().width();
        QPointF offset = mapImage->tileToImageCoords(0, 0) / mapImage->scale() * tileScale;
        QRectF bounds = QRectF(mScene->renderer()->tileToPixelCoords(lot->x(), lot->y(), lot->level()) - offset,
                               mapImage->image().size() / mapImage->scale() * tileScale);
        mLotImages[index].mBounds = bounds;
        mLotImages[index].mMapImage = mapImage;
    } else {
        mLotImages[index].mBounds = QRectF();
        mLotImages[index].mMapImage = 0;
    }
}

void CellMiniMapItem::updateBoundingRect()
{
    QRectF bounds = mScene->renderer()->boundingRect(QRect(0, 0, 300, 300));

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

void CellMiniMapItem::lotAdded(int index)
{
    mLotImages.insert(index, LotImage());
    updateLotImage(index);
    updateBoundingRect();
    update();
}

void CellMiniMapItem::lotRemoved(int index)
{
    mLotImages.remove(index);
    updateBoundingRect();
    update();
}

void CellMiniMapItem::lotMoved(int index)
{
    updateLotImage(index);
    updateBoundingRect();
    update();
}

void CellMiniMapItem::cellContentsAboutToChange()
{
    mLotImages.clear();
}

void CellMiniMapItem::cellContentsChanged()
{
    updateCellImage();

    mLotImages.resize(mCell->lots().size());
    for (int i = 0; i < mCell->lots().size(); i++)
        updateLotImage(i);

    updateBoundingRect();
    update();
}

// cellContentsChanged -> CellScene::loadMap -> sceneRectChanged
void CellMiniMapItem::sceneRectChanged(const QRectF &sceneRect)
{
    Q_UNUSED(sceneRect)
    updateCellImage();
    for (int i = 0; i < mLotImages.size(); i++)
        updateLotImage(i);
}

void CellMiniMapItem::mapImageChanged(MapImage *mapImage)
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

TilesetTexturesPerContext::~TilesetTexturesPerContext()
{
    if (mContext != nullptr) {
        QOpenGLContext *context = mContext->shareContext() ? mContext->shareContext() : mContext;
        if (context->makeCurrent(context->surface())) {
            for (TilesetTexture *texture : qAsConst(mTextures)) {
#if TILESET_TEXTURE_GL
                texture->mTexture->destroy();
                delete texture->mTexture;
#else
                if (texture->mID != -1) {
                    GLuint id = texture->mID;
                    mContext->functions()->glDeleteTextures(1, &id);
                }
#endif
            }
        } else {
            qDebug() << "~TilesetTexturesPerContext() failed to set OpenGL context";
        }
    }
    qDeleteAll(mTextures);
}

/////

#include <QSurface>

TilesetTexture *TilesetTextures::get(const QString& tilesetName, const QList<Tiled::Tileset*> &tilesets)
{
    if (false) return nullptr;

    if (mConnected == false) {
        mConnected = true;
        connect(Tiled::Internal::TilesetManager::instance(), &Internal::TilesetManager::tilesetChanged, this, &TilesetTextures::tilesetChanged);
    }

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (context->shareContext() != nullptr)
        context = context->shareContext();

    TilesetTexturesPerContext *contextTextures = mContextToTextures[context];
    if (contextTextures == nullptr) {
        qDebug() << "TilesetTextures::get() added context" << context;
        contextTextures = new TilesetTexturesPerContext();
        contextTextures->mContext = context;
        mContextToTextures[context] = contextTextures;
        connect(context, &QOpenGLContext::aboutToBeDestroyed, this, &TilesetTextures::aboutToBeDestroyed);
    }

//    if (QSurface *surface = context->surface()) {
//        qDebug() << surface->format() << "r=" << surface->format().redBufferSize() << "g=" << surface->format().greenBufferSize() << "b=" << surface->format().blueBufferSize();
//    }

    if (contextTextures->mChanged.contains(tilesetName)) {
        contextTextures->mChanged.remove(tilesetName);
        contextTextures->mMissing.remove(tilesetName);

        if (contextTextures->mTextureMap.contains(tilesetName)) {
            TilesetTexture *texture = contextTextures->mTextureMap[tilesetName];
#if TILESET_TEXTURE_GL == 0
            if (Tiled::Tileset *tileset = findTileset(tilesetName, tilesets)) {
                if ((texture->mID != -1) && (texture->mChangeCount == tileset->changeCount()))
                    return texture;
                texture->mChangeCount = tileset->changeCount();
                const QImage image = tileset->image().convertToFormat(QImage::Format_RGBA8888);
                const uchar *pixels = image.constBits();
                if (texture->mID == -1) {
                    GLuint id;
                    context->functions()->glGenTextures(1, &id);
                    texture->mID = id;
                }
                context->functions()->glActiveTexture(GL_TEXTURE0);
                Q_ASSERT(context->functions()->glGetError() == 0);
                context->functions()->glBindTexture(GL_TEXTURE_2D, texture->mID);
                Q_ASSERT(context->functions()->glGetError() == 0);
                context->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                context->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//                GLint swizzleMask[] = {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA}; // FIXME: red/blue swapped
//                context->functions()->glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
                context->functions()->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                context->functions()->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileset->image().width(), tileset->image().height(), 0, GL_RGBA8, GL_UNSIGNED_BYTE, pixels);
                Q_ASSERT(context->functions()->glGetError() == 0);
                context->functions()->glBindTexture(GL_TEXTURE_2D, 0);
                qDebug() << "TilesetTextures UPLOAD" << tilesetName << image << image.format() << "id=" << texture->mID;
            }
#else
            texture->mTexture->destroy();
            texture->mTexture->create();
            if (Tiled::Tileset *tileset = findTileset(tilesetName, tilesets)) {
                texture->mTexture->setData(tileset->image(), QOpenGLTexture::DontGenerateMipMaps);
            }
#endif
            return texture;
        }
    }

    if (contextTextures->mMissing.contains(tilesetName)) {
        return nullptr;
    }

    TilesetTexture *texture = contextTextures->mTextureMap.contains(tilesetName) ? contextTextures->mTextureMap[tilesetName] : nullptr;
    if (texture == nullptr) {
//        const QList<Tileset *> tilesets = Tiled::Internal::TilesetManager::instance()->tilesets();
        if (Tiled::Tileset *tileset = findTileset(tilesetName, tilesets)) {
            if (tileset->image().isNull()) {
                // The texture may still be loading
                qDebug() << "TilesetTextures MISSING" << tilesetName;
                contextTextures->mMissing += tilesetName;
                return nullptr;
            }
            texture = new TilesetTexture();
            texture->mChangeCount = tileset->changeCount();
#if TILESET_TEXTURE_GL == 0
            const QImage image = tileset->image().convertToFormat(QImage::Format_RGBA8888);
            const uchar *pixels = image.constBits();
//            uchar *pixels2 = new uchar[image.width() * image.height() * 4];
//            memset(pixels2, 0xF0, image.width() * image.height() * 4);
            if (texture->mID == -1) {
                GLuint id;
                context->functions()->glGenTextures(1, &id);
                Q_ASSERT(context->functions()->glGetError() == 0);
                texture->mID = id;
            }
            context->functions()->glActiveTexture(GL_TEXTURE0);
            Q_ASSERT(context->functions()->glGetError() == 0);
            context->functions()->glBindTexture(GL_TEXTURE_2D, texture->mID);
            Q_ASSERT(context->functions()->glGetError() == 0);
            bool mipmap = false; // would need Zac's alpha-padding magic for this to look ok
            context->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
            context->functions()->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//            GLint swizzleMask[] = {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA}; // FIXME: red/blue swapped
//            context->functions()->glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
            context->functions()->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            context->functions()->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileset->image().width(), tileset->image().height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            Q_ASSERT(context->functions()->glGetError() == 0);
            if (mipmap) {
                context->functions()->glGenerateMipmap(GL_TEXTURE_2D);
            }
            context->functions()->glBindTexture(GL_TEXTURE_2D, 0);
            Q_ASSERT(context->functions()->glGetError() == 0);
            qDebug() << "TilesetTextures CREATE" << tilesetName << image << image.format() << " id=" << texture->mID;
            Q_ASSERT(context->functions()->glGetError() == 0);
//            delete [] pixels2;
#else
            texture->mTexture = new QOpenGLTexture(tileset->image(), QOpenGLTexture::DontGenerateMipMaps);
            texture->mTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
            texture->mTexture->setMinificationFilter(QOpenGLTexture::Nearest);
#endif
            contextTextures->mTextureMap[tilesetName] = texture;
            contextTextures->mTextures += texture;
            return texture;
        }
        if (contextTextures->mMissing.contains(tilesetName) == false) {
            qDebug() << "TilesetTextures MISSING" << tilesetName;
            contextTextures->mMissing += tilesetName;
        }
    }
    return texture;
}

Tileset *TilesetTextures::findTileset(const QString &tilesetName, const QList<Tiled::Tileset*> &tilesets)
{
//    const QList<Tileset *> tilesets = Tiled::Internal::TilesetManager::instance()->tilesets();
    for (Tileset *tileset : tilesets) {
        if ((tileset->name() == tilesetName) && (tileset->image().isNull() == false)) {
            return tileset;
        }
    }
    return nullptr;
}

void TilesetTextures::aboutToBeDestroyed()
{
    QObject *sender = this->sender();
    QOpenGLContext *context = reinterpret_cast<QOpenGLContext*>(sender);
    if (mContextToTextures.contains(context) == false) {
        return;
    }
    qDebug() << "TilesetTextures::aboutToBeDestroyed" << context;
    TilesetTexturesPerContext *contextTextures = mContextToTextures[context];
    delete contextTextures;
    mContextToTextures.remove(context);
}

void TilesetTextures::tilesetChanged(Tileset *tileset)
{
    qDebug() << "TilesetTextures CHANGED" << tileset->name();
    for (TilesetTexturesPerContext *contextTextures : qAsConst(mContextToTextures)) {
        contextTextures->mChanged[tileset->name()] = tileset->changeCount();
    }
#if 0
    QOpenGLContext *current = QOpenGLContext::currentContext();

    if (mMissing.contains(tileset->name())) {
        mMissing.remove(tileset->name());
        get(tileset->name());
        return;
    }

    for (TilesetTexturesPerContext *contextTextures : qAsConst(mContextToTextures)) {
        if (contextTextures->mTextureMap.contains(tileset->name()) == false) {
            continue;
        }
        TilesetTexture *texture = contextTextures->mTextureMap[tileset->name()];
        qDebug() << "TilesetTextures::tilesetChanged" << tileset->name() << contextTextures->mContext;
        if (contextTextures->mContext->makeCurrent(contextTextures->mContext->surface())) {
            texture->mTexture->destroy();
            texture->mTexture->create();
            texture->mTexture->setData(tileset->image(), QOpenGLTexture::DontGenerateMipMaps);
            texture->mChanged = false;
        } else {
            texture->mChanged = true;
        }
    }

    if (current != nullptr) {
        current->makeCurrent(current->surface());
    }
#endif
}

static TilesetTextures TILESET_TEXTURES;

LayerGroupVBO::LayerGroupVBO()
    : mLayerGroup(nullptr)
{
    mTiles.fill(nullptr);
}

LayerGroupVBO::~LayerGroupVBO()
{
    mDestroying = true;

    if (mCreated == false) {
        qDeleteAll(mTiles);
        mTiles.fill(nullptr);
        for (int i = 0; i < mMapCompositeVBO->mLayerVBOs.size(); i++) {
            if (mMapCompositeVBO->mLayerVBOs[i] == this) {
                mMapCompositeVBO->mLayerVBOs[i] = nullptr;
                break;
            }
        }
        return;
    }
    if (mContext != nullptr) {
        if (mContext->makeCurrent(mContext->surface())) {
            qDeleteAll(mTiles);
            mTiles.fill(nullptr);
            for (int i = 0; i < mMapCompositeVBO->mLayerVBOs.size(); i++) {
                if (mMapCompositeVBO->mLayerVBOs[i] == this) {
                    mMapCompositeVBO->mLayerVBOs[i] = nullptr;
                    break;
                }
            }
            return;
        }
    }
    Q_ASSERT(false);
}

void LayerGroupVBO::paint(QPainter *painter, Tiled::MapRenderer *renderer, const QRectF& exposedRect)
{
    if (mDestroying) {
        return;
    }

    painter->beginNativePainting();

    if (mCreated == false || mContext != QOpenGLContext::currentContext()) {
        initializeOpenGLFunctions();
    }

    if (mContext == nullptr) {
        QOpenGLContext *context = QOpenGLContext::currentContext();
//        if (context->shareContext() != nullptr)
//            context = context->shareContext();
        mContext = context;
        connect(mContext, &QOpenGLContext::aboutToBeDestroyed, this, &LayerGroupVBO::aboutToBeDestroyed);
    }

    bool pushPop = true;
    if (pushPop) {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    }

#define PZ_OPENGL_WIDGET 1
#if PZ_OPENGL_WIDGET
    // Set the model-view-projection matrices for QGraphicsScene.
    // This isn't needed when using QGLWidget, but is with QOpenGLWidget.
    QRect viewport = painter->viewport();
//    glViewport(viewport.x(), viewport.y(), viewport.width(), viewport.height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
//    glOrtho(-viewport.width() / 2, viewport.width() / 2, -viewport.height() / 2, viewport.height() / 2, -1.f, 1.f);
    glOrtho(0.f, viewport.width(), viewport.height(), 0, -1.f, 1.f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    QTransform xfrm = painter->transform();
    float scaleX = xfrm.m11();
    float scaleY = xfrm.m22();
    float translateX = xfrm.m31();
    float translateY = xfrm.m32();
    glTranslatef(translateX, translateY, 0.0f);
    glScalef(scaleX, scaleY, 1.0f);
#endif

    paint2(painter, renderer, exposedRect);

    if (pushPop) {
        glPopAttrib();
        glPopClientAttrib();
    }

    painter->endNativePainting();
//    painter->restore();
}

static inline bool isLotVisible(MapComposite *lot)
{
    return lot->isGroupVisible() && lot->isVisible() && (lot->isHiddenDuringDrag() == false);
};

void LayerGroupVBO::paint2(QPainter *painter, Tiled::MapRenderer *renderer, const QRectF& exposedRect)
{
    Q_UNUSED(painter)

//    QOpenGLContext *context = QOpenGLContext::currentContext();
//    if (context->shareContext() != nullptr)
//        context = context->shareContext();

    MapComposite *mapComposite = mMapCompositeVBO->mMapComposite/*mLayerGroup->owner()*/;
    if (mapComposite->changeCount() != mChangeCount) {
        mChangeCount = mapComposite->changeCount();
        if (mCreated) {
            qDebug() << "LayerGroupVBO recreate";
            mCreated = false;
            for (auto* vboTiles : mTiles) {
                if (vboTiles != nullptr) {
                    vboTiles->mCreated = false;
                    vboTiles->mGathered = false;
                    vboTiles->mTiles.clear();
                    vboTiles->mTileCount.fill(0);
                    vboTiles->mTileFirst.fill(-1);
                }
            }
        }
    }

    QList<VBOTiles*> exposedTiles;
    gatherTiles(renderer, exposedRect, exposedTiles);

    if (mCreated == false) {
        mCreated = true;
#if 0
        GLint maxVertices, maxIndices;
        glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &maxIndices);
        glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &maxVertices);
        qDebug() << "GL_MAX_ELEMENTS_INDICES" << maxIndices << "GL_MAX_ELEMENTS_VERTICES" << maxVertices;
#endif
    }

    for (VBOTiles *vboTiles : qAsConst(exposedTiles)) {
//        VBOTiles *vboTiles = mTiles[vxy.x() + vxy.y() * VBO_PER_CELL];
//        if (vboTiles == nullptr)
//            continue;
        if (vboTiles->mGathered == false)
            continue;
        if (vboTiles->mCreated)
            continue;
        vboTiles->mCreated = true;
        const QList<VBOTile>& tiles = vboTiles->mTiles;
        if (tiles.isEmpty()) {
            continue;
        }
        if (vboTiles->mIndexBuffer.isCreated() == false) {
            if (vboTiles->mIndexBuffer.create() == false) Q_ASSERT(false);
            vboTiles->mIndexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        }
        if (vboTiles->mIndexBuffer.bind() == false) Q_ASSERT(false);
        GLuint *indices = new GLuint[tiles.size() * 4];
        for (int i = 0; i < tiles.size() * 4; i++) {
            indices[i] = i;
        }
        vboTiles->mIndexBuffer.allocate(indices, tiles.size() * 4 * sizeof(GLuint));
        delete[] indices;

        if (vboTiles->mVertexBuffer.isCreated() == false) {
            if (vboTiles->mVertexBuffer.create() == false) Q_ASSERT(false);
            vboTiles->mVertexBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
        }
        if (vboTiles->mVertexBuffer.bind() == false) Q_ASSERT(false);
        // x, y, u, v
        GLfloat *vertices = new GLfloat[tiles.size() * 4 * 4];
        for (int i = 0; i < tiles.size(); i++) {
            const auto& tile = tiles[i];
            int n = i * 4 * 4;
            const QRect& bounds = tile.mRect;
            const Tile::UVST uvst = tile.mAtlasUVST;
            float u0 = uvst.u;
            float v0 = uvst.v;
            float u1 = uvst.s;
            float v1 = uvst.t;

            vertices[n++] = bounds.x();
            vertices[n++] = bounds.y();
            vertices[n++] = u0;
            vertices[n++] = v0;

            vertices[n++] = bounds.right() + 1;
            vertices[n++] = bounds.y();
            vertices[n++] = u1;
            vertices[n++] = v0;

            vertices[n++] = bounds.right() + 1;
            vertices[n++] = bounds.bottom() + 1;
            vertices[n++] = u1;
            vertices[n++] = v1;

            vertices[n++] = bounds.left();
            vertices[n++] = bounds.bottom() + 1;
            vertices[n++] = u0;
            vertices[n++] = v1;
        }
        vboTiles->mVertexBuffer.allocate(vertices, tiles.size() * 4 * 4 * sizeof(GL_FLOAT));
        delete[] vertices;

//            qDebug() << "mTiles.size() == " << tiles.size();
    }

    if (isEmpty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE2);
    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);


    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    bool wireframe = false;
    if (wireframe) {
        glLineWidth(1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    GLuint textureID = 0;

    bool visibleLayers[100];
    qreal layerOpacity[100];
    int layerCount = mLayerGroup->layers().size();
    for (int i = 0; i < layerCount; i++) {
        TileLayer *tl = mLayerGroup->layers()[i];
        // FIXME: submaps also
        visibleLayers[i] = mLayerGroup->isLayerVisible(tl);
        layerOpacity[i] = mLayerGroup->layerOpacity(tl);
    }

    bool drawAll = false;
    if (drawAll) {
        {
            for (VBOTiles *vboTiles : mTiles) {
                if (vboTiles == nullptr)
                    continue;
                if (vboTiles->mCreated == false)
                    continue;
                QList<VBOTile>& tiles = vboTiles->mTiles;
                if (tiles.isEmpty())
                    continue;
                if (vboTiles->mIndexBuffer.bind() == false) Q_ASSERT(false);
                if (vboTiles->mVertexBuffer.bind() == false) Q_ASSERT(false);
                glVertexPointer(2, GL_FLOAT, 4 * sizeof(GL_FLOAT), 0);
                glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(GL_FLOAT), (void*)(2 * sizeof(GL_FLOAT)));

                for (int i = 0; i < tiles.size(); i++) {
                    GLuint start = i * 4;
                    GLuint end = start + 4 - 1;
                    GLuint count = 4;
                    if (tiles[i].mTexture == nullptr) {
                        tiles[i].mTexture = TILESET_TEXTURES.get(tiles[i].mTilesetName, mMapCompositeVBO->mUsedTilesets);
                    }
#if TILESET_TEXTURE_GL == 0
                    if (tiles[i].mTexture == nullptr || tiles[i].mTexture->mID == -1)
                        continue;
                    if (textureID != tiles[i].mTexture->mID) {
                        glBindTexture(GL_TEXTURE_2D, tiles[i].mTexture->mID);
                        textureID = tiles[i].mTexture->mID;
                    }
                    glDrawRangeElements(GL_QUADS, start, end, count, GL_UNSIGNED_INT, (void*)(start * sizeof(GLuint)));
#else
                    if (tiles[i].mTexture == nullptr || tiles[i].mTexture->mTexture->isCreated() == false)
                        continue;
                    tiles[i].mTexture->mTexture->bind();
                    glDrawRangeElements(GL_QUADS, start, end, count, GL_UNSIGNED_INT, (void*)(start * sizeof(GLuint)));
#endif
                }
            }
        }
    } else {
        qreal opacity = 1.0f;
        glColor4f(1.f, 1.f, 1.f, opacity);

        MapComposite *mapComposite = mLayerGroup->owner();
        QRegion suppressRgn;
        if (mapComposite->levelRecursive() + mLayerGroup->level() == mapComposite->root()->suppressLevel())
            suppressRgn = mapComposite->root()->suppressRegion();

        QList<QPoint> squares;
        getSquaresInRect(renderer, exposedRect, squares);
        {
            VBOTiles *currentTiles = nullptr;
            for (const QPoint& square : qAsConst(squares)) {
                VBOTiles *vboTiles = (currentTiles != nullptr && currentTiles->mBounds.contains(square)) ? currentTiles : getTilesFor(square, false);
                if (vboTiles == nullptr)
                    continue;
                if (vboTiles->mCreated == false)
                    continue;
                QList<VBOTile>& tiles = vboTiles->mTiles;
                if (tiles.isEmpty())
                    continue;
                if (currentTiles != vboTiles) {
                    if (currentTiles != nullptr) {
                        QPointF screenOrigin = renderer->tileToPixelCoords(currentTiles->mBounds.topLeft() + QPointF(0.5f, 1.5f), mLayerGroup->level());
                        glMatrixMode(GL_MODELVIEW);
                        glTranslatef(-screenOrigin.x(), -screenOrigin.y(), 0.0f);
                    }
                    currentTiles = vboTiles;
                    if (vboTiles->mIndexBuffer.bind() == false) Q_ASSERT(false);
                    if (vboTiles->mVertexBuffer.bind() == false) Q_ASSERT(false);
                    if (true) {
                        QPointF screenOrigin = renderer->tileToPixelCoords(vboTiles->mBounds.topLeft() + QPointF(0.5f, 1.5f), mLayerGroup->level());
                        glMatrixMode(GL_MODELVIEW);
                        glTranslatef(screenOrigin.x(), screenOrigin.y(), 0.0f);
                    }
                    glVertexPointer(2, GL_FLOAT, 4 * sizeof(GL_FLOAT), 0);
                    glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(GL_FLOAT), (void*)(2 * sizeof(GL_FLOAT)));
                }
                auto& tileFirst = vboTiles->mTileFirst;
                auto& tileCount = vboTiles->mTileCount;
                {
                    if (vboTiles->mBounds.contains(square.x(), square.y()) == false)
                        continue;
                    int x = (square.x() + 300) % VBO_SQUARES;
                    int y = (square.y() + 300) % VBO_SQUARES;
                    if (tileFirst[x + y * VBO_SQUARES] == -1)
                        continue;
                    for (int i = tileFirst[x+y*VBO_SQUARES], n = i + tileCount[x+y*VBO_SQUARES]; i < n; i++) {
                        if ((i > tileFirst[x+y*VBO_SQUARES]) && suppressRgn.contains(square))
                            continue;
                        GLuint start = i * 4;
                        GLuint end = start + 4 - 1;
                        GLuint count = 4;
                        auto& tile = tiles[i];
                        if ((tile.mHideIfVisible != nullptr) && isLotVisible(tile.mHideIfVisible))
                            continue;
                        if ((tile.mSubMap != nullptr) && (isLotVisible(tile.mSubMap) == false))
                            continue;
                        if ((tile.mLayerIndex >= 0) && (tile.mLayerIndex < layerCount)) {
                            if (visibleLayers[tile.mLayerIndex] == false)
                                continue;
                            if (opacity != layerOpacity[tile.mLayerIndex]) {
                                glColor4f(1.f, 1.f, 1.f, opacity = layerOpacity[tile.mLayerIndex]);
                            }
                        } else {
                            if (opacity != 1.0) {
                                glColor4f(1.f, 1.f, 1.f, opacity = 1.0);
                            }
                        }
                        if (tile.mTexture == nullptr) {
                            tile.mTexture = TILESET_TEXTURES.get(tile.mTilesetName, mMapCompositeVBO->mUsedTilesets);
                        }
#if TILESET_TEXTURE_GL == 0
                        if (tile.mTexture == nullptr || tile.mTexture->mID == -1)
                            continue;
                        if (textureID != tile.mTexture->mID) {
                            glBindTexture(GL_TEXTURE_2D, tile.mTexture->mID);
                            textureID = tile.mTexture->mID;
                        }
                        glDrawRangeElements(GL_QUADS, start, end, count, GL_UNSIGNED_INT, (void*)(start * sizeof(GLuint)));
                        Q_ASSERT(glGetError() == 0);
#else
                        if (tile.mTexture == nullptr || tile.mTexture->mTexture->isCreated() == false)
                            continue;
                        tile.mTexture->mTexture->bind();
                        glDrawRangeElements(GL_QUADS, start, end, count, GL_UNSIGNED_INT, (void*)(start * sizeof(GLuint)));
#endif
                    }
                }
            }
            if (currentTiles != nullptr) {
                currentTiles->mIndexBuffer.release();
                currentTiles->mVertexBuffer.release();
            }
        }
    }
#if 0
    GLuint start = 0;
    GLuint end = mTiles.size() * 4 - 1;
    GLuint count = mTiles.size() * 4;
    glDrawRangeElements(GL_QUADS, start, end, count, GL_UNSIGNED_SHORT, (void*)(0));
#endif

    glBindTexture(GL_TEXTURE_2D, 0);

    if (wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (false) {
        glDisable(GL_TEXTURE_2D);

        glLineWidth(10.0f);

        glBegin(GL_LINES);
        glColor3f(1.0f, 0.0f, 0.0f);
        glVertex3f(0.f, 0.f, 0.f);
        glVertex3f(10000.f, 0.f, 0.f);

        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex3f(0.f, 0.f, 0.f);
        glVertex3f(0.f, 10000.f, 0.f);

        glColor3f(0.0f, 0.0f, 1.0f);
        glVertex3f(0.f, 0.f, 0.f);
        glVertex3f(0.f, 0.f, 10000.f);
        glEnd();

        glColor3f(1.0f, 1.0f, 1.0f);
    }
}

// tangram-es\core\src\util\rasterize.cpp
class Rasterize
{
public:
    struct dvec2 {
        double x;
        double y;
    };

    struct Edge { // An edge between two points; oriented such that y is non-decreasing
        double x0 = 0, y0 = 0;
        double x1 = 0, y1 = 0;
        double dx = 0, dy = 0;

        Edge(dvec2 _a, dvec2 _b)
        {
            if (_a.y > _b.y) { std::swap(_a, _b); }
            x0 = _a.x;
            y0 = _a.y;
            x1 = _b.x;
            y1 = _b.y;
            dx = x1 - x0;
            dy = y1 - y0;
        }
    };

    void scanLine(int _x0, int _x1, int _y)
    {
        for (int x = _x0; x < _x1; x++) {
            if (mPoints.contains({x, _y}) == false) {
                mPoints += {x, _y};
            }
        }
    }

    void scanSpan(Edge _e0, Edge _e1, int _min, int _max)
    {
        // _e1 has a shorter y-span, so we'll use it to limit our y coverage
        int y0 = fmax(_min, floor(_e1.y0));
        int y1 = fmin(_max, ceil(_e1.y1));

        // sort edges by x-coordinate
        if (_e0.x0 == _e1.x0 && _e0.y0 == _e1.y0) {
            if (_e0.x0 + _e1.dy / _e0.dy * _e0.dx < _e1.x1) { std::swap(_e0, _e1); }
        } else {
            if (_e0.x1 - _e1.dy / _e0.dy * _e0.dx < _e1.x0) { std::swap(_e0, _e1); }
        }

        // scan lines!
        double m0 = _e0.dx / _e0.dy;
        double m1 = _e1.dx / _e1.dy;
        double d0 = _e0.dx > 0 ? 1.0 : 0.0;
        double d1 = _e1.dx < 0 ? 1.0 : 0.0;
        for (int y = y0; y < y1; y++) {
            double x0 = m0 * fmax(0.0, fmin(_e0.dy, y + d0 - _e0.y0)) + _e0.x0;
            double x1 = m1 * fmax(0.0, fmin(_e1.dy, y + d1 - _e1.y0)) + _e1.x0;
            scanLine(floor(x1), ceil(x0), y);
        }
    }

    void scanTriangle(const dvec2& _a, const dvec2& _b, const dvec2& _c, int _min, int _max)
    {
        Edge ab = Edge(_a, _b);
        Edge bc = Edge(_b, _c);
        Edge ca = Edge(_c, _a);

        // place edge with greatest y distance in ca
        if (ab.dy > ca.dy) { std::swap(ab, ca); }
        if (bc.dy > ca.dy) { std::swap(bc, ca); }

        // scan span! scan span!
        if (ab.dy > 0) { scanSpan(ca, ab, _min, _max); }
        if (bc.dy > 0) { scanSpan(ca, bc, _min, _max); }
    }

    QList<QPoint> mPoints;
};

#define DISPLAY_TILE_WIDTH (mLayerGroup->owner()->map()->tileWidth() * (renderer->is2x() ? 2 : 1))
#define DISPLAY_TILE_HEIGHT (mLayerGroup->owner()->map()->tileHeight() * (renderer->is2x() ? 2 : 1))

void LayerGroupVBO::gatherTiles(Tiled::MapRenderer *renderer, const QRectF& exposed, QList<VBOTiles*> &exposedTiles)
{
    const int level = mLayerGroup->level();

    qreal tileSize = VBO_SQUARES;
    QPointF TL = renderer->pixelToTileCoords(exposed.topLeft(), level) / tileSize;
    QPointF TR = renderer->pixelToTileCoords(exposed.topRight(), level) / tileSize;
    QPointF BR = renderer->pixelToTileCoords(exposed.bottomRight(), level) / tileSize;
    QPointF BL = renderer->pixelToTileCoords(exposed.bottomLeft(), level) / tileSize;
    Rasterize rasterize;
    rasterize.scanTriangle({TL.x(), TL.y()}, {TR.x(), TR.y()}, {BL.x(), BL.y()}, -VBO_PER_CELL, VBO_PER_CELL * 2);
    rasterize.scanTriangle({TR.x(), TR.y()}, {BR.x(), BR.y()}, {BL.x(), BL.y()}, -VBO_PER_CELL, VBO_PER_CELL * 2);

    auto* layerGroup = mLayerGroup;

    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    QVector<TilePlusLayer> cells(40); // or QVarLengthArray

    for (const QPoint& point : qAsConst(rasterize.mPoints)) {
        VBOTiles *vboTiles = getTilesFor(point * VBO_SQUARES, true);
        if (vboTiles != nullptr) {
            exposedTiles += vboTiles;
        }
        if (vboTiles == nullptr || vboTiles->mGathered) {
            continue;
        }
        vboTiles->mGathered = true;
        auto& tiles = vboTiles->mTiles;
        auto& tileCount = vboTiles->mTileCount;
        auto& tileFirst = vboTiles->mTileFirst;
        QPointF screenOrigin; // = renderer->tileToPixelCoords(vboTiles->mBounds.topLeft() + QPointF(0.5f, 1.5f), level);
        for (int vy = 0; vy < VBO_SQUARES; vy++) {
            for (int vx = 0; vx < VBO_SQUARES; vx++) {
                QPoint square(vboTiles->mBounds.x() + vx, vboTiles->mBounds.y() + vy);
                cells.resize(0);
                if (layerGroup->orderedCellsAt3(square, cells) == false)
                    continue;
                QPointF screenPos(screenOrigin.x() + (vx - vy) * tileWidth / 2, screenOrigin.y() + (vx + vy) * tileHeight / 2); // renderer->tileToPixelCoords(square + QPointF(0.5, 1.5), level);
                for (int i = 0; i < cells.size(); i++) {
                    const TilePlusLayer &cell = cells[i];
                    if (cell.mTile == nullptr)
                        continue;
                    const Tile *tile = cell.mTile;
                    VBOTile vboTile;
                    Tileset *tileset = tile->tileset();
                    vboTile.mSubMap = cell.mSubMap;
                    vboTile.mHideIfVisible = cell.mHideIfVisible;
                    vboTile.mLayerIndex = mMapCompositeVBO->mLayerNameToIndex.value(cell.mLayerName, -1);
                    vboTile.mRect = QRect(screenPos.x() + tile->offset().x(), screenPos.y() + tile->offset().y() - tile->height(), tile->atlasSize().width(), tile->atlasSize().height());
                    vboTile.mTilesetName = tileset->name();
                    vboTile.mAtlasUVST = tile->atlasUVST();

                    if (tileWidth == tile->width() * 2) {
                        vboTile.mRect.translate(tile->offset().x(), tile->offset().y() - tile->height());
                        vboTile.mRect.setWidth(tile->atlasSize().width() * 2);
                        vboTile.mRect.setHeight(tile->atlasSize().height() * 2);
                    }

                    if (tileCount[vx + vy * VBO_SQUARES] == 0) {
                        tileFirst[vx + vy * VBO_SQUARES] = vboTiles->mTiles.size();
                    }
                    tileCount[vx + vy * VBO_SQUARES]++;
                    tiles += vboTile;
                }
            }
        }
    }
}

VBOTiles *LayerGroupVBO::getTilesFor(const QPoint &square, bool bCreate)
{
    if (mMapCompositeVBO->mBounds.contains(square) == false)
        return nullptr;

    int col = ((square.x() + 300) % 300) / VBO_SQUARES;
    int row = ((square.y() + 300) % 300) / VBO_SQUARES;
    if (col < 0 || col >= VBO_PER_CELL || row < 0 || row >= VBO_PER_CELL) {
        return nullptr;
    }
    if (VBOTiles *vboTiles = mTiles[col + row * VBO_PER_CELL]) {
        return vboTiles;
    }
    if (bCreate == false) {
        return nullptr;
    }
    VBOTiles *vboTiles = new VBOTiles();
    vboTiles->mBounds = QRect(col * VBO_SQUARES, row * VBO_SQUARES, VBO_SQUARES, VBO_SQUARES);
    vboTiles->mBounds.translate(mMapCompositeVBO->mBounds.topLeft());
    mTiles[col + row * VBO_PER_CELL] = vboTiles;
    return vboTiles;
}

void LayerGroupVBO::getSquaresInRect(Tiled::MapRenderer *renderer, const QRectF &exposed, QList<QPoint> &out)
{
    const int tileWidth = DISPLAY_TILE_WIDTH;
    const int tileHeight = DISPLAY_TILE_HEIGHT;

    auto* layerGroup = mLayerGroup;

    if (tileWidth <= 0 || tileHeight <= 1 || layerGroup->bounds().isEmpty())
        return;

    int level = layerGroup->level();

    QRect rect = exposed.toAlignedRect();
    if (rect.isNull())
        rect = layerGroup->boundingRect(renderer).toAlignedRect();

    QMargins drawMargins = layerGroup->drawMargins() * (renderer->is2x() ? 2 : 1);
    drawMargins.setTop(drawMargins.top() - tileHeight);
    drawMargins.setRight(drawMargins.right() - tileWidth);

    rect.adjust(-drawMargins.right(),
                -drawMargins.bottom(),
                drawMargins.left(),
                drawMargins.top());

    // Determine the tile and pixel coordinates to start at
    QPointF tilePos = renderer->pixelToTileCoords(rect.x(), rect.y(), level);
    QPoint rowItr = QPoint((int) std::floor(tilePos.x()),
                           (int) std::floor(tilePos.y()));
    QPointF startPos = renderer->tileToPixelCoords(rowItr, level);
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
#if 0
            if (columnItr.x() % 10 != 0 || columnItr.y() % 10 != 0)
                continue;
#endif
            if (mMapCompositeVBO->mBounds.contains(columnItr)) {
                // TODO: change x,y,x2 spans
                out += columnItr;
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

bool LayerGroupVBO::isEmpty() const
{
    for (VBOTiles *vboTiles : mTiles) {
        if (vboTiles != nullptr && vboTiles->mTiles.isEmpty() == false) {
            return false;
        }
    }
    return true;
}

void LayerGroupVBO::aboutToBeDestroyed()
{
    // The QOpenGLContext is going away and QOpenGLFunctions_3_0 becomes invalid.
    for (int i = 0; i < 9; i++) {
        if (mLayerGroupItem->mVBO[i] == this) {
            mLayerGroupItem->mVBO[i] = nullptr;
            break;
        }
    }
    delete this;
}

/////

MapCompositeVBO::MapCompositeVBO()
{
    mLayerVBOs.fill(nullptr);
    mBounds = QRect(0, 0, 300, 300);
}

MapCompositeVBO::~MapCompositeVBO()
{

}

LayerGroupVBO *MapCompositeVBO::getLayerVBO(CompositeLayerGroupItem *item)
{
#if 0
    if (mMapComposite == nullptr) {
        mMapComposite = mScene->mapComposite();
        mUsedTilesets = mMapComposite->usedTilesets();
    }
#endif
    LayerGroupVBO* layerVBO = mLayerVBOs[item->layerGroup()->level()];
    if (layerVBO == nullptr) {
        layerVBO = new LayerGroupVBO();
        layerVBO->mMapCompositeVBO = this;
        layerVBO->mLayerGroupItem = item;
        layerVBO->mLayerGroup = item->layerGroup();
        mLayerVBOs[item->layerGroup()->level()] = layerVBO;
    }
    return layerVBO;
}

///// ///// ///// ///// /////

CompositeLayerGroupItem::CompositeLayerGroupItem(CellScene *cellScene, CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(cellScene)
    , mLayerGroup(layerGroup)
    , mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = layerGroup->boundingRect(mRenderer);

    mVBO.fill(nullptr);
}

CompositeLayerGroupItem::~CompositeLayerGroupItem()
{
    qDeleteAll(mVBO);
}

void CompositeLayerGroupItem::synchWithTileLayers()
{
//    mLayerGroup->synch();

    QRectF bounds = mLayerGroup->boundingRect(mRenderer);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF CompositeLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void CompositeLayerGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (mScene->isDestroying()) {
        return;
    }

    if (mLayerGroup->needsSynch())
        return; // needed, see MapComposite::mapAboutToChange
#if 0
    QImage image = createZoomedOutImage(mRenderer);
    if (image.isNull() == false) {
        p->drawImage(mLayerGroup->boundingRect(mRenderer).toAlignedRect(), image);
        return;
    }
#endif

    if (Preferences::instance()->useOpenGL()) {
        QRect exposed = option->exposedRect.toAlignedRect();
exposed = QRect(); // FIXME: flush area covered by whole VBOTiles
        if (exposed.isNull())
            exposed = mLayerGroup->boundingRect(mRenderer).toAlignedRect();
        mLayerGroup->prepareDrawing3(mRenderer, exposed);

        for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
                // FIXME: can these MapComposites be recreated?
                MapComposite *mc = (x == 1 && y == 1) ? layerGroup()->owner() : layerGroup()->owner()->adjacentMap(x - 1, y - 1);
                if (mc == nullptr)
                    continue;
                MapCompositeVBO *mcVBO = mScene->mapCompositeVBO(x + y * 3);
                if (mVBO[x + y * 3] == nullptr) {
                    mcVBO->mMapComposite = mc;
                    mcVBO->mBounds = QRect((x - 1) * 300, (y - 1) * 300, 300, 300);
    //                if (mcVBO->mScene == nullptr)
                        mcVBO->mScene = mScene;
                    mVBO[x + y * 3] = mcVBO->getLayerVBO(this);
                }

                mcVBO->mUsedTilesets = mc->usedTilesets();
                mcVBO->mLayerNameToIndex.clear();
                if (CompositeLayerGroup *rootLayerGroup = mc->root()->layerGroupForLevel(mLayerGroup->level())) {
                    for (int i = 0; i < rootLayerGroup->layerCount(); i++) {
                        TileLayer *layer = rootLayerGroup->layers()[i];
                        mcVBO->mLayerNameToIndex[layer->name()] = i;
                    }
                }

                mVBO[x + y * 3]->paint(p, mRenderer, option->exposedRect);
            }
        }
    } else {
        mLayerGroup->prepareDrawing(mRenderer, option->exposedRect.toAlignedRect());
        mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
    }

#ifdef _DEBUG
    p->drawRect(mBoundingRect);
#endif
#if 0
    Tileset *tileset = TILESET_TEXTURES.findTileset(QStringLiteral("blends_street_01"), Tiled::Internal::TilesetManager::instance()->tilesets());
    if (tileset != nullptr) {
        p->drawImage(QRect(exposed.topLeft(), tileset->image().size()), tileset->image());
}
#endif
}

/////

ObjectLabelItem::ObjectLabelItem(ObjectItem *item, QGraphicsItem *parent)
    : QGraphicsSimpleTextItem(parent)
    , mItem(item)
    , mShowSize(false)
{
    setAcceptHoverEvents(true);
    setFlag(ItemIgnoresTransformations);

    synch();
}

QRectF ObjectLabelItem::boundingRect() const
{
    QRectF r = QGraphicsSimpleTextItem::boundingRect().adjusted(-3, -3, 2, 2);
    return r.translated(-r.center());
}

QPainterPath ObjectLabelItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

bool ObjectLabelItem::contains(const QPointF &point) const
{
    return boundingRect().contains(point);
}

void ObjectLabelItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                            QWidget *)
{
    QRectF r = boundingRect();
    painter->fillRect(r, mBgColor);
    painter->drawText(r, Qt::AlignCenter, text());
}

void ObjectLabelItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    mItem->hoverEnterEvent(event);
}

void ObjectLabelItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    mItem->hoverLeaveEvent(event);
}

static void resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    foreach (PropertyTemplate *pt, ph->templates())
        resolveProperties(pt, result);
    foreach (Property *p, ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

void ObjectLabelItem::synch()
{
    QString text = mItem->object()->name();
    PropertyList properties;
    resolveProperties(mItem->object(), properties);
    if (!properties.empty()) {
        foreach (Property *p, properties) {
            if (!text.isEmpty())
                text += QLatin1String("\n");
            text += p->mDefinition->mName + QLatin1String("=") + p->mValue;
        }
    }

    if (!mItem->resizeDelta().isNull() || mShowSize) {
        QSizeF size = mItem->object()->size() + mItem->resizeDelta();
        text = QString::fromLatin1("%1 x %2").arg((int)size.width()).arg((int)size.height());
    }

    if (!Preferences::instance()->showObjectNames() || text.isEmpty()) {
        setVisible(false);
    } else {
        setVisible(true);
        setText(text);
        setPos(mItem->boundingRect().center());

        mBgColor = mItem->isMouseOverHighlighted() ? Qt::white : Qt::lightGray;
        mBgColor.setAlphaF(0.75);

        update();
    }
}

/////

/**
 * A resize handle that allows resizing of a WorldCellObject.
 */
class ResizeHandle : public QGraphicsItem
{
public:
    ResizeHandle(ObjectItem *item, CellScene *scene)
        : QGraphicsItem(item)
        , mItem(item)
        , mScene(scene)
        , mSynching(false)
    {
        setFlags(QGraphicsItem::ItemIsMovable |
                 QGraphicsItem::ItemSendsGeometryChanges |
                 QGraphicsItem::ItemIgnoresTransformations |
                 QGraphicsItem::ItemIgnoresParentOpacity);

        setCursor(Qt::SizeFDiagCursor);

        synch();
    }

    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void synch();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    QVariant itemChange(GraphicsItemChange change, const QVariant &value);

private:
    QSizeF mOldSize;
    ObjectItem *mItem;
    CellScene *mScene;
    bool mSynching;
};


QRectF ResizeHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void ResizeHandle::paint(QPainter *painter,
                   const QStyleOptionGraphicsItem *,
                   QWidget *)
{
    painter->setBrush(mItem->object()->group()->color());
    painter->setPen(Qt::black);
    painter->drawRect(QRectF(-5, -5, 10, 10));
}

void ResizeHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // Remember the old size since we may resize the object
    if (event->button() == Qt::LeftButton) {
        mOldSize = mItem->object()->size();
        mItem->labelItem()->setShowSize(true);
        mItem->labelItem()->synch();
    }

    QGraphicsItem::mousePressEvent(event);
}

void ResizeHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    WorldCellObject *obj = mItem->object();
    QSizeF delta = mItem->resizeDelta();
    if (event->button() == Qt::LeftButton && !delta.isNull()) {
        WorldDocument *document = mScene->document()->worldDocument();
        mItem->setResizeDelta(QSizeF(0, 0));
        document->resizeCellObject(obj, mOldSize + delta);
    }
    mItem->labelItem()->setShowSize(false);
    mItem->labelItem()->synch();
}

QVariant ResizeHandle::itemChange(GraphicsItemChange change,
                                  const QVariant &value)
{
    if (!mSynching) {
        int level = mItem->object()->level();

        if (change == ItemPositionChange) {
            // Calculate the absolute pixel position
            const QPointF itemPos = mItem->pos();
            QPointF pixelPos = value.toPointF() + itemPos;

            // Calculate the new coordinates in tiles
            QPointF tileCoords = mScene->renderer()->pixelToTileCoords(pixelPos, level);

            const QPointF objectPos = mItem->object()->pos();
            tileCoords -= objectPos;
            tileCoords.setX(qMax(tileCoords.x(), qreal(MIN_OBJECT_SIZE)));
            tileCoords.setY(qMax(tileCoords.y(), qreal(MIN_OBJECT_SIZE)));
            tileCoords += objectPos;

#if 1
            tileCoords = tileCoords.toPoint();
#else
            bool snapToGrid = Preferences::instance()->snapToGrid();
            if (QApplication::keyboardModifiers() & Qt::ControlModifier)
                snapToGrid = !snapToGrid;
            if (snapToGrid)
                tileCoords = tileCoords.toPoint();
#endif

            return mScene->renderer()->tileToPixelCoords(tileCoords, level) - itemPos;
        }
        else if (change == ItemPositionHasChanged) {
            // Update the size of the map object
            const QPointF newPos = value.toPointF() + mItem->pos();
            QPointF tileCoords = mScene->renderer()->pixelToTileCoords(newPos, level);
            tileCoords -= mItem->object()->pos();
            mItem->setResizeDelta(QSizeF(tileCoords.x(), tileCoords.y()) - mItem->object()->size());
        }
    }

    return QGraphicsItem::itemChange(change, value);
}

void ResizeHandle::synch()
{
    int level = mItem->object()->level();
    QPointF bottomRight = mItem->tileBounds().bottomRight();
    QPointF scenePos = mScene->renderer()->tileToPixelCoords(bottomRight, level);
    if (scenePos != pos()) {
        mSynching = true;
        setPos(scenePos);
        mSynching = false;
    }
}

/////

// Just like MapRenderer::boundingRect, but takes fractional tile coords
static QRectF boundingRect(MapRenderer *renderer, const QRectF &bounds, int level)
{
    qreal left = renderer->tileToPixelCoords(bounds.bottomLeft(), level).x();
    qreal top = renderer->tileToPixelCoords(bounds.topLeft(), level).y();
    qreal right = renderer->tileToPixelCoords(bounds.topRight(), level).x();
    qreal bottom = renderer->tileToPixelCoords(bounds.bottomRight(), level).y();
    return QRectF(left, top, right-left, bottom-top);
}

ObjectItem::ObjectItem(WorldCellObject *obj, CellScene *scene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(scene)
    , mRenderer(scene->renderer())
    , mObject(obj)
    , mSyncing(false)
    , mIsEditable(false)
    , mIsSelected(false)
    , mHoverRefCount(0)
    , mResizeDelta(0, 0)
    , mResizeHandle(new ResizeHandle(this, scene))
    , mLabel(new ObjectLabelItem(this, this))
    , mAdjacent(false)
    , mAddPointIndex(-1)
{
    setAcceptHoverEvents(true);
    mBoundingRect = ::boundingRect(mRenderer, QRectF(mObject->pos(), mObject->size()),
                                   mObject->level()).adjusted(-20, -20, 20, 20);
    mResizeHandle->setVisible(false);

    // Update the tooltip
    synchWithObject();
}

QRectF ObjectItem::boundingRect() const
{
    return mBoundingRect;
}

#if 0
namespace {

class OutlineCell {
public:
    OutlineCell(int x, int y)
        : x(x)
        , y(y)
    {
    }

    void reset()
    {
        w = n = e = s = false;
        tw = tn = te = ts = false;
        start = false;
    }

    int x = -1, y = -1;
    bool w = false, n = false, e = false, s = false; // true if no cell in this direction
    bool tw = false, tn = false, te = false, ts = false; // true if traced the given edge
    bool inner = false;
    bool start = false;
};

typedef std::shared_ptr<OutlineCell> OutlineCellPtr;

class OutlineGrid {
public:
    std::vector<OutlineCellPtr> elements;
    int W, H;
    bool EXTEND = true;

    void setSize(int w, int h) {
        elements.resize(size_t(w * h));
        W = w;
        H = h;
    }

    void setInner(int x, int y) {
        OutlineCellPtr f1 = get(x, y);
        if (f1) {
            f1->inner = true;
        }
    }

    bool isInner(int x, int y) {
        OutlineCellPtr f1 = get(x, y);
        return f1 && (f1->start || f1->inner);
    }

    bool canTrace_W(int x, int y) {
        OutlineCellPtr cell = get(x, y);
        return cell && cell->inner && cell->w && !cell->tw;
    }

    bool canTrace_N(int x, int y) {
        OutlineCellPtr cell = get(x, y);
        return cell && cell->inner && cell->n && !cell->tn;
    }

    bool canTrace_E(int x, int y) {
        OutlineCellPtr cell = get(x, y);
        return cell && cell->inner && cell->e && !cell->te;
    }

    bool canTrace_S(int x, int y) {
        OutlineCellPtr cell = get(x, y);
        return cell && cell->inner && cell->s && !cell->ts;
    }

    OutlineCellPtr& elementAt(int x, int y) {
        return elements[size_t(x + y * W)];
    }

    OutlineCellPtr get(int x, int y) {
        if (x < 0 || x >= W)
            return nullptr;
        if (y < 0 || y >= H)
            return nullptr;
        if (!elementAt(x, y))
            elementAt(x, y) = std::make_shared<OutlineCell>(x, y);
        return elementAt(x, y);
    }

    void trace_W(OutlineCell& cell, QPolygon& nodes, int extend) {
        const int x = cell.x, y = cell.y;
        if (EXTEND && extend != -1) {
            nodes[extend] = { x, y };
        } else {
            nodes += { x, y };
        }
        cell.tw = true; // done

        // turn w, continue n, turn e
        if (canTrace_S(x - 1, y - 1)) {
            trace_S(*get(x - 1, y - 1), nodes, -1);
        } else if (canTrace_W(x, y - 1)) {
            trace_W(*get(x, y - 1), nodes, nodes.size()-1);
        } else if (canTrace_N(x, y)) {
            trace_N(cell, nodes, -1);
        }
    }

    void trace_N(OutlineCell& cell, QPolygon& nodes, int extend) {
        const int x = cell.x, y = cell.y;
        if (EXTEND && extend != -1) {
            nodes[extend] = { x + 1, y };
        } else {
            nodes += { x + 1, y };
        }
        cell.tn = true; // done

        // turn n, continue e, turn s
        if (canTrace_W(x + 1, y - 1)) {
            trace_W(*get(x + 1, y - 1), nodes, -1);
        } else if (canTrace_N(x + 1, y)) {
            trace_N(*get(x + 1, y), nodes, nodes.size()-1);
        } else if (canTrace_E(x, y)) {
            trace_E(cell, nodes, -1);
        }
    }

    void trace_E(OutlineCell& cell, QPolygon& nodes, int extend) {
        const int x = cell.x, y = cell.y;
        if (EXTEND && extend != -1) {
            nodes[extend] = { x + 1, y + 1 };
        } else {
            nodes += { x + 1, y + 1 };
        }
        cell.te = true; // done

        // turn e, continue s, turn w
        if (canTrace_N(x + 1, y + 1)) {
            trace_N(*get(x + 1, y + 1), nodes, -1);
        } else if (canTrace_E(x, y + 1)) {
            trace_E(*get(x, y + 1), nodes, nodes.size()-1);
        } else if (canTrace_S(x, y)) {
            trace_S(cell, nodes, -1);
        }
    }

    void trace_S(OutlineCell& cell, QPolygon& nodes, int extend) {
        const int x = cell.x, y = cell.y;
        if (EXTEND && extend != -1) {
            nodes[extend] = { x, y + 1 };
        } else {
            nodes += { x, y + 1 };
        }
        cell.ts = true; // done

        // turn s, continue w, turn n
        if (canTrace_E(x - 1, y + 1)) {
            trace_E(*get(x - 1, y + 1), nodes, -1);
        } else if (canTrace_S(x - 1, y)) {
            trace_S(*get(x - 1, y), nodes, nodes.size()-1);
        } else if (canTrace_W(x, y)) {
            trace_W(cell, nodes, -1);
        }
    }

    QPolygon trace(OutlineCell& cell) {
        const int x = cell.x, y = cell.y;
        QPolygon nodes;
        QPoint node1(x, y);
        nodes += node1;
        cell.start = true;
        trace_N(cell, nodes, -1);
        if (nodes.back() == nodes.first())
            nodes.pop_back();
        return nodes;
    }

    void trace(bool extend, std::function<void(QPolygon&)> callback) {
        EXTEND = extend;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                OutlineCell& cell = *get(x, y);
                cell.reset();
                if (!cell.inner)
                    continue;
                if (!isInner(x - 1, y))
                    cell.w = true;
                if (!isInner(x, y - 1))
                    cell.n = true;
                if (!isInner(x + 1, y))
                    cell.e = true;
                if (!isInner(x, y + 1))
                    cell.s = true;
            }
        }

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                OutlineCellPtr cell = get(x, y);
                // every poly must have a nw corner.
                // this should only happen once.
                if (cell && cell->n && cell->w && cell->inner && !(cell->tw | cell->tn | cell->te | cell->ts)) {
                    QPolygon nodes = trace(*cell);
                    if (nodes.isEmpty())
                        continue;
                    callback(nodes);
                }
            }
        }
    }
};

namespace networkx
{

class NodeAttributes : public QMap<QString,QString>
{
public:
    void update(NodeAttributes attrs)
    {

    }
};

template <typename T>
class Node
{
public:
    Node()
        : _value()
    {
    }

    Node(T value)
        : _value(value)
    {

    }

    bool operator==(const Node& rhs) const
    {
        return _value == rhs._value;
    }

    bool operator!=(const Node& rhs) const
    {
        return _value != rhs._value;
    }

#if 1 // for QHash
#endif
#if 0 // for QMap
    bool operator<(const Node& rhs) const
    {
        return _value < rhs._value;
    }
#endif

    T _value;
};

template <typename T>
uint qHash(const Node<T> &key)
{
    return ::qHash(key._value);
}

template <typename T>
class Edge
{
public:
    bool operator==(const Edge& rhs) const
    {
        return (node1 == rhs.node1) && (node2 == rhs.node2);
    }

    Node<T> node1;
    Node<T> node2;
};

template <typename T>
class Graph;

template <typename T>
class EdgeView
{
public:
    EdgeView(const Graph<T>& graph)
        : _graph(graph)
    {

    }

    bool contains(const Edge<T>& edge) const
    {

    }

    const Graph<T>& _graph;
};


template <typename T>
class Graph
{
public:
    void add_node(T node_for_adding, const NodeAttributes& attrs)
    {
        if (_node.contains(node_for_adding)) {
            _node[node_for_adding].update(attrs);
        } else {
            _adj[node_for_adding].clear();
            _node[node_for_adding].update(attrs);
        }
    }

    void add_edge(const Node<T> &u, const Node<T> &v)
    {
        NodeAttributes attrs;
        if (_node.contains(u) == false) {
            _adj[u].clear();
            _node[u].update(attrs);
        }
        if (_node.contains(v) == false) {
            _adj[v].clear();
            _node[v].update(attrs);
        }
#if 1
        if (_adj[u].contains(v) == false)
             _adj[u] += v;
         if (_adj[v].contains(u) == false)
             _adj[v] += u;
#else
        datadict = self._adj[u].get(v, self.edge_attr_dict_factory())
        datadict.update(attr)
        self._adj[u][v] = datadict
        self._adj[v][u] = datadict
#endif
    }

    void add_edges_from(const QList<Edge<T>>& ebunch_to_add)
    {
        for (const Edge<T> &e : ebunch_to_add) {
            add_edge(e.node1, e.node2);
        }
    }

    void remove_edge(const Node<T>& u, const Node<T>& v)
    {
        if (_adj.contains(u) == false) {
            throw std::exception("The edge {u}-{v} is not in the graph");
        }
        _adj[u].removeOne(v);
        if (u != v) { // self-loop needs only one entry removed
            _adj[v].removeOne(u);
        }
    }

    QVector<Edge<T>> edges() const
    {
        // FIXME: should this return u,v and v,u ???
        QVector<Edge<T>> result;
        for (const auto& node : _node.keys()) {
            for (const auto& adj : _adj[node]) {
                result += { node, adj };
            }
        }
        return result;
    }

    QList<Node<T>> nodes() const
    {
        return _node.keys();
    }

    QHash<Node<T>,int> degree() const
    {
        QHash<Node<T>,int> result;
        for (auto& n : _adj.keys()) {
            result[n] = _adj[n].size();
        }
        return result;
    }

    QList<Node<T>> neighbours(const Node<T>& n) const
    {
        return _adj[n];
    }

    int len() const
    {
        return _node.size();
    }

    bool is_directed() const
    {
        return false;
    }

    QHash<Node<T>, QList<Node<T>>> _adj;
    QHash<Node<T>, NodeAttributes> _node; // unordered for arbitrary_element()
};

template <typename T>
Node<T> arbitrary_element(const Graph<T> &G)
{
    return G._node.cbegin().key();
}

template <typename T>
QSet<Node<T>> _plain_bfs(const Graph<T> &G, const Node<T> &source)
{
    const auto& G_adj = G._adj;
    QSet<Node<T>> seen;
    QSet<Node<T>> nextlevel;
    nextlevel.insert(source);
    while (nextlevel.isEmpty() == false) {
        auto thislevel = nextlevel;
        nextlevel.clear();
        for (const auto &v : thislevel) {
            if (seen.contains(v) == false) {
                seen.insert(v);
                for (const auto &n : G_adj[v]) {
                    nextlevel += n;
                }
            }
        }
    }
    return seen;
}

template <typename T>
bool is_connected(const Graph<T> &G)
{
    if (G.len() == 0) {
        throw std::exception("Connectivity is undefined for the null graph.");
    }
    return _plain_bfs(G, arbitrary_element(G)).size() == G.len();
}

template <typename T>
bool is_weakly_connected(const Graph<T> &G)
{
    throw std::exception("unimplemented");
    return false;
}

template <typename T>
QList<Node<T>> isolates(const Graph<T> &G) // should return an iterator
{
    QList<Node<T>> result;
    QHash<Node<T>,int> degree = G.degree();
    for (Node<T> n : degree.keys()) {
        if (degree[n] == 0) {
            result += n;
        }
    }
    return result;
}

namespace bipartite
{

template<typename T>
QHash<Node<T>, bool> color(const Graph<T> &G)
{
    if (G.is_directed()) {
        throw std::exception("unimplemented");
    }
    QHash<Node<T>, bool> _color;
    for (const auto &n : G.nodes()) { // handle disconnected graphs
        if (_color.contains(n) || G.neighbours(n).isEmpty()) // skip isolates
            continue;
        QVector<Node<T>> queue;
        queue += n;
        _color[n] = true;
        while (queue.isEmpty() == false) {
            auto v = queue.back();
            queue.pop_back();
            bool c = !_color[v]; // opposite color of node v
            for (const auto& w : G.neighbours(v)) {
                if (_color.contains(w)) {
                    if (_color[w] == _color[v]) {
                        throw std::exception("Graph is not bipartite.");
                    }
                } else {
                    _color[w] = c;
                    queue += w;
                }
            }
        }
    }
    // color isolates with 0
    for (auto& n : isolates(G)) {
        _color[n] = false;
    }
    return _color;
}

template<typename T>
bool is_bipartite(const Graph<T>& G)
{
    try {
        color(G);
        return true;
    }  catch (std::exception e) {
        return false;
    }
}

template<typename T>
void sets(const Graph<T>& G, const QSet<Node<T>> &top_nodes, QSet<Node<T>> &X, QSet<Node<T>> &Y)
{
    std::function<bool(const Graph<T>&)> is_connected1;
    if (G.is_directed()) {
        is_connected1 = networkx::is_weakly_connected<T>;
    } else {
        is_connected1 = networkx::is_connected<T>;
    }

    if (top_nodes.isEmpty() == false) {
        X.clear();
        X = top_nodes;
        const QList<Node<T>> nodes = G.nodes();
        Y = QSet<Node<T>>(nodes.constBegin(), nodes.constEnd()) - X;
    } else {
#if 0 //
        if (is_connected1(G) == false) {
            QSet<Node<int>> plain = _plain_bfs(G, arbitrary_element(G));
            qDebug() << "is_connected()==false" << "_plain_bfs.size()=" << plain.size() << "len=" << G.len();
            for (auto& n : plain) qDebug() << "plain" << n._value << "#adj=" << G.neighbours(n).size();
            for (auto& n : G.nodes()) qDebug() << "G.nodes" << n._value << "#adj=" << G.neighbours(n).size();
            throw std::exception("Disconnected graph: Ambiguous solution for bipartite sets.");
        }
#endif
        auto c = color(G);
        for (auto& node : c.keys()) {
            if (c[node])
                X.insert(node);
            else
                Y.insert(node);
        }
    }
}

template <typename T>
QHash<Node<T>, Node<T>> maximum_matching(Graph<T>& G, const QSet<Node<T>> &top_nodes, const Node<T> &None)
{
    QSet<Node<T>> left, right;
    sets(G, top_nodes, left, right);

    QHash<Node<T>, Node<T>> leftmatches, rightmatches;
    for (const auto& n : left) {
        leftmatches[n] = None;
    }
    for (const auto& n : right) {
        rightmatches[n] = None;
    }

    QHash<Node<T>, int> distances;
    QVector<Node<T>> queue;

    const int inf = std::numeric_limits<int>::max(); // infinity();

    auto breadth_first_search = [&]() -> bool {
        for (const auto& v : left) {
            if (leftmatches[v] == None) {
                distances[v] = 0;
                queue.append(v);
            } else {
                distances[v] = inf;
            }
        }
        distances[None] = inf;
        while (queue.size() > 0) {
            auto v = queue.front();
            queue.pop_front();
            if (distances[v] < distances[None]) {
                for (auto& u : G.neighbours(v)) {
                    if (distances[rightmatches[u]] == inf) {
                        distances[rightmatches[u]] = distances[v] + 1;
                        queue.append(rightmatches[u]);
                    }
                }
            }
        }
        return distances[None] != inf;
    };

    std::function<bool(const Node<T>&)> depth_first_search;
    depth_first_search = [&](const Node<T>& v) -> bool {
        if (v == None) {
            return true;
        }
        for (auto& u : G.neighbours(v)) {
            if (distances[rightmatches[u]] == distances[v] + 1) {
                if (depth_first_search(rightmatches[u])) {
                    rightmatches[u] = v;
                    leftmatches[v] = u;
                    return true;
                }
            }
        }
        distances[v] = inf;
        return false;
    };

    // Implementation note: this counter is incremented as pairs are matched but
    // it is currently not used elsewhere in the computation.
    int num_matched_pairs = 0;
    while (breadth_first_search()) {
        for (const auto& v : left) {
            if (leftmatches[v] == None) {
                if (depth_first_search(v)) {
                    num_matched_pairs += 1;
                }
            }
        }
    }

    // Strip the entries matched to `None`.

    // At this point, the left matches and the right matches are inverses of one
    // another. In other words,
    //
    //     leftmatches == {v, k for k, v in rightmatches.items()}
    //
    // Finally, we combine both the left matches and right matches.
    QHash<Node<T>, Node<T>> result;
    for (auto& n : leftmatches.keys()) {
        if (leftmatches[n] == None)
            continue;
        result[n] = leftmatches[n];
    }
    for (auto& n : rightmatches.keys()) {
        if (rightmatches[n] == None)
            continue;
        result[n] = rightmatches[n];
    }
    return result;
}

// namespace bipartite
}

// namespace networkx
}

// https://github.com/mittalgovind/Polygon-Partition
class PolygonPartition
{
public:
    enum class VertexType
    {
        Collinear,
        Convex,
        Concave
    };

    class Vertex
    {
    public:
        int x;
        int y;
        VertexType type;
    };

    class Chord
    {
    public:
        int v1;
        int v2;
    };

//    QVector<VertexType> vertex_type;
    QVector<int> x;
    QVector<int> y;
    QVector<int> collinear_vertices;
    QVector<int> concave_vertices;
    QVector<Chord> horizontal_chords;
    QVector<Chord> vertical_chords;

    void compute_maximum_partition(const QVector<Vertex> &p)
    {
        x.clear();
        y.clear();
        collinear_vertices.clear();
        concave_vertices.clear();
        horizontal_chords.clear();
        vertical_chords.clear();

        for (int i = 0; i < p.size(); i++) {
            x += p[i].x;
            y += p[i].y;
            if (p[i].type == VertexType::Collinear) {
                collinear_vertices += i;
            }
            if (p[i].type == VertexType::Concave) {
                concave_vertices += i;
            }
        }

        for (int i = 0; i < concave_vertices.size(); i++) {
            for (int j = i+1; j < concave_vertices.size(); j++) {
                if (concave_vertices[j] != concave_vertices[i] + 1) {
                    if (y[concave_vertices[i]] == y[concave_vertices[j]]) {
                        QVector<int> middles;
                        for (int k = 0; k < x.size(); k++) {
                            if ((y[concave_vertices[i]] == y[k]) &&
                                    ((x[concave_vertices[i]] < x[k] && x[concave_vertices[j]] > x[k]) ||
                                     (x[concave_vertices[i]] > x[k] && x[concave_vertices[j]] < x[k]))) {
                                middles.append(k);
                            }
                        }
                        if (middles.isEmpty()) {
                            horizontal_chords.append({ concave_vertices[i], concave_vertices[j] });
                        }
                    }
                    if (x[concave_vertices[i]] == x[concave_vertices[j]]) {
                        QVector<int> middles;
                        for (int k = 0; k < x.size(); k++) {
                            if ((x[concave_vertices[i]] == x[k]) &&
                                    ((y[concave_vertices[i]] < y[k] && y[concave_vertices[j]] > y[k]) ||
                                     (y[concave_vertices[i]] > y[k] && y[concave_vertices[j]] < y[k]))) {
                                middles.append(k);
                            }
                        }
                        if (middles.isEmpty()) {
                            vertical_chords.append({concave_vertices[i],concave_vertices[j]});
                        }
                    }
                }
            }
        }

        for (int i = 0; i < collinear_vertices.size(); i++) {
            for (int j = 0; j < concave_vertices.size(); j++) {
                if (y[collinear_vertices[i]] == y[concave_vertices[j]]) {
                    QVector<int> middles;
                    if (collinear_vertices[i] < concave_vertices[j]) {
                        for (int k  = 0; k < x.size(); k++) {
                            if ((y[k] == y[collinear_vertices[i]]) &&
                                    ((x[k] < x[concave_vertices[j]] && x[k] > x[collinear_vertices[i]]) ||
                                     (x[k] > x[concave_vertices[j]] && x[k] < x[collinear_vertices[i]]))) {
                                middles.append(k);
                            }
                        }
                        if (collinear_vertices[i]+1 == concave_vertices[j]) {
                            middles.append(0);
                        }
                    } else {
                        for (int k  = 0; k < x.size(); k++) {
                            if ((y[k] == y[collinear_vertices[i]]) &&
                                    ((x[k] > x[concave_vertices[j]] && x[k] < x[collinear_vertices[i]]) ||
                                     (x[k] < x[concave_vertices[j]] && x[k] > x[collinear_vertices[i]]))) {
                                middles.append(k);
                            }
                        }
                        if (collinear_vertices[i] == concave_vertices[j]+1) {
                            middles.append(0);
                        }
                    }
                    if (middles.isEmpty()) {
                        horizontal_chords.append({collinear_vertices[i],concave_vertices[j]});
                    }
                }
                if (x[collinear_vertices[i]] == x[concave_vertices[j]]) {
                    QVector<int> middles;
                    if (collinear_vertices[i] < concave_vertices[j]) {
                        for (int k  = 0; k < x.size(); k++) {
                            if ((x[k] == x[collinear_vertices[i]]) &&
                                    ((y[k] < y[concave_vertices[j]] && y[k] > y[collinear_vertices[i]]) ||
                                     (y[k] > y[concave_vertices[j]] && y[k] < y[collinear_vertices[i]]))) {
                                middles.append(k);
                            }
                        }
                        if (collinear_vertices[i]+1 == concave_vertices[j]) {
                            middles.append(0);
                        }
                    } else {
                        for (int k  = 0; k < x.size(); k++) {
                            if ((x[k] == x[collinear_vertices[i]]) &&
                                    ((y[k] > y[concave_vertices[j]] && y[k] < y[collinear_vertices[i]]) ||
                                     (y[k] < y[concave_vertices[j]] && y[k] > y[collinear_vertices[i]]))) {
                                middles.append(k);
                            }
                        }
                        if (collinear_vertices[i] == concave_vertices[j]+1) {
                            middles.append(0);
                        }
                    }
                    if (middles.isEmpty()) {
                        vertical_chords.append({collinear_vertices[i],concave_vertices[j]});
                    }
                }
            }
        }
    }

    QVector<QPoint> min1, min2;

    void compute_minimum_partition(const QVector<Vertex> &p)
    {
        x.clear();
        y.clear();
        collinear_vertices.clear();
        concave_vertices.clear();
        horizontal_chords.clear();
        vertical_chords.clear();

        // and the origin is always going to be a convex vertex
        if (p[0].type != VertexType::Convex)
            throw std::exception("origin should be convex");

        for (int i = 0; i < p.size(); i++) {
            x += p[i].x;
            y += p[i].y;
            if (p[i].type == VertexType::Collinear) {
                collinear_vertices += i;
            }
            if (p[i].type == VertexType::Concave) {
                concave_vertices += i;
            }
        }

        // middles is used because, there are cases when there is a chord between vertices
        // and they intersect with external chords, hence if there is any vertex in between
        // two vertices then skip that chord.
        for (int i = 0; i < concave_vertices.size(); i++) {
            for (int j = i+1; j < concave_vertices.size(); j++) {
                if (concave_vertices[j] != concave_vertices[i] + 1) {
                    if (y[concave_vertices[i]] == y[concave_vertices[j]]) {
                        QVector<int> middles;
                        for (int k = 0; k < x.size(); k++) {
                            if ((y[concave_vertices[i]] == y[k]) &&
                                    ((x[concave_vertices[i]] < x[k] && x[concave_vertices[j]] > x[k]) ||
                                     (x[concave_vertices[i]] > x[k] && x[concave_vertices[j]] < x[k]))) {
                                middles.append(k);
                            }
                        }
                        if (middles.isEmpty()) {
                            horizontal_chords.append({ concave_vertices[i], concave_vertices[j] });
                        }
                    }
                    if (x[concave_vertices[i]] == x[concave_vertices[j]]) {
                        QVector<int> middles;
                        for (int k = 0; k < x.size(); k++) {
                            if ((x[concave_vertices[i]] == x[k]) &&
                                    ((y[concave_vertices[i]] < y[k] && y[concave_vertices[j]] > y[k]) ||
                                     (y[concave_vertices[i]] > y[k] && y[concave_vertices[j]] < y[k]))) {
                                middles.append(k);
                            }
                        }
                        if (middles.isEmpty()) {
                            vertical_chords.append({concave_vertices[i],concave_vertices[j]});
                        }
                    }
                }
            }
        }

        // Creating a bipartite graph from the set of chords
        networkx::Graph<int> G;
        for (int i = 0; i < horizontal_chords.size(); i++) {
            const Chord& h = horizontal_chords[i];
            int y1 = y[h.v1];
            int x1 = std::min(x[h.v1], x[h.v2]);
            int x2 = std::max(x[h.v1], x[h.v2]);
            networkx::NodeAttributes attrs1;
            attrs1.insert(QStringLiteral("bipartite"), QStringLiteral("true"));
            G.add_node(i, attrs1);
            for (int j = 0; j < vertical_chords.size(); j++) {
                const Chord &v = vertical_chords[j];
                int x3 = x[v.v1];
                int y3 = std::min(y[v.v1], y[v.v2]);
                int y4 = std::max(y[v.v1], y[v.v2]);
                networkx::NodeAttributes attrs2;
                attrs2.insert(QStringLiteral("bipartite"), QStringLiteral("false"));
                G.add_node(j + horizontal_chords.size(), attrs2);
                if (x1 <= x3 && x3 <= x2 && y3 <= y1 && y1 <= y4) {
                    G.add_edge(i, j + horizontal_chords.size());
                }
            }
        }

        if (horizontal_chords.isEmpty()) {
            for (int j = 0; j < vertical_chords.size(); j++) {
                networkx::NodeAttributes attrs2;
                attrs2.insert(QStringLiteral("bipartite"), QStringLiteral("false"));
                G.add_node(j, attrs2);
            }
        }

        if (networkx::bipartite::is_bipartite(G) == false)
            return;

        // There could be no horizontal and no vertical chords
        if (G.len() == 0) {
//            return; // FIXME: this should be allowed I think
        }

        // finding the maximum matching of the bipartite graph, G.
        networkx::Node<int> None(-1);
        auto maximum_matching1 = networkx::bipartite::maximum_matching(G, QSet<networkx::Node<int>>(), None);
        QList<networkx::Edge<int>> maximum_matching_list;
        auto it = maximum_matching1.constBegin();
        for (; it != maximum_matching1.constEnd(); it++) {
            maximum_matching_list += { it.key(), it.value() };
        }
        networkx::Graph<int> M;
        M.add_edges_from(maximum_matching_list);
        auto maximum_matching = M.edges();

        // breaking up into two sets
        QSet<networkx::Node<int>> H, V;
        networkx::bipartite::sets(G, QSet<networkx::Node<int>>(), H, V);
        QVector<networkx::Node<int>> free_vertices;
        for (const auto& u : H) {
            QVector<networkx::Node<int>> temp;
            for (const auto &v : V) {
                if (maximum_matching.contains({u,v}) || maximum_matching.contains({v,u})) {
                    temp += v;
                }
            }
            if (temp.isEmpty()) {
                free_vertices += u;
            }
        }
        for (auto &u : V) {
            QVector<networkx::Node<int>> temp;
            for (auto& v : H) {
                if (maximum_matching.contains({u,v}) || maximum_matching.contains({v,u})) {
                    temp += v;
                }
            }
            if (temp.isEmpty()) {
                free_vertices += u;
            }
        }

        // finding the maximum independent set
        QList<networkx::Node<int>> max_independent;
        while (free_vertices.size() != 0 || maximum_matching.size() != 0) {
            networkx::Node<int> u(-1);
            if (free_vertices.size() != 0) {
                u = free_vertices.back();
                free_vertices.pop_back();
                max_independent += u;
            } else {
                networkx::Edge<int> uv = maximum_matching.back();
                maximum_matching.pop_back();
                u = uv.node1;
                auto v = uv.node2;
                G.remove_edge(u, v);
                max_independent += u;
            }
            for (const networkx::Node<int> &v : G.neighbours(u)) {
                G.remove_edge(u, v);
                for (const networkx::Node<int> &h : G.nodes()) {
                    if (maximum_matching.contains({v,h})) {
                        maximum_matching.removeOne({v,h});
                        free_vertices += h;
                    }
                    if (maximum_matching.contains({h,v})) {
                        maximum_matching.removeOne({h,v});
                        free_vertices += h;
                    }
                }
            }
        }

        // drawing the partitioned polygon
        QList<Chord> ind_chords;
        for (const auto& i : max_independent) {
            if (i._value >= horizontal_chords.size()) {
                ind_chords += vertical_chords[i._value-horizontal_chords.size()];
            } else {
                ind_chords += horizontal_chords[i._value];
            }
        }
        QVector<int> unmatched_concave_vertices = concave_vertices;
        for (const auto& ij : ind_chords) {
            auto& i = ij.v1;
            auto& j = ij.v2;
            if (unmatched_concave_vertices.contains(i))
                unmatched_concave_vertices.removeOne(i);
            if (unmatched_concave_vertices.contains(j))
                unmatched_concave_vertices.removeOne(j);
        }

        const int inf = std::numeric_limits<int>::max(); // infinity();

        QList<Chord> nearest_chord;
        for (const auto& i : unmatched_concave_vertices) {
            int dist = 0;
            int nearest_distance = inf;
            for (const auto& j : max_independent) {
                if (j._value < horizontal_chords.size()) {
                    Chord hc = horizontal_chords[j._value];
                    auto temp1 = hc.v1, temp2 = hc.v2;
                    if ((std::abs(y[i] - y[temp1]) < nearest_distance) &&
                            (((x[i] <= x[temp1]) && (x[i] >= x[temp2])) || ((x[i] >= x[temp1]) && (x[i] <= x[temp2]))) &&
                            (std::abs(temp1 - i) != 1) && (std::abs(temp2 - i) != 1)) {
                        QVector<int> middles;
                        for (int u = 0; u < x.size(); u++) {
                            if ((x[i] == x[u]) && (((y[i] < y[u]) && (y[u] < y[temp1])) || ((y[temp1] < y[u]) && (y[u] < y[i])))) {
                                middles.append(u);
                            }
                        }
                        if (middles.isEmpty()) {
                            nearest_distance = std::abs(y[i] - y[temp1]);
                            dist = y[temp1] - y[i];
                        }
                    }
                }
            }

            if (nearest_distance != inf) {
                nearest_chord.append({i, dist});
            } else {
                for (int k : collinear_vertices) {
                    if ((x[k] == x[i]) && (std::abs(y[k] - y[i]) < nearest_distance) && (std::abs(k-i) != 1)) {
                        QVector<int> middles;
                        for (int u = 0; u < x.size(); u++) {
                            if ((x[i] == x[u]) && (((y[i] < y[u]) && (y[u] < y[k])) || ((y[k] < y[u]) && (y[u] < y[i])))) {
                                middles.append(u);
                            }
                        }
                        if (middles.isEmpty()) {
                            nearest_distance = std::abs(y[i] - y[k]);
                            dist = y[k] - y[i];
                        }
                    }
                }
                nearest_chord.append({i, dist});
            }
        }

#if 0
        for i,j in ind_chords:
                ax.plot([x[i],x[j]],[y[i],y[j]],color='black')
            for i,dist in nearest_chord:
                ax.plot([x[i],x[i]],[y[i], y[i]+dist],color='black')
#endif
        for (int i = 0; i < ind_chords.size(); i++) {
            auto &c = ind_chords[i];
            min1 += {x[c.v1], y[c.v1]};
            min1 += {x[c.v2], y[c.v2]};
        }
        for (int i = 0; i < nearest_chord.size(); i++) {
            auto &c = nearest_chord[i];
            min2 += {x[c.v1], y[c.v1]};
            min2 += {x[c.v1], y[c.v1] + c.v2};
        }
    }
};

} // namespace

// Copied from BuildingFloor::roomRegion()
static QRegion cleanupRegion(QRegion region)
{
    // Clean up the region by merging vertically-adjacent rectangles of the
    // same width.
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++) {
        QRect r = rects[i];
        if (!r.isValid()) continue;
        for (int j = 0; j < rects.size(); j++) {
            if (i == j) continue;
            QRect r2 = rects.at(j);
            if (!r2.isValid()) continue;
            if (r2.left() == r.left() && r2.right() == r.right()) {
                if (r.bottom() + 1 == r2.top()) {
                    r.setBottom(r2.bottom());
                    rects[j] = QRect();
                } else if (r.top() == r2.bottom() + 1) {
                    r.setTop(r2.top());
                    rects[j] = QRect();
                }
            }
        }
        rects[i] = r;
    }

    QRegion ret;
    foreach (QRect r, rects) {
        if (r.isValid())
            ret += r;
    }
    return ret;
}
#endif

#include "InGameMap/clipper.hpp"

void ObjectItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (mObject->points().isEmpty() == false) {
        QColor color = mObject->group()->color();
        if (mIsSelected)
            color = QColor(0x33,0x99,0xff/*,255/8*/);
        if (isMouseOverHighlighted())
            color = color.lighter();

    //    if (mHoverRefCount)
    //        painter->drawPath(shape());

        QColor brushColor = color;
        brushColor.setAlpha(50);
        QBrush brush(brushColor);

        QPen pen(Qt::black);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        pen.setWidth(2);
        pen.setCosmetic(true);

        painter->setPen(pen);
        painter->setRenderHint(QPainter::Antialiasing);

        QPolygonF screenPolygon = mRenderer->tileToPixelCoords(mPolygon.translated(mDragOffset));

        switch (mObject->geometryType()) {
        case ObjectGeometryType::INVALID:
            break;
        case ObjectGeometryType::Point:
            pen.setColor(color);
            painter->setPen(pen);
            painter->setBrush(brush);
            painter->drawEllipse(screenPolygon[0], 10, 10);
            break;
        case ObjectGeometryType::Polygon:
            painter->drawPolygon(screenPolygon);

            pen.setColor(color);
            painter->setPen(pen);
            painter->setBrush(brush);
            screenPolygon.translate(0, -1);
            painter->drawPolygon(screenPolygon);
            if (false) {
                QPainterPathStroker stroker;

                auto scene = static_cast<CellScene*>(this->scene());
                auto view = static_cast<CellView*>(scene->views().first());
                qreal zoom = view->zoomable()->scale();
                zoom = qMin(zoom, 1.0);

                stroker.setWidth(20 / zoom);
                QPainterPath path;
                screenPolygon += screenPolygon[0];
                path.addPolygon(screenPolygon);
                painter->strokePath(stroker.createStroke(path), pen);
            }
            break;
        case ObjectGeometryType::Polyline:
        {
            painter->drawPolyline(screenPolygon);

            pen.setColor(color);
            painter->setPen(pen);
            painter->setBrush(brush);
            screenPolygon.translate(0, -1);
            painter->drawPolyline(screenPolygon);
#if 1
            int width = mObject->polylineWidth();
            if (width <= 0)
                break;
            QPolygonF screenPolygon2;
#endif
#if 0
            for (int i = 0; i < mPolygon.size() - 1; i++) {
                QPointF& p1 = mPolygon[i];
                QPointF& p2 = mPolygon[i + 1];
                QVector2D v(p2 - p1);
                v.normalize();
                qreal xp = v.x() * qCos(qDegreesToRadians(90.0)) - v.y() * qSin(qDegreesToRadians(90.0));
                qreal yp = v.x() * qSin(qDegreesToRadians(90.0)) + v.y() * qCos(qDegreesToRadians(90.0));
                screenPolygon2.clear();
                screenPolygon2 += mRenderer->tileToPixelCoords(p1.x() + mDragOffset.x() + xp * width/2, p1.y() + mDragOffset.y() + yp * width / 2);
                screenPolygon2 += mRenderer->tileToPixelCoords(p2.x() + mDragOffset.x() + xp * width/2, p2.y() + mDragOffset.y() + yp * width / 2);
                screenPolygon2 += mRenderer->tileToPixelCoords(p2.x() + mDragOffset.x() - xp * width/2, p2.y() + mDragOffset.y() - yp * width / 2);
                screenPolygon2 += mRenderer->tileToPixelCoords(p1.x() + mDragOffset.x() - xp * width/2, p1.y() + mDragOffset.y() - yp * width / 2);
                screenPolygon2 += screenPolygon2[0];
                painter->drawPolyline(screenPolygon2);
            }
#endif
#if 1
            if (mPolylineOutline.isEmpty())
                break;
            for (const QPointF& op : mPolylineOutline) {
                screenPolygon2 += mRenderer->tileToPixelCoords(op + mDragOffset);
            }
            screenPolygon2 += screenPolygon2[0];
            painter->drawPolyline(screenPolygon2);
#endif
#if 0
            ClipperLib::ClipperOffset offset;
            ClipperLib::Path path;
            for (int i = 0; i < mPolygon.size(); i++) {
                QPointF& p1 = mPolygon[i];
                path << ClipperLib::IntPoint((p1.x() + mDragOffset.x()) * 100, (p1.y() + mDragOffset.y()) * 100);
                if (/*i < mPolygon.size() - 1 && */(width % 2) != 0) {
//                    QPointF& p1 = mPolygon[i];
//                    QPointF& p2 = mPolygon[i + 1];

//                    // Calculate a perpedicular vector to the line segment.
//                    QVector2D v(p2 - p1);
//                    v.normalize();
//                    qreal xp = v.x() * qCos(qDegreesToRadians(90.0)) - v.y() * qSin(qDegreesToRadians(90.0));
//                    qreal yp = v.x() * qSin(qDegreesToRadians(90.0)) + v.y() * qCos(qDegreesToRadians(90.0));
                    ClipperLib::IntPoint cp = path[path.size()-1];
                    path[path.size()-1] = ClipperLib::IntPoint(cp.X + 50, cp.Y + 50);
                }
            }
            offset.AddPath(path, ClipperLib::JoinType::jtMiter, ClipperLib::EndType::etOpenButt);
            ClipperLib::Paths paths;
            offset.Execute(paths, width * 100 / 2.0);
            for (const auto &path : paths) {
                screenPolygon2.clear();
                for (const auto& cp : path) {
                    screenPolygon2 += mRenderer->tileToPixelCoords(cp.X / 100.0, cp.Y / 100.0);
                }
                screenPolygon2 += screenPolygon2[0];
                painter->drawPolyline(screenPolygon2);
            }
#endif
            break;
        }
        }
#if 0
        if (mAddPointIndex != -1) {
            auto scene = static_cast<CellScene*>(this->scene());
            auto view = static_cast<CellView*>(scene->views().first());
            qreal zoom = view->zoomable()->scale();
            zoom = qMin(zoom, 1.0);

            painter->drawRect(mAddPointPos.x() - 5/zoom, mAddPointPos.y() -5/zoom, (int)(10 / zoom), (int)(10 / zoom));
        }
#endif
#if 0
        if (isPolygon())
        {
            QPolygonF polygon;

            int minX = std::numeric_limits<int>::max();
            int minY = std::numeric_limits<int>::max();
            int maxX = std::numeric_limits<int>::min();
            int maxY = std::numeric_limits<int>::min();
            for (const auto& point : mObject->points()) {
                minX = std::min(minX, point.x);
                minY = std::min(minY, point.y);
                maxX = std::max(maxX, point.x);
                maxY = std::max(maxY, point.y);
                polygon += {qreal(point.x), qreal(point.y)};
            }
            polygon += polygon[0];
#if 0
            OutlineGrid outlineGrid;
            outlineGrid.EXTEND = false;
            outlineGrid.setSize(maxX - minX + 1, maxY - minY + 1);
            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    if (polygon.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                        outlineGrid.setInner(x - minX, y - minY);
                    }
                }
            }
            outlineGrid.trace(true, [&](QPolygon& nodes) {
                QPolygonF screenPolygon = mRenderer->tileToPixelCoords(nodes.translated(minX, minY)/*.translated(mDragOffset)*/);
                painter->drawPolygon(screenPolygon);
            });

            outlineGrid.trace(false, [&](QPolygon& nodes) {
                QVector<PolygonPartition::Vertex> p;
                QPolygonF polygonF;
                // 'nodes' is clockwise order
                // we require counter-clockwise
                // the first node must be convex (it's a north-west corner)
                p += { nodes[0].x(), nodes[0].y(), PolygonPartition::VertexType::Convex };
                polygonF += nodes[0];
                for (int i = nodes.size() - 1; i > 0; i--) {
                    p += { nodes[i].x(), nodes[i].y(), PolygonPartition::VertexType::Collinear };
                    polygonF += nodes[i];
                }
//                p += p[0]; // last is same as the first
                for (int i = 0; i < p.size(); i++) {
                    const auto& p1 = p[i];
                    auto& p2 = p[(i+1) % p.size()];
                    const auto& p3 = p[(i+2) % p.size()];
                    int dx1 = p1.x - p2.x;
                    int dy1 = p1.y - p2.y;
                    int dx3 = p3.x - p2.x;
                    int dy3 = p3.y - p2.y;
                    int normalX = dx1 + dx3;
                    int normalY = dy1 + dy3;
                    if (normalX == 0 && normalY == 0) {
                        p2.type = PolygonPartition::VertexType::Collinear;
                        qDebug() << (i+1) % p.size() << "Collinear";
                    } else {
                        if (polygonF.containsPoint(QPoint(p2.x + normalX / 2.0, p2.y + normalY / 2.0), Qt::OddEvenFill)) {
                            p2.type = PolygonPartition::VertexType::Convex;
                            qDebug() << (i+1) % p.size() << "Convex";
                        } else {
                            p2.type = PolygonPartition::VertexType::Concave;
                            qDebug() << (i+1) % p.size() << "Concave";
                        }
                    }
                }

                PolygonPartition partition;
#if 1 // minimum partition
                try {
                    partition.compute_minimum_partition(p);
                    for (int i = 0; i < partition.min1.size() - 1; i += 2) {
                        QPoint pA = partition.min1[i];
                        QPoint pB = partition.min1[i + 1];
                        QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + pA.x(), minY + pA.y()));
                        QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + pB.x(), minY + pB.y()));
                        painter->drawLine(p1, p2);
                    }
                    for (int i = 0; i < partition.min2.size() - 1; i += 2) {
                        QPoint pA = partition.min2[i];
                        QPoint pB = partition.min2[i + 1];
                        QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + pA.x(), minY + pA.y()));
                        QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + pB.x(), minY + pB.y()));
                        painter->drawLine(p1, p2);
                    }
                }  catch (std::exception e) {
                    for (const auto& chord : partition.horizontal_chords) {
                        QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v1].x, minY + p[chord.v1].y));
                        QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v2].x, minY + p[chord.v2].y));
                        painter->drawLine(p1, p2);
                    }
                    for (const auto& chord : partition.vertical_chords) {
                        QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v1].x, minY + p[chord.v1].y));
                        QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v2].x, minY + p[chord.v2].y));
                        painter->drawLine(p1, p2);
                    }
                }
            });
#endif
#if 0 // maximum partition
                partition.compute_maximum_partition(p);
                for (const auto& chord : partition.horizontal_chords) {
                    QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v1].x, minY + p[chord.v1].y));
                    QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v2].x, minY + p[chord.v2].y));
                    painter->drawLine(p1, p2);
                }
                for (const auto& chord : partition.vertical_chords) {
                    QPointF p1 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v1].x, minY + p[chord.v1].y));
                    QPointF p2 = mRenderer->tileToPixelCoords(QPoint(minX + p[chord.v2].x, minY + p[chord.v2].y));
                    painter->drawLine(p1, p2);
                }
            });
#endif
#endif
#if 0
            QRegion region;
            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    if (polygon.containsPoint(QPointF(x + 0.5, y + 0.5), Qt::OddEvenFill)) {
                        int x2 = x + 1;
                        while (x2 <= maxX && polygon.containsPoint(QPointF(x2 + 0.5, y + 0.5), Qt::OddEvenFill)) {
                            x2++;
                        }
                        region += QRect(x, y, x2 - x, 1);
                    }
                }
            }
            region = cleanupRegion(region);
            for (const QRect& rect : region.rects()) {
                QPolygonF screenPolygon = mRenderer->tileToPixelCoords(rect/*.translated(mDragOffset)*/);
                painter->drawPolygon(screenPolygon);
            }
#endif
#if 0
            QPainterPath painterPath;
            painterPath.addRegion(region);
            QList<QPolygonF> regionPolygon = painterPath.toSubpathPolygons();
            for (const QPolygonF &subPoly : regionPolygon) {
                QPolygonF screenPolygon = mRenderer->tileToPixelCoords(subPoly.translated(mDragOffset));
                painter->drawPolygon(screenPolygon);
            }
#endif
        }
#endif
        return;
    }

    QColor color = mObject->group()->color();
    if (mIsSelected)
        color = QColor(0x33,0x99,0xff/*,255/8*/);
    if (isMouseOverHighlighted())
        color = color.lighter();
    mRenderer->drawFancyRectangle(painter, tileBounds(), color, mObject->level());

    /**
      * There is something badly broken with the OpenGL line stroking when the
      * painter's transform's dx/dy is a large-ish negative number. The lines
      * sometimes aren't drawn on different edges (clipping issue?), sometimes
      * the lines get double-width, and the dash pattern is messed up unless
      * the view is scrolled to just the right position . It seems likely to be
      * the result of some rounding issue. The cure is to translate the painter
      * back to an origin of 0,0.
      */
    QRectF bounds = mBoundingRect.translated(-mBoundingRect.topLeft());
    painter->translate(mBoundingRect.topLeft());

#ifdef _DEBUG
    if (!mIsEditable)
        painter->drawRect(bounds);
#endif

    if (mIsEditable) {
        QLineF top(bounds.topLeft(), bounds.topRight());
        QLineF left(bounds.topLeft(), bounds.bottomLeft());
        QLineF right(bounds.topRight(), bounds.bottomRight());
        QLineF bottom(bounds.bottomLeft(), bounds.bottomRight());

        QPen dashPen(Qt::DashLine);
        dashPen.setCosmetic(true);
        dashPen.setDashOffset(qMax(qreal(0), mBoundingRect.x()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << top << bottom);

        dashPen.setDashOffset(qMax(qreal(0), mBoundingRect.y()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << left << right);
    }

    painter->translate(-mBoundingRect.topLeft());
}

void ObjectItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if ((++mHoverRefCount == 1) && hoverToolCurrent()) {
        update();

        mLabel->synch();
    }
}

// http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
static float distanceOfPointToLineSegment(QVector2D p1, QVector2D p2, QVector2D p)
{
    QVector2D n = p2 - p1;
    QVector2D pa =  p1 - p;

    float c = QVector2D::dotProduct(n, pa);

    if (c > 0.0f)
        return QVector2D::dotProduct(pa, pa);

    QVector2D bp = p - p2;

    if (QVector2D::dotProduct(n, bp) > 0.0f)
        return QVector2D::dotProduct(bp, bp);

    QVector2D e = pa - n * (c / QVector2D::dotProduct(n, n));

    return QVector2D::dotProduct(e, e);
}

#include <QtMath>

void ObjectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!isEditable()) {
        if (mAddPointIndex != -1) {
            mAddPointIndex = -1;
            update();
        }
        return;
    }

    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    zoom = qMin(zoom, 1.0);

    QPolygonF scenePoly = mRenderer->tileToPixelCoords(mPolygon, 0);

    // Don't add points near other points
    for (int i = 0; i < scenePoly.size(); i++) {
        float d = QVector2D(event->scenePos()).distanceToPoint(QVector2D(scenePoly[i]));
        if (d < 20 / (float) zoom) {
            if (mAddPointIndex != -1) {
                mAddPointIndex = -1;
                update();
            }
            return;
        }
    }

    // Find the line segment the mouse pointer is over
    int closestIndex = -1;
    float closestDist = 10000;
    int size = scenePoly.size();
    if (isPolygon() == false)
        size--;
    for (int i = 0; i < scenePoly.size(); i++) {
        QVector2D p1(scenePoly[i]);
        QVector2D p2(scenePoly[(i+1) % scenePoly.size()]);
//        QVector2D dir = (p2 - p1).normalized();
//        float d = QVector2D(event->scenePos()).distanceToLine(p1, dir);
        float d = distanceOfPointToLineSegment(p1, p2, QVector2D(event->scenePos()));
        d = qSqrt(qAbs(d));
        if (d < 10 / (float)zoom && d < closestDist) {
            closestIndex = i;
            closestDist = d;
        }
    }
    if (closestIndex != -1) {
        mAddPointIndex = closestIndex;
        mAddPointPos = event->scenePos();
//        qDebug() << "mAddPointIndex " << mAddPointIndex << " dist " << closestDist;
        update();
    } else if (mAddPointIndex != -1) {
        mAddPointIndex = -1;
        update();
    }
}

void ObjectItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    Q_ASSERT(mHoverRefCount > 0);
    if (--mHoverRefCount == 0) {
        mAddPointIndex = -1;
        update();

        mLabel->synch();
    }
}

QPainterPath ObjectItem::shape() const
{
    if (mObject->points().isEmpty() == false) {
        QPolygonF polygon = mRenderer->tileToPixelCoords(mPolygon, 0);

        if (isPolygon())
            polygon += polygon[0];

        auto scene = static_cast<CellScene*>(this->scene());
        auto view = static_cast<CellView*>(scene->views().first());
        qreal zoom = view->zoomable()->scale();
        zoom = qMin(zoom, 1.0);

        QPainterPath path;
        if (isPoint()) {
    //        path.addEllipse(polygon[0], 10, 10);
            path.addRect(polygon[0].x() - 10, polygon[0].y() - 10, 20, 20);
            return path;
        }
        path.addPolygon(polygon);
        if (isPolygon()) {
            return path;
        }
        QPainterPathStroker stroker;
        stroker.setWidth(20 / zoom);
        return stroker.createStroke(path);
    }

    QPolygonF polygon = mRenderer->tileToPixelCoords(tileBounds(), mObject->level());

    QPainterPath path;
    path.addPolygon(polygon);
    return path;
}

void ObjectItem::setEditable(bool editable)
{
    if (editable == mIsEditable)
        return;

    mIsEditable = editable;

    mResizeHandle->setVisible(mIsEditable && (mObject->geometryType() == ObjectGeometryType::INVALID));

    if (mIsEditable)
        setCursor(Qt::SizeAllCursor);
    else
        unsetCursor();

    update();
}

void ObjectItem::setSelected(bool selected)
{
    if (selected == mIsSelected)
        return;

    mIsSelected = selected;

    update();
}

void ObjectItem::synchWithObject()
{
    QString toolTip = mObject->name();
    if (toolTip.isEmpty())
        toolTip = QLatin1String("<no name>");
    QString type = mObject->type()->name();
    if (type.isEmpty())
        type = QLatin1String("<no type>");
    toolTip += QLatin1String(" (") + type + QLatin1String(")");
    setToolTip(toolTip);

    if (mObject->geometryType() == ObjectGeometryType::INVALID) {
        QRectF tileBounds(mObject->pos() + mDragOffset, mObject->size() + mResizeDelta);
        QRectF sceneBounds = ::boundingRect(mRenderer, tileBounds, mObject->level()).adjusted(-20, -20, 20, 20);
        if (sceneBounds != mBoundingRect) {
            prepareGeometryChange();
            mBoundingRect = sceneBounds;
        }
    }
    mResizeHandle->synch();

    mLabel->synch();

    mPolygon.clear();
    mPolylineOutline.clear();
    mAddPointIndex = -1;

    switch (mObject->geometryType()) {
    case ObjectGeometryType::INVALID:
        return;
    case ObjectGeometryType::Point: {
        WorldCellObjectPoint center = mObject->points()[0];
        mPolygon += { qreal(center.x) , qreal(center.y) };
        QPointF scenePos = mRenderer->tileToPixelCoords(mPolygon[0] + mDragOffset);
        QRectF bounds(scenePos.x() - 10, scenePos.y() - 10, 20, 20);
        if (bounds != mBoundingRect) {
            prepareGeometryChange();
            mBoundingRect = bounds;
        }
        return;
    }
    case ObjectGeometryType::Polygon:
        for (const auto& point : mObject->points()) {
            mPolygon += QPoint(point.x, point.y);
        }
        break;
    case ObjectGeometryType::Polyline:
        for (const auto& point : mObject->points()) {
            mPolygon += QPoint(point.x, point.y);
        }
        if (mObject->polylineWidth() > 0) {
            mPolylineOutline = createPolylineOutline();
            if (mPolylineOutline.empty())
                break;
            QRectF bounds = mRenderer->tileToPixelCoords(mPolylineOutline.translated(mDragOffset)).boundingRect().adjusted(-20, -20, 20, 20);
            if (bounds != mBoundingRect) {
                prepareGeometryChange();
                mBoundingRect = bounds;
            }
            return;
        }
        break;
    }

    QRectF bounds = mRenderer->tileToPixelCoords(mPolygon.translated(mDragOffset)).boundingRect().adjusted(-20, -20, 20, 20);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void ObjectItem::setDragOffset(const QPointF &offset)
{
    mDragOffset = offset;
    synchWithObject();
}

void ObjectItem::setResizeDelta(const QSizeF &delta)
{
    mResizeDelta = delta;
    synchWithObject();
}

QRectF ObjectItem::tileBounds() const
{
    return QRectF(mObject->pos() + mDragOffset, mObject->size() + mResizeDelta);
}

bool ObjectItem::isMouseOverHighlighted() const
{
    return (mHoverRefCount > 0) && hoverToolCurrent();
}

bool ObjectItem::hoverToolCurrent() const
{
    return SelectMoveObjectTool::instance()->isCurrent() ||
            (EditPolygonObjectTool::instance().isCurrent() && (isRectangle() == false));
}

bool ObjectItem::isPoint() const
{
    return mObject->isPoint();
}

bool ObjectItem::isPolygon() const
{
    return mObject->isPolygon();
}

bool ObjectItem::isPolyline() const
{
    return mObject->isPolyline();
}

bool ObjectItem::isRectangle() const
{
    return mObject->isRectangle();
}

int ObjectItem::pointAt(qreal sceneX, qreal sceneY)
{
    auto scene = static_cast<CellScene*>(this->scene());
    auto view = static_cast<CellView*>(scene->views().first());
    qreal zoom = view->zoomable()->scale();
    zoom = qMin(zoom, 1.0);

    QPolygonF scenePoly = mRenderer->tileToPixelCoords(mPolygon, 0);
    for (int i = 0; i < scenePoly.size(); i++) {
        float d = QVector2D(QPointF(sceneX, sceneY)).distanceToPoint(QVector2D(scenePoly[i]));
        if (d < 10 / (float) zoom) {
            return i;
        }
    }
    return -1;
}

void ObjectItem::movePoint(int pointIndex, const WorldCellObjectPoint &point)
{
    mObject->setPoint(pointIndex, point);
    QRectF boundingRect = mBoundingRect;
    synchWithObject();
    if (boundingRect == mBoundingRect) {
        update();
    }
}

QPolygonF ObjectItem::createPolylineOutline()
{
    ClipperLib::ClipperOffset offset;
    ClipperLib::Path path;
    int SCALE = 100;
    for (int i = 0; i < mObject->points().size(); i++) {
        WorldCellObjectPoint p1 = mObject->points()[i];
        path << ClipperLib::IntPoint(p1.x * SCALE, p1.y * SCALE);
        if (/*i < mPolygon.size() - 1 && */(mObject->polylineWidth() % 2) != 0) {
//                    QPointF& p1 = mPolygon[i];
//                    QPointF& p2 = mPolygon[i + 1];

//                    // Calculate a perpedicular vector to the line segment.
//                    QVector2D v(p2 - p1);
//                    v.normalize();
//                    qreal xp = v.x() * qCos(qDegreesToRadians(90.0)) - v.y() * qSin(qDegreesToRadians(90.0));
//                    qreal yp = v.x() * qSin(qDegreesToRadians(90.0)) + v.y() * qCos(qDegreesToRadians(90.0));
            ClipperLib::IntPoint cp = path[path.size()-1];
            path[path.size()-1] = ClipperLib::IntPoint(cp.X + SCALE / 2, cp.Y + SCALE / 2);
        }
    }
    offset.AddPath(path, ClipperLib::JoinType::jtMiter, ClipperLib::EndType::etOpenButt);
    ClipperLib::Paths paths;
    offset.Execute(paths, mObject->polylineWidth() * SCALE / 2.0);
    QPolygonF result;
    if (paths.empty()) {
        return result;
    }
    ClipperLib::Path cPath = paths.at(0);
    for (const auto &cPoint : cPath) {
        result << QPointF(cPoint.X / (qreal) SCALE, cPoint.Y / (qreal) SCALE);
    }
    return result;
}

/////

ObjectPointHandle::ObjectPointHandle(ObjectItem *objectItem, int pointIndex)
    : QGraphicsItem(objectItem)
    , mObjectItem(objectItem)
    , mPointIndex(pointIndex)
    , mSizeItemBG(nullptr)
    , mSizeItem(nullptr)
{
    setFlags(QGraphicsItem::ItemIsMovable |
             QGraphicsItem::ItemSendsGeometryChanges |
             QGraphicsItem::ItemIgnoresTransformations |
             QGraphicsItem::ItemIgnoresParentOpacity);
    setAcceptHoverEvents(true);
}

QRectF ObjectPointHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void ObjectPointHandle::paint(QPainter *painter, const QStyleOptionGraphicsItem*, QWidget *)
{
    painter->setBrush(mHoverRefCount ? Qt::red : Qt::blue);
    painter->setPen(isSelected() ? Qt::white : Qt::black);
    painter->drawRect(QRectF(-5, -5, 10, 10));
}

bool ObjectPointHandle::isSelected() const
{
    CellScene *scene = mObjectItem->mScene;
    return scene->document()->selectedObjectPoints().contains(mPointIndex);
}

void ObjectPointHandle::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mousePressEvent(event);

    if (event->button() == Qt::LeftButton) {
        mOldPos = geometryPoint();
        mMoveAllPoints = (event->modifiers() & Qt::ShiftModifier) != 0;
        mCancelMove = false;

        CellScene *scene = mObjectItem->mScene;
        QList<int> selection = scene->document()->selectedObjectPoints();
        if (event->modifiers() & Qt::ControlModifier) {
            if (isSelected()) {
                selection.removeOne(mPointIndex);
            } else {
                selection += mPointIndex;
            }
        } else {
            if (isSelected() == false) {
                selection.clear();
                selection += mPointIndex;
            }
        }
        scene->document()->setSelectedObjectPoints(selection);
        if (isSelected() && (selection.size() == 1) && EditPolygonObjectTool::instance().isCurrent()) {
            updateSizeLabel();
        }
    }

    if (event->button() == Qt::RightButton) {
        if (event->buttons() & Qt::LeftButton) {
            if (mOldPos != geometryPoint()) {
                mObjectItem->movePoint(mPointIndex, mOldPos);
                setPos(mObjectItem->mRenderer->tileToPixelCoords(mOldPos.x, mOldPos.y));
                mCancelMove = true;
            }
        }
    }

    // Stop the object context menu messing us up.
    event->accept();
}

void ObjectPointHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);

    if (mSizeItem != nullptr) {
        delete mSizeItemBG;
        mSizeItemBG = nullptr;
        mSizeItem = nullptr;
    }

    if (event->button() == Qt::LeftButton) {
        mCancelMove = false;
    }

    if (event->button() == Qt::LeftButton && (mOldPos != geometryPoint())) {
        WorldDocument *document = mObjectItem->mScene->worldDocument();
        int objectIndex = mObjectItem->object()->index();
        WorldCellObjectPoint newPos = geometryPoint();
        mObjectItem->movePoint(mPointIndex, mOldPos);
        setPos(mObjectItem->mRenderer->tileToPixelCoords(mOldPos.x, mOldPos.y));

        WorldCellObjectPoints coords = mObjectItem->object()->points();
        int pointIndex = coords.indexOf(newPos);
        if (pointIndex != -1) {
            // Dragging a point onto another point deletes all points in between plus the moved point.
            int p1 = std::min(mPointIndex, pointIndex);
            int p2 = std::max(mPointIndex, pointIndex);
            if (mObjectItem->isPolygon() && (p2 == coords.size() - 1) && (p1 == 0)) {
                coords.removeAt(mPointIndex);
            } else if (mPointIndex > pointIndex) {
                for (int i = mPointIndex; i > pointIndex; i--) {
                    coords.removeAt(i);
                }
            } else {
                for (int i = pointIndex - 1; i >= mPointIndex; i--) {
                    coords.removeAt(i);
                }
            }
            if (mObjectItem->isPolygon()) {
                if (coords.size() < 3) {
                    return;
                }
            } else if (mObjectItem->isPolyline()) {
                if (coords.size() < 2) {
                    return;
                }
            }
            document->setCellObjectPoints(mObjectItem->object()->cell(), objectIndex, coords);
        } else if (mMoveAllPoints) {
            WorldCellObjectPoints coords = mObjectItem->object()->points();
            coords.translate(int(newPos.x - mOldPos.x), int(newPos.y - mOldPos.y));
            document->setCellObjectPoints(mObjectItem->object()->cell(), objectIndex, coords);
        } else {
            document->moveCellObjectPoint(mObjectItem->object()->cell(), objectIndex, mPointIndex, newPos);
        }
    }

    // Stop the context-menu messing us up.
    event->accept();
}

QVariant ObjectPointHandle::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (!mObjectItem->mSyncing) {
        auto* renderer = mObjectItem->mRenderer;

        if (change == ItemPositionChange) {
            if (mCancelMove) {
                return mObjectItem->mRenderer->tileToPixelCoords(mOldPos.x, mOldPos.y);
            }
            bool snapToGrid = true;

            // Calculate the absolute pixel position
            const QPointF itemPos = mObjectItem->pos();
            QPointF pixelPos = value.toPointF() + itemPos;

            // Calculate the new coordinates in tiles
            QPointF tileCoords = renderer->pixelToTileCoords(pixelPos, 0);

            const QPointF objectPos = { 0, 0 };
            tileCoords -= objectPos;
#if 0
            tileCoords.setX(qMax(tileCoords.x(), qreal(0)));
            tileCoords.setY(qMax(tileCoords.y(), qreal(0)));
#endif
            if (snapToGrid)
                tileCoords = tileCoords.toPoint();
            tileCoords += objectPos;

            return renderer->tileToPixelCoords(tileCoords, 0) - itemPos;
        }
        else if (change == ItemPositionHasChanged) {
            const QPointF newPos = value.toPointF();
            QPointF tileCoords = renderer->pixelToTileCoords(newPos, 0);
            WorldCellObjectPoint point(tileCoords.x(), tileCoords.y());
            mObjectItem->movePoint(mPointIndex, point);
            if (isSelected() && EditPolygonObjectTool::instance().isCurrent()) {
                updateSizeLabel();
            }
        }
    }

    return QGraphicsItem::itemChange(change, value);
}

void ObjectPointHandle::updateSizeLabel()
{
    int length;
    WorldCellObject *object = mObjectItem->object();
    if (pointIndex() == 0) {
        auto p1 = object->points().at(1);
        auto p2 = object->points().at(0);
        length = QVector2D(p2.x - p1.x, p2.y - p1.y).length();
    } else if (pointIndex() == object->points().size() - 1) {
        auto p1 = object->points().at(object->points().size() - 2);
        auto p2 = object->points().at(object->points().size() - 1);
        length = QVector2D(p2.x - p1.x, p2.y - p1.y).length();
    } else {
        return;
    }
    if (mSizeItem == nullptr) {
        mSizeItemBG = new QGraphicsRectItem(this);
        mSizeItemBG->setBrush(Qt::lightGray);
        mSizeItemBG->setPos(20.0, 0.0);
        mSizeItem = new QGraphicsSimpleTextItem(mSizeItemBG);
        mSizeItem->setPos(4.0, 4.0);
    }
    mSizeItem->setText(QStringLiteral("length %1").arg(length));
    mSizeItemBG->setRect(QRectF(QPointF(), mSizeItem->boundingRect().size() + QSizeF(8.0, 8.0)));
}

/////

SpawnPointItem::SpawnPointItem(WorldCellObject *object, CellScene *scene, QGraphicsItem *parent) :
    ObjectItem(object, scene, parent)
{
//    setFlag(ItemIgnoresTransformations);
}

QRectF SpawnPointItem::boundingRect() const
{
    QRectF bounds = ObjectItem::boundingRect();
    QPointF pos = mObject->pos() + mDragOffset;
    bounds |= mRenderer->boundingRect(QRect(pos.x() - 3, pos.y() - 3, 1, 1), mObject->level());
    bounds |= mRenderer->boundingRect(QRect(pos.x(), pos.y(), 1, 1), mObject->level());
    return bounds;
}

void SpawnPointItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
{
    ObjectItem::paint(painter, option, widget);

    QPen pen(Qt::black);
    pen.setCosmetic(true);
    painter->setPen(pen);
    QColor color = mObject->group()->color();
    color.setAlpha(200);

    int level = mObject->level();

    qreal inset = 0.15;
    QPointF pos = mObject->pos() + mDragOffset;

    // Bottom-right
    QPolygonF poly;
    poly << mRenderer->tileToPixelCoords(pos + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 0), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.darker(125));
    painter->drawPolygon(poly);
    poly.clear();

    // Bottom-left
    poly << mRenderer->tileToPixelCoords(pos + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(0, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.darker(115));
    painter->drawPolygon(poly);
    poly.clear();

    // Top-right
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 0), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(3, 3) + QPointF(0.5, 0.5), level);
    poly << poly.first();
    painter->setBrush(color.lighter(100));
    painter->drawPolygon(poly);
    poly.clear();

    // Top-left
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(1-inset, 1-inset), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(3, 3) + QPointF(0.5, 0.5), level);
    poly << mRenderer->tileToPixelCoords(pos - QPointF(1.5, 1.5) + QPointF(0, 1-inset), level);
    poly << poly.first();
    painter->setBrush(color.lighter(115));
    painter->drawPolygon(poly);
    poly.clear();
}

bool SpawnPointItem::hoverToolCurrent() const
{
    return SpawnPointTool::instancePtr()->isCurrent() ||
            SelectMoveObjectTool::instance()->isCurrent();
}

/////

SubMapItem::SubMapItem(MapComposite *map, WorldCellLot *lot, MapRenderer *renderer, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mMap(map)
    , mRenderer(renderer)
    , mLot(lot)
    , mIsEditable(false)
    , mIsMouseOver(false)
{
    setAcceptHoverEvents(true);
    mBoundingRect = mMap->boundingRect(mRenderer);

    QString mapFileName = mMap->mapInfo()->path();
#if 0
    if (!lot->cell()->mapFilePath().isEmpty()) {
        QDir mapDir = QFileInfo(lot->cell()->mapFilePath()).absoluteDir();
        mapFileName = mapDir.relativeFilePath(mapFileName);
    }
#endif
    QString toolTip = QDir::toNativeSeparators(mapFileName);
    toolTip += QLatin1String(" (lot)");
    setToolTip(toolTip);

    checkValidPos();
}

QRectF SubMapItem::boundingRect() const
{
    return mBoundingRect;
}

void SubMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    QRect tileBounds(mMap->origin(), mMap->map()->size());
    QColor color = Qt::darkGray;
    if (mIsEditable)
        color = QColor(0x33,0x99,0xff/*,255/8*/);
    if (!mIsValidPos)
        color = QColor(255, 0, 0);
    if (mIsMouseOver)
        color = color.lighter();
    mRenderer->drawFancyRectangle(painter, tileBounds,
                                  color,
                                  mMap->levelOffset());

    /* See note in ObjectItem::paint about OpenGL rendering bug. */
    QRectF bounds = mBoundingRect.translated(-mBoundingRect.topLeft());
    painter->translate(mBoundingRect.topLeft());

#ifdef _DEBUG
    if (!mIsEditable)
        painter->drawRect(bounds);
#endif

    if (mIsEditable) {
        QLineF top(bounds.topLeft(), bounds.topRight());
        QLineF left(bounds.topLeft(), bounds.bottomLeft());
        QLineF right(bounds.topRight(), bounds.bottomRight());
        QLineF bottom(bounds.bottomLeft(), bounds.bottomRight());

        QPen dashPen(Qt::DashLine);
        dashPen.setDashOffset(qMax(qreal(0), mBoundingRect.x()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << top << bottom);

        dashPen.setDashOffset(qMax(qreal(0), mBoundingRect.y()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << left << right);
    }
}

void SubMapItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (SubMapTool::instance()->isCurrent()) {
        mIsMouseOver = true;
        update();
    }
}

void SubMapItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event)
    if (mIsMouseOver) {
        mIsMouseOver = false;
        update();
    }
}

QPainterPath SubMapItem::shape() const
{
    // FIXME: MapRenderer should return a poly for a cell rectangle (like MapRenderer::shape)
    int level = mMap->levelOffset();
    const QRect rect(mMap->origin(), mMap->map()->size() + QSize(1, 1));
    const QPointF topLeft = mRenderer->tileToPixelCoords(rect.topLeft(), level);
    const QPointF topRight = mRenderer->tileToPixelCoords(rect.topRight(), level);
    const QPointF bottomRight = mRenderer->tileToPixelCoords(rect.bottomRight(), level);
    const QPointF bottomLeft = mRenderer->tileToPixelCoords(rect.bottomLeft(), level);
    QPolygonF polygon;
    polygon << topLeft << topRight << bottomRight << bottomLeft;

    QPainterPath path;
    path.addPolygon(polygon);
    return path;
}

void SubMapItem::setEditable(bool editable)
{
    if (editable == mIsEditable)
        return;

    mIsEditable = editable;

    if (mIsEditable)
        setCursor(Qt::SizeAllCursor);
    else
        unsetCursor();

    update();
}

void SubMapItem::subMapMoved()
{
    checkValidPos();

    QRectF bounds = mMap->boundingRect(mRenderer);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void SubMapItem::checkValidPos()
{
    mIsValidPos = true;
    foreach (ObjectGroup *og, mMap->map()->objectGroups()) {
        if (og->name().endsWith(QLatin1String("RoomDefs"))) {
            foreach (MapObject *o, og->objects()) {

                int x = qFloor(o->x());
                int y = qFloor(o->y());
                int w = qCeil(o->x() + o->width()) - x;
                int h = qCeil(o->y() + o->height()) - y;
                QRect roomRect(x, y, w, h);
                roomRect.translate(mLot->pos());

                if (!QRect(0, 0, 300, 300).contains(roomRect)) {
                    mIsValidPos = false;
                    return;
                }
            }
        }
    }
}

/////

CellRoadItem::CellRoadItem(CellScene *scene, Road *road)
    : QGraphicsItem()
    , mScene(scene)
    , mRoad(road)
    , mSelected(false)
    , mEditable(false)
    , mDragging(false)
{
    synchWithRoad();
}

QRectF CellRoadItem::boundingRect() const
{
    return mBoundingRect;
}

QPainterPath CellRoadItem::shape() const
{
    QPainterPath path;
    path.addPolygon(polygon());
    return path;
}

void CellRoadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QColor c = Qt::blue;
    if (mSelected)
        c = Qt::green;
    if (mEditable)
        c = Qt::yellow;
    painter->setPen(c);
    painter->drawPath(shape());
}

void CellRoadItem::synchWithRoad()
{
    QPoint offset = mDragging ? mDragOffset : QPoint();
    QPolygonF polygon = mScene->roadRectToScenePolygon(mRoad->bounds().translated(offset));
    if (polygon != mPolygon) {
        mPolygon = polygon;
    }

    QRectF bounds = polygon.boundingRect();
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void CellRoadItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void CellRoadItem::setEditable(bool editable)
{
    mEditable = editable;
    update();
}

void CellRoadItem::setDragging(bool dragging)
{
    mDragging = dragging;
    synchWithRoad();
}

void CellRoadItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    synchWithRoad();
}

/////

DnDItem::DnDItem(const QString &path, MapRenderer *renderer, int level, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mMapImage(MapImageManager::instance()->getMapImage(path))
    , mRenderer(renderer)
    , mLevel(level)
{
    setHotSpot(mMapImage->mapInfo()->width() / 2, mMapImage->mapInfo()->height() / 2);
}

QRectF DnDItem::boundingRect() const
{
    return mBoundingRect;
}

void DnDItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setOpacity(0.5);
    QRectF target = mBoundingRect;
    QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
    painter->drawImage(target, mMapImage->image(), source);
    painter->setOpacity(effectiveOpacity());

    QRect tileBounds(mPositionInMap.x() - mHotSpot.x(), mPositionInMap.y() - mHotSpot.y(),
                     mMapImage->mapInfo()->width(), mMapImage->mapInfo()->height());
    mRenderer->drawFancyRectangle(painter, tileBounds, Qt::darkGray, mLevel);

#ifdef _DEBUG
    painter->drawRect(mBoundingRect);
#endif
}

QPainterPath DnDItem::shape() const
{
    // FIXME: need polygon
    return QGraphicsItem::shape();
}

void DnDItem::setTilePosition(QPoint tilePos)
{
    mPositionInMap = tilePos;

    qreal tileScale = mRenderer->boundingRect(QRect(0,0,1,1)).width() / (qreal)mMapImage->tileSize().width();
    QSize scaledImageSize(mMapImage->image().size() / mMapImage->scale() * tileScale);
    QRectF bounds = QRectF(-mMapImage->tileToImageCoords(mHotSpot) / mMapImage->scale() * tileScale,
                           scaledImageSize);
    bounds.translate(mRenderer->tileToPixelCoords(mPositionInMap, mLevel));
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void DnDItem::setHotSpot(const QPoint &pos)
{
    // Position the item so that the top-left corner of the hotspot tile is at the item's origin
    mHotSpot = pos;
    qreal tileScale = mRenderer->boundingRect(QRect(0,0,1,1)).width() / (qreal)mMapImage->tileSize().width();
    QSize scaledImageSize(mMapImage->image().size() / mMapImage->scale() * tileScale);
    mBoundingRect = QRectF(-mMapImage->tileToImageCoords(mHotSpot) / mMapImage->scale() * tileScale, scaledImageSize);
}

QPoint DnDItem::dropPosition()
{
    return mPositionInMap - mHotSpot;
}

MapInfo *DnDItem::mapInfo()
{
    return mMapImage->mapInfo();
}

/////

class DummyGraphicsItem : public QGraphicsItem
{
public:
    DummyGraphicsItem()
        : QGraphicsItem()
    {
        // Since we don't do any painting, we can spare us the call to paint()
        setFlag(QGraphicsItem::ItemHasNoContents);
    }

    // QGraphicsItem
    QRectF boundingRect() const
    {
        return QRectF();
    }
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0)
    {
        Q_UNUSED(painter)
        Q_UNUSED(option)
        Q_UNUSED(widget)
    }
};

///// ///// ///// ///// /////

#define DARKENING_FACTOR 0.6

const int CellScene::ZVALUE_GRID = 10000;
const int CellScene::ZVALUE_ROADITEM_CREATING = 20002;
const int CellScene::ZVALUE_ROADITEM_SELECTED = 20001;
const int CellScene::ZVALUE_ROADITEM_UNSELECTED = 20000;

CellScene::CellScene(QObject *parent)
    : BaseGraphicsScene(CellSceneType, parent)
    , mMap(0)
    , mMapComposite(0)
    , mDocument(0)
    , mRenderer(0)
    , mDnDItem(0)
    , mDarkRectangle(new QGraphicsRectItem)
    , mGridItem(new CellGridItem(this))
    , mMapBordersItem(new QGraphicsPolygonItem)
    , mMapBuildings(new MapBuildings)
    , mMapBuildingsInvalid(true)
    , mPendingFlags(None)
    , mPendingActive(false)
    , mPendingDefer(true)
    , mActiveTool(0)
    , mLightSwitchOverlays(this)
    , mWaterFlowOverlay(new WaterFlowOverlay(this))
    , mDestroying(false)
{
    setBackgroundBrush(Qt::darkGray);

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(DARKENING_FACTOR);
//    addItem(mDarkRectangle);

    // These signals are handled even when the document isn't current
    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(showCellGridChanged(bool)), SLOT(setGridVisible(bool)));
    connect(prefs, SIGNAL(highlightCurrentLevelChanged(bool)), SLOT(setHighlightCurrentLevel(bool)));
    setGridVisible(prefs->showCellGrid());
    connect(prefs, SIGNAL(gridColorChanged(QColor)), SLOT(update()));
    connect(prefs, SIGNAL(showObjectsChanged(bool)), SLOT(showObjectsChanged(bool)));
    connect(prefs, SIGNAL(showObjectNamesChanged(bool)), SLOT(showObjectNamesChanged(bool)));

    mHighlightCurrentLevel = prefs->highlightCurrentLevel();

    QPen pen(QColor(128, 128, 128, 128));
    pen.setWidth(28); // only good for isometric 64x32 tiles!
    pen.setJoinStyle(Qt::MiterJoin);
    mMapBordersItem->setPen(pen);
    mMapBordersItem->setZValue(ZVALUE_GRID - 1);
//    addItem(mMapBordersItem);


}

CellScene::~CellScene()
{
    mDestroying = true;
    // mMap, mMapInfo are shared, don't destroy
    delete mMapComposite;
    delete mRenderer;
    delete mMapBuildings;
}

void CellScene::setTool(AbstractTool *tool)
{
    BaseCellSceneTool *cellTool = tool ? tool->asCellTool() : 0;

    if (mActiveTool == cellTool)
        return;

    if (mActiveTool) {
        mActiveTool->deactivate();
    }

    mActiveTool = cellTool;

    if (mActiveTool) {
        mActiveTool->activate();
    }

    // Deselect all lots if they can't be moved
    if (mActiveTool != SubMapTool::instance())
        setSelectedSubMapItems(QSet<SubMapItem*>());

    // Deselect all objects if they can't be edited
    if (mActiveTool != SelectMoveObjectTool::instance())
        setSelectedObjectItems(QSet<ObjectItem*>());
    else {
        foreach (ObjectItem *item, mSelectedObjectItems)
            item->setEditable(true);
    }

    if (mActiveTool != CellEditRoadTool::instance())
        foreach (CellRoadItem *item, mRoadItems)
            item->setEditable(false);
    if (mActiveTool != CellSelectMoveRoadTool::instance())
        worldDocument()->setSelectedRoads(QList<Road*>());

    if (mActiveTool != EditPolygonObjectTool::instancePtr()) {
        for (ObjectItem* item : mObjectItems){
            item->setEditable(false);
        }
    }

    bool bFeatureToolActive = dynamic_cast<BaseInGameMapFeatureTool*>(mActiveTool) != nullptr;
    for (InGameMapFeatureItem* item : qAsConst(mFeatureItems)) {
        item->setVisible(bFeatureToolActive);
    }
    for (AdjacentMap *adjacentMap : qAsConst(mAdjacentMaps)) {
        adjacentMap->setTool(mActiveTool);
    }
    if (mActiveTool != EditInGameMapFeatureTool::instancePtr()) {
        for (InGameMapFeatureItem* item : qAsConst(mFeatureItems)) {
            item->setEditable(false);
        }
    }

    // Restack ObjectItems and SubMapItems based on the current tool.
    // This is to ensure the mouse-over highlight works as expected.
    setGraphicsSceneZOrder();
}

void CellScene::viewTransformChanged(BaseGraphicsView *view)
{
    Q_UNUSED(view)
    foreach (ObjectItem *item, mObjectItems)
        item->synchWithObject(); // actually just need to update the label
}

void CellScene::setDocument(CellDocument *doc)
{
    mDocument = doc;

    connect(worldDocument(), SIGNAL(cellAdded(WorldCell*)),
            SLOT(cellAdded(WorldCell*)));
    connect(worldDocument(), SIGNAL(cellAboutToBeRemoved(WorldCell*)),
            SLOT(cellAboutToBeRemoved(WorldCell*)));

    connect(worldDocument(), SIGNAL(cellLotAdded(WorldCell*,int)), SLOT(cellLotAdded(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)), SLOT(cellLotAboutToBeRemoved(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellLotMoved(WorldCellLot*)), SLOT(cellLotMoved(WorldCellLot*)));
    connect(worldDocument(), SIGNAL(lotLevelChanged(WorldCellLot*)), SLOT(lotLevelChanged(WorldCellLot*)));
    connect(worldDocument(), &WorldDocument::cellLotReordered, this, &CellScene::cellLotReordered);
    connect(mDocument, SIGNAL(selectedLotsChanged()), SLOT(selectedLotsChanged()));

    connect(worldDocument(), SIGNAL(cellObjectAdded(WorldCell*,int)), SLOT(cellObjectAdded(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellObjectAboutToBeRemoved(WorldCell*,int)), SLOT(cellObjectAboutToBeRemoved(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellObjectMoved(WorldCellObject*)), SLOT(cellObjectMoved(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectResized(WorldCellObject*)), SLOT(cellObjectResized(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(objectLevelChanged(WorldCellObject*)), SLOT(objectLevelChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectNameChanged(WorldCellObject*)), SLOT(objectXXXXChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectTypeChanged(WorldCellObject*)), SLOT(objectXXXXChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectGroupChanged(WorldCellObject*)),
            SLOT(cellObjectGroupChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectReordered(WorldCellObject*)),
            SLOT(cellObjectReordered(WorldCellObject*)));
    connect(mDocument, SIGNAL(selectedObjectsChanged()), SLOT(selectedObjectsChanged()));

    connect(worldDocument(), &WorldDocument::cellObjectPointMoved, this, &CellScene::cellObjectPointMoved);
    connect(worldDocument(), &WorldDocument::cellObjectPointsChanged, this, &CellScene::cellObjectPointsChanged);
    connect(mDocument, &CellDocument::selectedObjectPointsChanged, this, &CellScene::selectedObjectPointsChanged);

    connect(worldDocument(), SIGNAL(objectGroupReordered(int)),
            SLOT(objectGroupReordered(int)));
    connect(worldDocument(), SIGNAL(objectGroupColorChanged(WorldObjectGroup*)),
            SLOT(objectGroupColorChanged(WorldObjectGroup*)));

    connect(mDocument, SIGNAL(cellMapFileChanged()), SLOT(cellMapFileChanged()));
    connect(mDocument, SIGNAL(cellContentsChanged()), SLOT(cellContentsChanged()));
    connect(mDocument, SIGNAL(layerVisibilityChanged(Tiled::Layer*)),
            SLOT(layerVisibilityChanged(Tiled::Layer*)));
    connect(mDocument, SIGNAL(layerGroupVisibilityChanged(Tiled::ZTileLayerGroup*)),
            SLOT(layerGroupVisibilityChanged(Tiled::ZTileLayerGroup*)));
    connect(mDocument, SIGNAL(lotLevelVisibilityChanged(int)),
            SLOT(lotLevelVisibilityChanged(int)));
    connect(mDocument, SIGNAL(objectLevelVisibilityChanged(int)),
            SLOT(objectLevelVisibilityChanged(int)));
    connect(mDocument, SIGNAL(objectGroupVisibilityChanged(WorldObjectGroup*,int)),
            SLOT(objectGroupVisibilityChanged(WorldObjectGroup*,int)));
    connect(mDocument, SIGNAL(currentLevelChanged(int)), SLOT(currentLevelChanged(int)));

    // These are to update ObjectLabelItem
    connect(worldDocument(), &WorldDocument::propertyAdded, this, &CellScene::propertiesChanged);
    connect(worldDocument(), &WorldDocument::propertyRemoved, this, &CellScene::propertiesChanged);
    connect(worldDocument(), &WorldDocument::propertyValueChanged, this, &CellScene::propertiesChanged);
    connect(worldDocument(), QOverload<PropertyHolder*,int>::of(&WorldDocument::templateAdded), this, &CellScene::propertiesChanged);
    connect(worldDocument(), &WorldDocument::templateRemoved, this, &CellScene::propertiesChanged);

    connect(worldDocument(), &WorldDocument::inGameMapFeatureAdded, this, &CellScene::inGameMapFeatureAdded);
    connect(worldDocument(), &WorldDocument::inGameMapFeatureAboutToBeRemoved, this, &CellScene::inGameMapFeatureAboutToBeRemoved);
    connect(worldDocument(), &WorldDocument::inGameMapPointMoved, this, &CellScene::inGameMapPointMoved);
    connect(worldDocument(), &WorldDocument::inGameMapGeometryChanged, this, &CellScene::inGameMapGeometryChanged);
    connect(mDocument, &CellDocument::selectedInGameMapFeaturesChanged, this, &CellScene::selectedInGameMapFeaturesChanged);
    connect(mDocument, &CellDocument::selectedInGameMapPointsChanged, this, &CellScene::selectedInGameMapPointsChanged);

    connect(worldDocument(), SIGNAL(roadAdded(int)),
           SLOT(roadAdded(int)));
    connect(worldDocument(), SIGNAL(roadRemoved(Road*)),
           SLOT(roadRemoved(Road*)));
    connect(worldDocument(), SIGNAL(roadCoordsChanged(int)),
           SLOT(roadCoordsChanged(int)));
    connect(worldDocument(), SIGNAL(roadWidthChanged(int)),
           SLOT(roadWidthChanged(int)));
    connect(worldDocument(), SIGNAL(roadTileNameChanged(int)),
            SLOT(roadsChanged()));
    connect(worldDocument(), SIGNAL(roadLinesChanged(int)),
            SLOT(roadsChanged()));
    connect(worldDocument(), SIGNAL(selectedRoadsChanged()),
            SLOT(selectedRoadsChanged()));

    connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
            SLOT(mapLoaded(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
            SLOT(mapFailedToLoad(MapInfo*)));

    loadMap();

    connect(Tiled::Internal::TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(Preferences::instance(), SIGNAL(highlightRoomUnderPointerChanged(bool)),
            SLOT(highlightRoomUnderPointerChanged(bool)));
}

WorldDocument *CellScene::worldDocument() const
{
    return mDocument->worldDocument();
}

World *CellScene::world() const
{
    return mDocument->worldDocument()->world();
}

WorldCell *CellScene::cell() const
{
    return document()->cell();
}

SubMapItem *CellScene::itemForLot(WorldCellLot *lot)
{
    foreach (SubMapItem *item, mSubMapItems) {
        if (item->lot() == lot)
            return item;
    }
    return 0;
}

WorldCellLot *CellScene::lotForItem(SubMapItem *item)
{
    return item->lot();
}

QList<SubMapItem *> CellScene::subMapItemsUsingMapInfo(MapInfo *mapInfo)
{
    QList<SubMapItem *> ret;
    foreach (SubMapItem *item, mSubMapItems) {
        if (item->subMap()->mapInfo() == mapInfo)
            ret += item;
    }
    return ret;
}

ObjectItem *CellScene::itemForObject(WorldCellObject *obj)
{
    foreach (ObjectItem *item, mObjectItems) {
        if (item->object() == obj)
            return item;
    }
    return nullptr;
}

InGameMapFeatureItem *CellScene::itemForInGameMapFeature(InGameMapFeature *feature)
{
    foreach (auto* item, mFeatureItems) {
        if (item->feature() == feature)
            return item;
    }
    return nullptr;
}

void CellScene::setSelectedSubMapItems(const QSet<SubMapItem *> &selected)
{
    QList<WorldCellLot*> selection;
    foreach (SubMapItem *item, selected) {
        selection << item->lot();
    }
    document()->setSelectedLots(selection);
}

void CellScene::setSelectedObjectItems(const QSet<ObjectItem *> &selected)
{
    QList<WorldCellObject*> selection;
    foreach (ObjectItem *item, selected) {
        selection << item->object();
    }
    document()->setSelectedObjects(selection);
}

void CellScene::setSelectedInGameMapFeatureItems(const QSet<InGameMapFeatureItem *>& selected)
{
    QList<InGameMapFeature*> selection;
    for (InGameMapFeatureItem *item : selected) {
        selection << item->feature();
    }
    document()->setSelectedInGameMapFeatures(selection);
}

// Determine sane Z-order for layers in and out of TileLayerGroups
void CellScene::setGraphicsSceneZOrder()
{
    int z = 0;
    foreach (MapComposite::ZOrderItem zo, mMapComposite->zOrder()) {
        if (zo.group) {
            int level = zo.group->level();
            if (mTileLayerGroupItems.contains(level))
                mTileLayerGroupItems[level]->setZValue(z);
        } else {
            if (zo.layerIndex < mLayerItems.size()) {
                if (QGraphicsItem *item = mLayerItems[zo.layerIndex])
                    item->setZValue(z);
            }
        }
        ++z;
    }

    // SubMapItems/ObjectItems should be above all TileLayerGroups
    // and arranged from bottom to top by level (and object-group).
    // When the active tool affects SubMapItems, stack them above
    // ObjectItems and vice versa.
    int numLevels = mMapComposite->maxLevel() + 1;
    int lotSpaces = mSubMapItems.size() * numLevels;
    const ObjectGroupList &groups = world()->objectGroups();
    int objSpaces = mObjectItems.size() * groups.size() * numLevels;
    int z2 = z;
    if (mActiveTool && mActiveTool->affectsLots())
        z2 += objSpaces;
    int lotIndex = 0;
    foreach (SubMapItem *item, mSubMapItems)
        item->setZValue(z2
                        + item->subMap()->levelOffset() * mSubMapItems.size()
                        + lotIndex++);

    z2 = z;
    if (mActiveTool && mActiveTool->affectsObjects())
        z2 += lotSpaces;
    int objectIndex = 0;
    foreach (ObjectItem *item, mObjectItems) {
        WorldCellObject *obj = item->object();
        int groupIndex = groups.indexOf(obj->group());
        if (item->isSelected()) {
            groupIndex = groups.size();
        }
        item->setZValue(z2
                        + groups.size() * mObjectItems.size() * obj->level()
                        + groupIndex * mObjectItems.size()
                        + objectIndex++);
    }

    mGridItem->setZValue(ZVALUE_GRID);
}

void CellScene::setSubMapVisible(WorldCellLot *lot, bool visible)
{
    if (SubMapItem *item = itemForLot(lot)) {
        item->subMap()->setVisible(visible);
//        item->setVisible(visible && mDocument->isLotLevelVisible(lot->level()));
        mMapBuildingsInvalid = true;
        doLater(AllGroups | Bounds | Synch | LotVisibility/*Paint*/);
    }
}

void CellScene::setObjectVisible(WorldCellObject *obj, bool visible)
{
    if (ObjectItem *item = itemForObject(obj)) {
        item->object()->setVisible(visible);
        item->setVisible(shouldObjectItemBeVisible(item));
    }
}

void CellScene::setInGameMapFeatureVisible(InGameMapFeature *feature, bool visible)
{
    if (InGameMapFeatureItem* item = itemForInGameMapFeature(feature)) {
        item->setVisible(visible);
    }
}

void CellScene::setLevelOpacity(int level, qreal opacity)
{
    if (mTileLayerGroupItems.contains(level))
        mTileLayerGroupItems[level]->setOpacity(opacity);
}

qreal CellScene::levelOpacity(int level)
{
    if (mTileLayerGroupItems.contains(level))
        return mTileLayerGroupItems[level]->opacity();
    return 1.0;
}

void CellScene::setLayerOpacity(int level, TileLayer *tl, qreal opacity)
{
    if (mTileLayerGroupItems.contains(level) && (mTileLayerGroupItems[level]->layerGroup()->layers().indexOf(tl) != -1)) {
        mTileLayerGroupItems[level]->layerGroup()->setLayerOpacity(tl, opacity);
        mTileLayerGroupItems[level]->update();
    }
}

qreal CellScene::layerOpacity(int level, Tiled::TileLayer *tl) const
{
    if (mTileLayerGroupItems.contains(level) && (mTileLayerGroupItems[level]->layerGroup()->layers().indexOf(tl) != -1))
        return mTileLayerGroupItems[level]->layerGroup()->layerOpacity(tl);
    return 1.0;
}

void CellScene::highlightRoomUnderPointerChanged(bool highlight)
{
    Q_UNUSED(highlight)
    setHighlightRoomPosition(mHighlightRoomPosition);
}

void CellScene::setHighlightRoomPosition(const QPoint &tilePos)
{
    QRegion buildingRgn, roomRgn;
    if (Preferences::instance()->highlightRoomUnderPointer())
        buildingRgn = getBuildingRegion(tilePos, roomRgn);
    if (buildingRgn - roomRgn != mMapComposite->suppressRegion() ||
            document()->currentLevel() != mMapComposite->suppressLevel()) {
        mMapComposite->setSuppressRegion(buildingRgn - roomRgn, document()->currentLevel());
        update();
    }
    mHighlightRoomPosition = tilePos;
}

QRegion CellScene::getBuildingRegion(const QPoint &tilePos, QRegion &roomRgn)
{
    if (!mMapComposite)
        return QRegion();
    if (mMapBuildingsInvalid) {
        mMapBuildings->calculate(mMapComposite);
        mMapBuildingsInvalid = false;
        mLightSwitchOverlays.update();
    }
    if (MapBuildingsNS::Room *room = mMapBuildings->roomAt(tilePos, document()->currentLevel())) {
        roomRgn = room->region();
        return room->building->region();
    }
    return QRegion();
}

QString CellScene::roomNameAt(const QPointF &scenePos)
{
    if (mMapBuildingsInvalid) {
        mMapBuildings->calculate(mMapComposite);
        mMapBuildingsInvalid = false;
        mLightSwitchOverlays.update();
    }
    QPoint tilePos = mRenderer->pixelToTileCoordsInt(scenePos, document()->currentLevel());
    if (MapBuildingsNS::Room *room = mMapBuildings->roomAt(tilePos, document()->currentLevel()))
        return room->name;
    return QString();
}

void CellScene::keyPressEvent(QKeyEvent *event)
{
    if (mActiveTool != 0) {
        mActiveTool->keyPressEvent(event);
        if (event->isAccepted())
            return;
    }
    QGraphicsScene::keyPressEvent(event);
}

void CellScene::loadMap()
{
    mPendingDefer = true;

    if (mMap) {
        removeItem(mDarkRectangle);
        removeItem(mGridItem);
        removeItem(mMapBordersItem);
        removeItem(mWaterFlowOverlay);

        mLightSwitchOverlays.removeOverlays();

        foreach (AdjacentMap *am, mAdjacentMaps)
            am->removeItems();
        qDeleteAll(mAdjacentMaps);
        mAdjacentMaps.clear();

        clearScene();

        setSceneRect(QRectF());

        delete mMapComposite;
        delete mRenderer;

        mLayerItems.clear();
        mTileLayerGroupItems.clear();
        mPendingGroupItems.clear();
        mObjectItems.clear();
        mSelectedObjectItems.clear();
        mSubMapItems.clear();
        mSelectedSubMapItems.clear();
        mRoadItems.clear();

        // mMap, mMapInfo are shared, don't destroy
        mMap = nullptr;
        mMapInfo = nullptr;
        mMapComposite = nullptr;
        mRenderer = nullptr;
    }

    PROGRESS progress(tr("Loading cell %1,%2").arg(cell()->x()).arg(cell()->y()));

    if (cell()->mapFilePath().isEmpty())
        mMapInfo = MapManager::instance()->getEmptyMap();
    else {
        mMapInfo = MapManager::instance()->loadMap(cell()->mapFilePath());
        if (!mMapInfo) {
            qDebug() << "failed to load cell map" << cell()->mapFilePath();
            mMapInfo = MapManager::instance()->getPlaceholderMap(cell()->mapFilePath(), 300, 300);
        }
    }
    if (!mMapInfo) {
        QMessageBox::warning(MainWindow::instance(), tr("Error Loading Map"),
                             tr("%1\nCouldn't load the map for cell %2,%3.\nTry setting the maptools folder and try again.")
                             .arg(cell()->mapFilePath()).arg(cell()->x()).arg(cell()->y()));
        return; // TODO: Add error handling
    }

    mMap = mMapInfo->map();

#if 1
    // Add any missing default tile layers so the user can hide/show them in the Layers Dock.
    // FIXME: mMap is shared, is this safe?
    for (int level = 0; level < MAX_WORLD_LEVELS; level++)
    {
        QStringList defaultLayerNames = BuildingEditor::BuildingTMX::instance()->tileLayerNamesForLevel(level);
        for (const QString& layerName : defaultLayerNames) {
            QString withoutPrefix = MapComposite::layerNameWithoutPrefix(layerName);
            QString withPrefix = QStringLiteral("%1_%2").arg(level).arg(withoutPrefix);
            bool exists = false;
            const QList<TileLayer*> tileLayers = mMap->tileLayers();
            for (TileLayer* layer : tileLayers) {
                if (layer->name() == withPrefix) {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;
            TileLayer* layer = new TileLayer(withPrefix, 0, 0, 300, 300);
            mMap->insertLayer(mMap->layerCount(), layer);
        }
    }
#endif

    switch (mMap->orientation()) {
    case Map::Isometric:
    case Map::LevelIsometric:
        mRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return; // TODO: Add error handling
    }

    mMapComposite = new MapComposite(mMapInfo, Map::LevelIsometric);

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    connect(mMapComposite, SIGNAL(layerGroupAdded(int)),
            SLOT(layerGroupAdded(int)));
    connect(mMapComposite, SIGNAL(layerGroupAdded(int)),
            mDocument, SIGNAL(layerGroupAdded(int)));
    connect(mMapComposite, SIGNAL(needsSynch()),
            SLOT(mapCompositeNeedsSynch()));

    for (int i = 0; i < cell()->lots().size(); i++)
        cellLotAdded(cell(), i);

    foreach (WorldCellObject *obj, cell()->objects()) {
        ObjectItem *item = obj->isSpawnPoint() ? new SpawnPointItem(obj, this)
                                               : new ObjectItem(obj, this);
        addItem(item);
        item->synchWithObject(); // for ObjectLabelItem
        mObjectItems += item;
        mMapComposite->ensureMaxLevels(obj->level());
    }

    // FIXME: This creates a new CellRoadItem for every road in the world,
    // even if many are not visible in this cell.
    foreach (Road *road, world()->roads()) {
        CellRoadItem *item = new CellRoadItem(this, road);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
        addItem(item);
        mRoadItems += item;
    }

    for (auto* feature : cell()->inGameMap().mFeatures) {
        InGameMapFeatureItem* item = new InGameMapFeatureItem(feature, this);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
        addItem(item);
        mFeatureItems += item;
    }

    // Explicitly set sceneRect, otherwise it will just be as large as is needed to display
    // all the items in the scene (without getting smaller, ever).
    setSceneRect(0, 0, 1, 1);

    initAdjacentMaps();

    updateBordersItem();

    mPendingFlags |= AllGroups | Bounds | Synch | ZOrder | Paint;
    mPendingDefer = false;
    handlePendingUpdates();

    addItem(mDarkRectangle);
    addItem(mGridItem);
    addItem(mMapBordersItem);
    addItem(mWaterFlowOverlay);

    updateCurrentLevelHighlight();

    mMapComposite->generateRoadLayers(QPoint(cell()->x()*300, cell()->y()*300),
                                      world()->roads());

    mMapBuildingsInvalid = true;
}

void CellScene::updateBordersItem()
{
    QPolygonF polygon;
    QRectF rect(0 - 0.5, 0 - 0.5, mMapInfo->width() + 1.0, mMapInfo->height() + 1.0);
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.topLeft()));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.topRight()));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.bottomRight()));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.bottomLeft()));
    mMapBordersItem->setPolygon(polygon);
}

void CellScene::cellAdded(WorldCell *_cell)
{
    int x = _cell->x() - cell()->x();
    int y = _cell->y() - cell()->y();
    if (QRect(-1, -1, 3, 3).contains(x, y)) {
        if (!mMapComposite->adjacentMap(x, y))
            mAdjacentMaps += new AdjacentMap(this, _cell);
    }
}

void CellScene::cellAboutToBeRemoved(WorldCell *_cell)
{
    for (int i = 0; i < mAdjacentMaps.size(); i++) {
        AdjacentMap *am = mAdjacentMaps[i];
        if (_cell == am->cell()) {
            int x = am->cell()->x() - cell()->x();
            int y = am->cell()->y() - cell()->y();
            mMapComposite->setAdjacentMap(x, y, 0);
            delete mAdjacentMaps.takeAt(i);
            doLater(AllGroups | Bounds | Synch | ZOrder);
            --i;
        }
    }
}

void CellScene::cellMapFileChanged()
{
    loadMap();
}

void CellScene::cellContentsChanged()
{
    loadMap();
}

void CellScene::cellLotAdded(WorldCell *_cell, int index)
{
    if (_cell == cell()) {
        WorldCellLot *lot = cell()->lots().at(index);
        MapInfo *subMapInfo = MapManager::instance()->loadMap(
                    lot->mapName(), QString(), true, MapManager::PriorityLow);
        if (!subMapInfo) {
            qDebug() << "failed to load lot map" << lot->mapName() << "in map" << mMapInfo->path();
            subMapInfo = MapManager::instance()->getPlaceholderMap(lot->mapName(), lot->width(), lot->height());
        }
        if (subMapInfo) {
            mSubMapsLoading += LoadingSubMap(lot, subMapInfo);
            if (!subMapInfo->isLoading()) {
                mapLoaded(subMapInfo);
            }
        }
    }
}

void CellScene::cellLotAboutToBeRemoved(WorldCell *_cell, int index)
{
    if (_cell == cell()) {
        WorldCellLot *lot = cell()->lots().at(index);
        SubMapItem *item = itemForLot(lot);
        if (item) {
            mMapComposite->removeMap(item->subMap());
            mSubMapItems.removeAll(item);
            mSelectedSubMapItems.remove(item);
            doLater(AllGroups | Bounds | Synch | ZOrder | Paint);
            removeItem(item);
            delete item;
            mMapBuildingsInvalid = true;
        }
    }
}

void CellScene::cellLotMoved(WorldCellLot *lot)
{
    if (lot->cell() != cell())
        return;
    if (SubMapItem *item = itemForLot(lot)) {
        mMapComposite->moveSubMap(item->subMap(), lot->pos());
        doLater(AllGroups | Bounds | Synch/* | Paint*/);
        item->subMapMoved();
        mMapBuildingsInvalid = true;
    }
}

void CellScene::lotLevelChanged(WorldCellLot *lot)
{
    if (lot->cell() != cell())
        return;
    if (SubMapItem *item = itemForLot(lot)) {

        // When the level changes, the position also changes to keep
        // the lot in the same visual location.
        item->subMap()->setOrigin(lot->pos());

        item->subMap()->setLevel(lot->level());
//        item->subMapMoved(); // also called in synchLayerGroups()

        // Make sure there are enough layer-groups to display the submap
        int maxLevel = lot->level() + item->subMap()->maxLevel();
        if (maxLevel > mMapComposite->maxLevel()) {
            mMapComposite->ensureMaxLevels(maxLevel);
//            foreach (CompositeLayerGroup *layerGroup, mMapComposite->layerGroups())
//                layerGroup->synch();
        }

        doLater(AllGroups | Bounds | Synch | ZOrder);

        mMapBuildingsInvalid = true;
    }
}

void CellScene::cellObjectAdded(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    WorldCellObject *obj = cell->objects().at(index);
    ObjectItem *item = obj->isSpawnPoint() ? new SpawnPointItem(obj, this)
                                           : new ObjectItem(obj, this);
    addItem(item);
    item->synchWithObject(); // update label coords
    mObjectItems.insert(index, item);

    doLater(ZOrder);
}

void CellScene::cellObjectAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    WorldCellObject *obj = cell->objects().at(index);
    if (ObjectItem *item = itemForObject(obj)) {
        mObjectItems.removeAll(item);
        mSelectedObjectItems.remove(item);
        removeItem(item);
        delete item;

        doLater(ZOrder);
    }
}

void CellScene::cellObjectMoved(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj))
        item->synchWithObject();
}

void CellScene::cellObjectResized(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj))
        item->synchWithObject();
}

void CellScene::objectLevelChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        item->synchWithObject();
        doLater(ZOrder);
    }
}

void CellScene::objectXXXXChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        if (item->isSpawnPoint() != obj->isSpawnPoint()) {
            cellObjectAboutToBeRemoved(obj->cell(), obj->index());
            cellObjectAdded(obj->cell(), obj->index());
            item = itemForObject(obj);
        }
        item->synchWithObject();
    }
}

void CellScene::propertiesChanged(PropertyHolder* ph)
{
    WorldCellObject* obj = dynamic_cast<WorldCellObject*>(ph);
    if (obj == nullptr)
        return;

    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        item->synchWithObject();
    }
}

void CellScene::inGameMapFeatureAdded(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    InGameMapFeature* feature = cell->inGameMap().mFeatures[index];
    InGameMapFeatureItem* item = new InGameMapFeatureItem(feature, this);
    item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    addItem(item);
    mFeatureItems += item;
    doLater(ZOrder);
}

void CellScene::inGameMapFeatureAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().mFeatures.at(index);
    if (auto* item = itemForInGameMapFeature(feature)) {
        mFeatureItems.removeAll(item);
        mSelectedFeatureItems.remove(item);
        removeItem(item);
        delete item;
        doLater(ZOrder);
    }

}

void CellScene::inGameMapPointMoved(WorldCell *cell, int featureIndex, int coordIndex, int pointIndex)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (auto* item = itemForInGameMapFeature(feature)) {
        item->synchWithFeature();
    }
}

void CellScene::inGameMapGeometryChanged(WorldCell *cell, int featureIndex)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (auto* item = itemForInGameMapFeature(feature)) {
        item->synchWithFeature();
        item->update();
    }
}

void CellScene::inGameMapHoleAdded(WorldCell *cell, int featureIndex, int holeIndex)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (auto* item = itemForInGameMapFeature(feature)) {
        if (holeIndex <= item->selectedCoordIndex()) {
            item->setSelectedCoordIndex(item->selectedCoordIndex() + 1);
        }
    }
}

void CellScene::inGameMapHoleRemoved(WorldCell *cell, int featureIndex, int holeIndex)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().mFeatures.at(featureIndex);
    if (auto* item = itemForInGameMapFeature(feature)) {
        if (holeIndex <= item->selectedCoordIndex()) {
            item->setSelectedCoordIndex(item->selectedCoordIndex() - 1);
        }
    }
}

void CellScene::selectedInGameMapFeaturesChanged()
{
    auto& selected = document()->selectedInGameMapFeatures();

    QSet<InGameMapFeatureItem*> items;
    for (auto* feature : selected) {
        items.insert(itemForInGameMapFeature(feature));
    }

    for (auto* item : mSelectedFeatureItems - items) {
        item->setSelected(false);
    }

    for (auto* item : items - mSelectedFeatureItems) {
        item->setSelected(true);
    }

    mSelectedFeatureItems = items;
}

void CellScene::selectedInGameMapPointsChanged()
{
    for (auto featureItem : mSelectedFeatureItems) {
        featureItem->update();
    }
}

void CellScene::cellObjectGroupChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;
    doLater(ZOrder);
    // Redraw for change in group color
    if (ObjectItem *item = itemForObject(obj))
        item->update();
}

void CellScene::cellObjectReordered(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;
    if (ObjectItem *item = itemForObject(obj)) {
        mObjectItems.removeAll(item);
        mObjectItems.insert(cell()->indexOf(obj), item);
    }
    doLater(ZOrder);
}

void CellScene::cellObjectPointMoved(WorldCell *cell, int objectIndex, int pointIndex)
{
    Q_UNUSED(pointIndex)

    if (cell != this->cell())
        return;

    WorldCellObject *object = cell->objects().at(objectIndex);
    if (auto* item = itemForObject(object)) {
        item->synchWithObject();
    }
}

void CellScene::cellObjectPointsChanged(WorldCell *cell, int objectIndex)
{
    if (cell != this->cell())
        return;

    WorldCellObject *object = cell->objects().at(objectIndex);
    if (auto* item = itemForObject(object)) {
        item->synchWithObject();
        item->update();
    }
}

void CellScene::selectedObjectsChanged()
{
    const QList<WorldCellObject*> &selection = document()->selectedObjects();

    QSet<ObjectItem*> items;
    foreach (WorldCellObject *obj, selection)
        items.insert(itemForObject(obj));

    foreach (ObjectItem *item, mSelectedObjectItems - items) {
        item->setSelected(false);
        item->setEditable(false);
    }

    bool editable = SelectMoveObjectTool::instance()->isCurrent();
    foreach (ObjectItem *item, items - mSelectedObjectItems) {
        item->setSelected(true);
        item->setEditable(editable);
    }

    mSelectedObjectItems = items;

    doLater(ZOrder); // Selected objects are on top
}

void CellScene::selectedObjectPointsChanged()
{
    for (auto objectItem : mSelectedObjectItems) {
        objectItem->update();
    }
}

void CellScene::layerVisibilityChanged(Layer *layer)
{
    if (TileLayer *tl = layer->asTileLayer()) {
        int level = tl->level(); //tl->group() ? tl->group()->level() : -1;
        if (/*(level != -1) && */mTileLayerGroupItems.contains(level)) {
            if (!mPendingGroupItems.contains(mTileLayerGroupItems[level]))
                mPendingGroupItems += mTileLayerGroupItems[level];
            doLater(Bounds | Synch | Paint); //mTileLayerGroupItems[level]->synchWithTileLayers();
        }
    }
}

void CellScene::layerGroupAdded(int level)
{
    Q_UNUSED(level);
    synchLayerGroupsLater();
}

void CellScene::layerGroupVisibilityChanged(ZTileLayerGroup *layerGroup)
{
    if (mTileLayerGroupItems.contains(layerGroup->level()))
        mTileLayerGroupItems[layerGroup->level()]->setVisible(layerGroup->isVisible());
}

void CellScene::lotLevelVisibilityChanged(int level)
{
    bool visible = mDocument->isLotLevelVisible(level);
    foreach (WorldCellLot *lot, cell()->lots()) {
        if (lot->level() == level) {
            SubMapItem *item = itemForLot(lot);
            item->subMap()->setGroupVisible(visible);
//            item->setVisible(visible && item->subMap()->isVisible());
            mMapBuildingsInvalid = true;
            doLater(AllGroups | Bounds | Synch | LotVisibility/*Paint*/);
        }
    }
}

void CellScene::objectLevelVisibilityChanged(int level)
{
    foreach (WorldCellObject *obj, cell()->objects()) {
        if (obj->level() == level) {
            ObjectItem *item = itemForObject(obj);
            item->setVisible(shouldObjectItemBeVisible(item));
        }
    }
    synchAdjacentMapObjectItemVisibility();
}

void CellScene::objectGroupVisibilityChanged(WorldObjectGroup *og, int level)
{
    foreach (WorldCellObject *obj, cell()->objects()) {
        if (obj->group() == og && obj->level() == level) {
            ObjectItem *item = itemForObject(obj);
            item->setVisible(shouldObjectItemBeVisible(item));
        }
    }
    synchAdjacentMapObjectItemVisibility();
}

void CellScene::objectGroupReordered(int index)
{
    Q_UNUSED(index)
    doLater(ZOrder);
}

void CellScene::objectGroupColorChanged(WorldObjectGroup *og)
{
    foreach (WorldCellObject *obj, cell()->objects()) {
        if (obj->group() == og)
            itemForObject(obj)->update();
    }
}

void CellScene::currentLevelChanged(int index)
{
    Q_UNUSED(index)
    updateCurrentLevelHighlight();
    mGridItem->updateBoundingRect();
}

void CellScene::selectedLotsChanged()
{
    const QList<WorldCellLot*> &selection = document()->selectedLots();

    QSet<SubMapItem*> items;
    foreach (WorldCellLot *lot, selection) {
        if (SubMapItem *item = itemForLot(lot))
            items.insert(item);
    }

    foreach (SubMapItem *item, mSelectedSubMapItems - items)
        item->setEditable(false);
    foreach (SubMapItem *item, items - mSelectedSubMapItems)
        item->setEditable(true);

    mSelectedSubMapItems = items;

    // TODO: Select a layer in the level that this object is on
}

void CellScene::cellLotReordered(WorldCellLot *lot)
{
    if (lot->cell() != cell())
        return;
    sortSubMaps();
    doLater(AllGroups | Bounds | Synch | ZOrder);
}

void CellScene::setGridVisible(bool visible)
{
    mGridItem->setVisible(visible);
}

void CellScene::gridColorChanged(const QColor &gridColor)
{
    Q_UNUSED(gridColor)
    mGridItem->update();
}

void CellScene::showObjectsChanged(bool show)
{
    Q_UNUSED(show)
    foreach (ObjectItem *item, mObjectItems)
        item->setVisible(shouldObjectItemBeVisible(item));
    synchAdjacentMapObjectItemVisibility();
}

void CellScene::showObjectNamesChanged(bool show)
{
    Q_UNUSED(show)
    foreach (ObjectItem *item, mObjectItems)
        item->synchWithObject(); // just synch the label
}

void CellScene::setHighlightCurrentLevel(bool highlight)
{
    if (mHighlightCurrentLevel == highlight)
        return;

    mHighlightCurrentLevel = highlight;
    updateCurrentLevelHighlight();
}

void CellScene::doLater(PendingFlags flags)
{
    mPendingFlags |= flags;
#if 0
    // Got a crash when undoing stuff and the progress dialog popped up
    // which called handlePendingUpdates during loadMap but before
    // the mMapComposite was loaded.
    handlePendingUpdates();
#else
    if (mPendingActive)
        return;
    QMetaObject::invokeMethod(this, "handlePendingUpdates",
                              Qt::QueuedConnection);
    mPendingActive = true;
#endif
}

void CellScene::synchLayerGroupsLater()
{
    doLater(Bounds | AllGroups | ZOrder);
}

void CellScene::handlePendingUpdates()
{
//    qDebug() << "CellScene::handlePendingUpdates";
    if (mPendingDefer) {
        QMetaObject::invokeMethod(this, "handlePendingUpdates",
                                  Qt::QueuedConnection);
        return;
    }

    // Adding a submap may create new TileLayerGroups to ensure
    // all the submap layers can be viewed.
    foreach (CompositeLayerGroup *layerGroup, mMapComposite->layerGroups()) {
        if (!mTileLayerGroupItems.contains(layerGroup->level())) {
            CompositeLayerGroupItem *item = new CompositeLayerGroupItem(this, layerGroup, mRenderer);
            addItem(item);
            mTileLayerGroupItems[layerGroup->level()] = item;

            mRenderer->setMaxLevel(mMapComposite->maxLevel());

            mPendingFlags |= AllGroups | Bounds | Synch;
        }
    }

    int oldSize = mLayerItems.count();
    mLayerItems.resize(mMap->layerCount());
    for (int layerIndex = oldSize; layerIndex < mMap->layerCount(); ++layerIndex)
        mLayerItems[layerIndex] = new DummyGraphicsItem();

    if (mPendingFlags & AllGroups)
        mPendingGroupItems = mTileLayerGroupItems.values();

    if (mPendingFlags & Synch) {
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->layerGroup()->synch();
    }
    if (mPendingFlags & Bounds) {
        // Calc bounds *after* setting scene rect?
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->synchWithTileLayers();

        QRectF sceneRect = mMapComposite->boundingRect(mRenderer);
        if (sceneRect != this->sceneRect()) {
            setSceneRect(sceneRect);
            mDarkRectangle->setRect(sceneRect);
            updateBordersItem();
            mGridItem->updateBoundingRect();

            // If new levels were added, the bounds of a LevelIsometric map will change,
            // requiring us to reposition any SubMapItems and ObjectItems.
            foreach (SubMapItem *item, mSubMapItems)
                item->subMapMoved();

            foreach (ObjectItem *item, mObjectItems)
                item->synchWithObject();
        }
    }
    if (mPendingFlags & LotVisibility) {
        foreach (SubMapItem *item, mSubMapItems) {
            WorldCellLot *lot = item->lot();
            bool visible = mDocument->isLotLevelVisible(lot->level()) && item->subMap()->isVisible();
            item->setVisible(visible);
        }
    }
    if (mPendingFlags & Paint) {
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->update();
    }

    if (mPendingFlags & ZOrder)
        setGraphicsSceneZOrder();

    // Hack -- Let LootWindow know it should update
    if (mPendingFlags & Synch)
        emit mapContentsChanged();

    mPendingFlags = None;
    mPendingGroupItems.clear();
    mPendingActive = false;
}

void CellScene::roadAdded(int index)
{
    Road *road = world()->roads().at(index);
    Q_ASSERT(itemForRoad(road) == 0);
    CellRoadItem *item = new CellRoadItem(this, road);
    item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    addItem(item);
    mRoadItems += item;

    roadsChanged();
}

void CellScene::roadRemoved(Road *road)
{
    CellRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    mRoadItems.removeAll(item);
//    mSelectedRoadItems.remove(item); // paranoia
    removeItem(item);
    delete item;

    roadsChanged();
}

void CellScene::roadCoordsChanged(int index)
{
    Road *road = world()->roads().at(index);
    CellRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();

    roadsChanged();
}

void CellScene::roadWidthChanged(int index)
{
    Road *road = world()->roads().at(index);
    CellRoadItem *item = itemForRoad(road);
    Q_ASSERT(item);
    item->synchWithRoad();

    roadsChanged();
}

void CellScene::selectedRoadsChanged()
{
    const QList<Road*> &selection = worldDocument()->selectedRoads();

    QSet<CellRoadItem*> items;
    foreach (Road *road, selection)
        items.insert(itemForRoad(road));

    foreach (CellRoadItem *item, mSelectedRoadItems - items) {
        item->setSelected(false);
        item->setEditable(false);
        item->setZValue(ZVALUE_ROADITEM_UNSELECTED);
    }

    bool editable = CellEditRoadTool::instance()->isCurrent();
    foreach (CellRoadItem *item, items - mSelectedRoadItems) {
        item->setSelected(true);
        item->setEditable(editable);
        item->setZValue(ZVALUE_ROADITEM_SELECTED);
    }

    mSelectedRoadItems = items;
}

void CellScene::roadsChanged()
{
    mMapComposite->generateRoadLayers(QPoint(cell()->x() * 300, cell()->y() * 300),
                                      world()->roads());
    if (mMapComposite->tileLayersForLevel(0))
        if (mTileLayerGroupItems.contains(0))
            mTileLayerGroupItems[0]->update();
}

// Called when our MapComposite adds a sub-map asynchronously.
void CellScene::mapCompositeNeedsSynch()
{
    mMapBuildingsInvalid = true;
    doLater(AllGroups | Bounds | Synch | ZOrder);
}

void CellScene::updateCurrentLevelHighlight()
{
    mLightSwitchOverlays.updateCurrentLevelHighlight();

    int currentLevel = mDocument->currentLevel();
    if (!mHighlightCurrentLevel) {
        mDarkRectangle->setVisible(false);

        for (int i = 0; i < mLayerItems.size(); ++i) {
            const Layer *layer = mMap->layerAt(i);
            mLayerItems.at(i)->setVisible(layer->isVisible());
        }

        foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems)
            item->setVisible(item->layerGroup()->isVisible());

        foreach (SubMapItem *item, mSubMapItems)
            item->setVisible(item->subMap()->isVisible() &&
                             mDocument->isLotLevelVisible(item->subMap()->levelOffset()));

        foreach (ObjectItem *item, mObjectItems)
            item->setVisible(shouldObjectItemBeVisible(item));

        synchAdjacentMapObjectItemVisibility();

        return;
    }

    Q_ASSERT(mTileLayerGroupItems.contains(currentLevel));
    QGraphicsItem *currentItem = mTileLayerGroupItems[currentLevel];

    // Hide items above the current item
    int index = 0;
    foreach (QGraphicsItem *item, mLayerItems) {
        Layer *layer = mMap->layerAt(index);
        bool visible = layer->isVisible() && (layer->level() <= currentLevel);
        item->setVisible(visible);
        ++index;
    }
    foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems) {
        CompositeLayerGroup *layerGroup = item->layerGroup();
        bool visible = layerGroup->isVisible() && (layerGroup->level() <= currentLevel);
        item->setVisible(visible);
    }

    // Hide object-like things not on the current level
    foreach (SubMapItem *item, mSubMapItems) {
        bool visible = item->subMap()->isVisible()
                && (item->subMap()->levelOffset() == currentLevel)
                && mDocument->isLotLevelVisible(currentLevel);
        item->setVisible(visible);
    }
    foreach (ObjectItem *item, mObjectItems) {
        bool visible = shouldObjectItemBeVisible(item);
        item->setVisible(visible);
    }
    synchAdjacentMapObjectItemVisibility();

    // Darken layers below the current item
    mDarkRectangle->setZValue(currentItem->zValue() - 0.5);
    mDarkRectangle->setVisible(true);
}

bool CellScene::shouldObjectItemBeVisible(ObjectItem *item)
{
    if (!Preferences::instance()->showObjects())
        return false;
    WorldCellObject *obj = item->object();
    return obj->isVisible() &&
            (!mHighlightCurrentLevel || (mDocument->currentLevel() == obj->level())) &&
            mDocument->isObjectGroupVisible(obj->group(), obj->level()) &&
            mDocument->isObjectLevelVisible(obj->level());
}

void CellScene::synchAdjacentMapObjectItemVisibility()
{
    foreach (AdjacentMap *am, mAdjacentMaps) {
        am->synchObjectItemVisibility();
    }
}

void CellScene::sortSubMaps()
{
    QMap<int,SubMapItem*> zzz;
    for (SubMapItem *item : mSubMapItems) {
        int index = cell()->indexOf(item->lot());
        zzz[index] = item;
    }
    mSubMapItems = zzz.values();

    QVector<MapComposite*> orderedMaps;
    for (SubMapItem *item : mSubMapItems) {
        orderedMaps += item->subMap();
    }
    mMapComposite->sortSubMaps(orderedMaps);
}

void CellScene::tilesetChanged(Tileset *tileset)
{
    // Saw this was 0 when a map was loaded, probably during event processing
    // inside loadMap().
    if (!mMapComposite)
        return;

    if (mMapComposite->isTilesetUsed(tileset))
        update();
}

bool CellScene::mapAboutToChange(MapInfo *mapInfo)
{
    // Saw this was 0 when a map was loaded, probably during event processing
    // inside loadMap().
    if (!mMapComposite)
        return false;

    if (mMapComposite->mapAboutToChange(mapInfo)) {
    }

    // Recreating the cell's MapComposite will delete all the submaps, so delete
    // all the SubMapItems.  Loading the map may put up a PROGRESS dialog which
    // causes repainting of the scene.
    if (mapInfo == mMapComposite->mapInfo()) {
        foreach (SubMapItem *item, mSubMapItems) {
            mSubMapItems.removeAll(item);
            mSelectedSubMapItems.remove(item);
            removeItem(item);
            delete item;
        }
    }

    // If the cell's map changed, other classes (like LayersModel) need to know.
    // If only a Lot map changed, other classes don't need to know.
    return (mapInfo == mMapComposite->mapInfo());
}

bool CellScene::mapChanged(MapInfo *mapInfo)
{
    // Saw this was 0 when a map was loaded, probably during event processing
    // inside loadMap().
    if (!mMapComposite)
        return false;

    if (mMapComposite->mapChanged(mapInfo)) {
        if (mapInfo != mMapComposite->mapInfo()) {
            foreach (SubMapItem *item, subMapItemsUsingMapInfo(mapInfo))
                item->subMapMoved(); // update bounds, check valid position
            doLater(AllGroups | Bounds | Synch | Paint); // only a Lot map changed
            mMapBuildingsInvalid = true;
        }
    }

    if (mapInfo == mMapComposite->mapInfo()) {
//        loadMap();
        return true; // CellDocument::cellMapFileChanged -> CellScene::cellMapFileChanged
    }
    return false;
}

void CellScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mousePressEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mousePressEvent(event);
}

void CellScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (event->isAccepted()) {
        // If an item receives Hover events, this event will get swallowed.
        // That will stop the active tool getting the mouse move event.
        if (event->buttons() & (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton))
            return;
    }

    QPoint tilePos = mRenderer->pixelToTileCoordsInt(event->scenePos(),
                                                     document()->currentLevel());
    if (tilePos != mHighlightRoomPosition)
        setHighlightRoomPosition(tilePos);

    if (mActiveTool)
        mActiveTool->mouseMoveEvent(event);
}

void CellScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);
    if (event->isAccepted())
        return;

    if (mActiveTool)
        mActiveTool->mouseReleaseEvent(event);
}

void CellScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    int level = document()->currentLevel();
    if (level < 0) {
        event->ignore();
        return;
    }

    foreach (const QUrl &url, event->mimeData()->urls()) {
        QFileInfo info(url.toLocalFile());
        if (!info.exists()) continue;
        if (!info.isFile()) continue;
        if (info.suffix() != QLatin1String("tmx") &&
                info.suffix() != QLatin1String("tbx")) continue;
        if (!MapManager::instance()->mapInfo(info.canonicalFilePath()))
            continue;

        QString path = info.canonicalFilePath();
        mDnDItem = new DnDItem(path, mRenderer, level);
        QPoint tilePos = mRenderer->pixelToTileCoords(event->scenePos(), level).toPoint();
        mDnDItem->setTilePosition(tilePos);
        addItem(mDnDItem);
        mDnDItem->setZValue(10001);

        mWasHighlightCurrentLevel = mHighlightCurrentLevel;
        Preferences::instance()->setHighlightCurrentLevel(true);

        event->accept();
        return;
    }

    event->ignore();
}

void CellScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDnDItem) {
        int level = document()->currentLevel();
        QPoint tilePos = mRenderer->pixelToTileCoords(event->scenePos(), level).toPoint();
        mDnDItem->setTilePosition(tilePos);
    }
}

void CellScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    if (mDnDItem) {
        Preferences::instance()->setHighlightCurrentLevel(mWasHighlightCurrentLevel);
        delete mDnDItem;
        mDnDItem = 0;
    }
}

void CellScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDnDItem) {
        QPoint dropPos = mDnDItem->dropPosition();
        int level = document()->currentLevel();
        WorldCellLot *lot = new WorldCellLot(cell(), mDnDItem->mapInfo()->path(),
                                             dropPos.x(), dropPos.y(), level,
                                             mDnDItem->mapInfo()->width(),
                                             mDnDItem->mapInfo()->height());
        int index = cell()->lots().size();
        worldDocument()->addCellLot(cell(), index, lot);

        Preferences::instance()->setHighlightCurrentLevel(mWasHighlightCurrentLevel);
        delete mDnDItem;
        mDnDItem = 0;

        event->accept();
        return;
    }
    event->ignore();
}

QPoint CellScene::pixelToRoadCoords(qreal x, qreal y) const
{
    QPoint tileCoords = mRenderer->pixelToTileCoordsInt(QPointF(x, y));
    return tileCoords + QPoint(cell()->x() * 300, cell()->y() * 300);
}

QPointF CellScene::roadToSceneCoords(const QPoint &pt) const
{
    QPoint tileCoords = pt - QPoint(cell()->x() * 300, cell()->y() * 300);
    return mRenderer->tileToPixelCoords(tileCoords);
}

QPolygonF CellScene::roadRectToScenePolygon(const QRect &roadRect) const
{
    QPolygonF polygon;
    QRect adjusted = roadRect.adjusted(0, 0, 1, 1);
    polygon += roadToSceneCoords(adjusted.topLeft());
    polygon += roadToSceneCoords(adjusted.topRight());
    polygon += roadToSceneCoords(adjusted.bottomRight());
    polygon += roadToSceneCoords(adjusted.bottomLeft());
    polygon += polygon[0];
    return polygon;
}

CellRoadItem *CellScene::itemForRoad(Road *road)
{
    foreach (CellRoadItem *item, mRoadItems) {
        if (item->road() == road)
            return item;
    }
    return 0;
}

QList<Road *> CellScene::roadsInRect(const QRectF &bounds)
{
    QList<Road*> result;
    foreach (QGraphicsItem *item, items(bounds)) {
        if (CellRoadItem *roadItem = dynamic_cast<CellRoadItem*>(item))
            result += roadItem->road();
    }
    return result;
}

void CellScene::initAdjacentMaps()
{
    if (!Preferences::instance()->showAdjacentMaps()) return;

    int X = cell()->x(), Y = cell()->y();
    for (int y = Y - 1; y <= Y + 1; y++) {
        for (int x = X - 1; x <= X + 1; x++) {
            WorldCell *cell2 = world()->cellAt(x, y);
            if (cell2 && cell2 != cell())
                mAdjacentMaps += new AdjacentMap(this, cell2);
        }
    }
}

void CellScene::mapLoaded(MapInfo *mapInfo)
{
    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        LoadingSubMap &sm = mSubMapsLoading[i];
        if (sm.mapInfo == mapInfo) {
            MapComposite *subMap = mMapComposite->addMap(sm.mapInfo, sm.lot->pos(),
                                                         sm.lot->level());

            SubMapItem *item = new SubMapItem(subMap, sm.lot, mRenderer);

            // Don't just call mSubMapItems.insert(), due to asynchronous loading.
            QMap<int,SubMapItem*> zzz;
            for (SubMapItem *item : mSubMapItems) {
                int index = cell()->indexOf(item->lot());
                zzz[index] = item;
            }
            zzz[cell()->indexOf(sm.lot)] = item;
            mSubMapItems = zzz.values();

            QVector<MapComposite*> orderedMaps;
            for (SubMapItem *item : mSubMapItems) {
                orderedMaps += item->subMap();
            }
            mMapComposite->sortSubMaps(orderedMaps);

            // Update with most-recent information
            sm.lot->setMapName(sm.mapInfo->path());
            sm.lot->setWidth(sm.mapInfo->width());
            sm.lot->setHeight(sm.mapInfo->height());

            mSubMapsLoading.removeAt(i);

            // Schedule update *before* addItem() schedules its update.
            doLater(AllGroups | Bounds | Synch | ZOrder);

            addItem(item);

            mMapBuildingsInvalid = true;

            --i;
        }
    }
}

void CellScene::mapFailedToLoad(MapInfo *mapInfo)
{
    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        LoadingSubMap &sm = mSubMapsLoading[i];
        if (sm.mapInfo == mapInfo) {
            mSubMapsLoading.removeAt(i);
            --i;
        }
    }
}

/////

AdjacentMap::AdjacentMap(CellScene *scene, WorldCell *cell) :
    QObject(scene), // DELETE WITH SCENE
    mScene(scene),
    mCell(cell),
    mMapComposite(nullptr),
    mMapInfo(nullptr),
    mObjectItemParent(new QGraphicsItemGroup),
    mInGameMapFeatureParent(new QGraphicsItemGroup)
{
    PROGRESS progress(tr("Loading adjacent cell %1,%2").arg(cell->x()).arg(cell->y()));

    mScene->addItem(mObjectItemParent);
    mScene->addItem(mInGameMapFeatureParent);

    connect(worldDocument(), SIGNAL(cellMapFileChanged(WorldCell*)),
            SLOT(cellMapFileChanged(WorldCell*)));
    connect(worldDocument(), SIGNAL(cellContentsChanged(WorldCell*)),
            SLOT(cellContentsChanged(WorldCell*)));

    connect(worldDocument(), SIGNAL(cellLotAdded(WorldCell*,int)),
            SLOT(cellLotAdded(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellLotAboutToBeRemoved(WorldCell*,int)),
            SLOT(cellLotAboutToBeRemoved(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellLotMoved(WorldCellLot*)),
            SLOT(cellLotMoved(WorldCellLot*)));
    connect(worldDocument(), SIGNAL(lotLevelChanged(WorldCellLot*)),
            SLOT(lotLevelChanged(WorldCellLot*)));
    connect(worldDocument(), &WorldDocument::cellLotReordered,
            this, &AdjacentMap::cellLotReordered);

    connect(worldDocument(), SIGNAL(cellObjectAdded(WorldCell*,int)), SLOT(cellObjectAdded(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellObjectAboutToBeRemoved(WorldCell*,int)), SLOT(cellObjectAboutToBeRemoved(WorldCell*,int)));
    connect(worldDocument(), SIGNAL(cellObjectMoved(WorldCellObject*)), SLOT(cellObjectMoved(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectResized(WorldCellObject*)), SLOT(cellObjectResized(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(objectLevelChanged(WorldCellObject*)), SLOT(objectLevelChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectNameChanged(WorldCellObject*)), SLOT(objectXXXXChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectTypeChanged(WorldCellObject*)), SLOT(objectXXXXChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectGroupChanged(WorldCellObject*)),
            SLOT(cellObjectGroupChanged(WorldCellObject*)));
    connect(worldDocument(), SIGNAL(cellObjectReordered(WorldCellObject*)),
            SLOT(cellObjectReordered(WorldCellObject*)));
    connect(worldDocument(), &WorldDocument::cellObjectPointMoved, this, &AdjacentMap::cellObjectPointMoved);
    connect(worldDocument(), &WorldDocument::cellObjectPointsChanged, this, &AdjacentMap::cellObjectPointsChanged);

    // These are to update ObjectLabelItem
    connect(worldDocument(), &WorldDocument::propertyAdded, this, &AdjacentMap::propertiesChanged);
    connect(worldDocument(), &WorldDocument::propertyRemoved, this, &AdjacentMap::propertiesChanged);
    connect(worldDocument(), &WorldDocument::propertyValueChanged, this, &AdjacentMap::propertiesChanged);
    connect(worldDocument(), QOverload<PropertyHolder*,int>::of(&WorldDocument::templateAdded), this, &AdjacentMap::propertiesChanged);
    connect(worldDocument(), &WorldDocument::templateRemoved, this, &AdjacentMap::propertiesChanged);

    connect(worldDocument(), &WorldDocument::inGameMapFeatureAdded, this, &AdjacentMap::inGameMapFeatureAdded);
    connect(worldDocument(), &WorldDocument::inGameMapFeatureAboutToBeRemoved, this, &AdjacentMap::inGameMapFeatureAboutToBeRemoved);
    connect(worldDocument(), &WorldDocument::inGameMapPointMoved, this, &AdjacentMap::inGameMapPointMoved);
    connect(worldDocument(), &WorldDocument::inGameMapPropertiesChanged, this, &AdjacentMap::inGameMapPropertiesChanged);
    connect(worldDocument(), &WorldDocument::inGameMapGeometryChanged, this, &AdjacentMap::inGameMapGeometryChanged);

    connect(mScene, SIGNAL(sceneRectChanged(QRectF)), SLOT(sceneRectChanged()));

    connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
            SLOT(mapLoaded(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
            SLOT(mapFailedToLoad(MapInfo*)));

    loadMap();
}

AdjacentMap::~AdjacentMap()
{
}

WorldDocument *AdjacentMap::worldDocument() const
{
    return mScene->worldDocument();
}

ObjectItem *AdjacentMap::itemForObject(WorldCellObject *obj)
{
    for (ObjectItem *item : qAsConst(mObjectItems)) {
        if (item->object() == obj)
            return item;
    }
    return 0;
}

InGameMapFeatureItem *AdjacentMap::itemForFeature(InGameMapFeature *feature)
{
    for (InGameMapFeatureItem *item : qAsConst(mInGameMapFeatureItems)) {
        if (item->feature() == feature) {
            return item;
        }
    }
    return nullptr;
}

void AdjacentMap::removeItems()
{
    delete mObjectItemParent;
    mObjectItemParent = nullptr;
    mObjectItems.clear(); // deleted with parent

    delete mInGameMapFeatureParent;
    mInGameMapFeatureParent = nullptr;
    mInGameMapFeatureItems.clear(); // deleted with parent
}

void AdjacentMap::cellMapFileChanged(WorldCell *_cell)
{
    if (_cell != cell()) return;

    loadMap();
}

void AdjacentMap::cellContentsChanged(WorldCell *_cell)
{
    if (_cell != cell()) return;

    loadMap();
}

void AdjacentMap::cellLotAdded(WorldCell *_cell, int index)
{
    if (_cell != cell()) return;

    WorldCellLot *lot = cell()->lots().at(index);
    MapInfo *subMapInfo = MapManager::instance()->loadMap(
                lot->mapName(), QString(), true, MapManager::PriorityLow);
    if (subMapInfo && !alreadyLoading(lot)) {
        mSubMapsLoading += LoadingSubMap(lot, subMapInfo);
        if (!subMapInfo->isLoading())
            mapLoaded(subMapInfo);
    }
}

void AdjacentMap::cellLotAboutToBeRemoved(WorldCell *_cell, int index)
{
    if (_cell != cell()) return;

    WorldCellLot *lot = cell()->lots().at(index);
    if (mLotToMC.contains(lot)) {
        QRectF bounds = lotSceneBounds(lot);
        mMapComposite->removeMap(mLotToMC[lot]);
        mLotToMC.remove(lot);
        scene()->mapCompositeNeedsSynch();
        scene()->update(bounds);
    }
}

void AdjacentMap::cellLotMoved(WorldCellLot *lot)
{
    if (lot->cell() != cell()) return;

    if (mLotToMC.contains(lot)) {
        QRectF bounds = lotSceneBounds(lot);
        mMapComposite->moveSubMap(mLotToMC[lot], lot->pos());
        bounds |= lotSceneBounds(lot);
        scene()->mapCompositeNeedsSynch();
        scene()->update(bounds);
    }
}

void AdjacentMap::lotLevelChanged(WorldCellLot *lot)
{
    if (lot->cell() != cell()) return;

    if (mLotToMC.contains(lot)) {

        // When the level changes, the position also changes to keep
        // the lot in the same visual location.
        mLotToMC[lot]->setOrigin(lot->pos());

        mLotToMC[lot]->setLevel(lot->level());

        // Make sure there are enough layer-groups to display the submap
        int maxLevel = lot->level() +  mLotToMC[lot]->maxLevel();
        if (maxLevel > mMapComposite->maxLevel()) {
            mMapComposite->ensureMaxLevels(maxLevel);
        }

        scene()->mapCompositeNeedsSynch();
    }
}

void AdjacentMap::cellLotReordered(WorldCellLot *lot)
{
    if (lot->cell() != cell())
        return;

}

void AdjacentMap::cellObjectAdded(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    WorldCellObject *obj = cell->objects().at(index);
    ObjectItem *item = obj->isSpawnPoint() ? new SpawnPointItem(obj, scene(), mObjectItemParent)
                                           : new ObjectItem(obj, scene(), mObjectItemParent);
    item->setAdjacent(true);
//    mScene->addItem(item);
    item->synchWithObject(); // update label coords
    mObjectItems.insert(index, item);

    setZOrder();
}

void AdjacentMap::cellObjectAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    WorldCellObject *obj = cell->objects().at(index);
    if (ObjectItem *item = itemForObject(obj)) {
        mObjectItems.removeAll(item);
        mScene->removeItem(item);
        delete item;

        setZOrder();
    }
}

void AdjacentMap::cellObjectMoved(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj))
        item->synchWithObject();
}

void AdjacentMap::cellObjectResized(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj))
        item->synchWithObject();
}

void AdjacentMap::objectLevelChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        item->synchWithObject();
        setZOrder();
    }
}

void AdjacentMap::objectXXXXChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        if (item->isSpawnPoint() != obj->isSpawnPoint()) {
            cellObjectAboutToBeRemoved(obj->cell(), obj->index());
            cellObjectAdded(obj->cell(), obj->index());
            item = itemForObject(obj);
        }
        item->synchWithObject();
    }
}

void AdjacentMap::cellObjectGroupChanged(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;
    setZOrder();
    // Redraw for change in group color
    if (ObjectItem *item = itemForObject(obj))
        item->update();
}

void AdjacentMap::cellObjectReordered(WorldCellObject *obj)
{
    if (obj->cell() != cell())
        return;
    setZOrder();
}

void AdjacentMap::cellObjectPointMoved(WorldCell *cell, int objectIndex, int pointIndex)
{
    cellObjectPointsChanged(cell, objectIndex);
}

void AdjacentMap::cellObjectPointsChanged(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    WorldCellObject *obj = cell->objects().at(index);
    if (ObjectItem *item = itemForObject(obj)) {
        item->synchWithObject();
    }
}

void AdjacentMap::propertiesChanged(PropertyHolder *ph)
{
    WorldCellObject* obj = dynamic_cast<WorldCellObject*>(ph);
    if (obj == nullptr)
        return;

    if (obj->cell() != cell())
        return;

    if (ObjectItem *item = itemForObject(obj)) {
        item->synchWithObject();
    }
}

void AdjacentMap::inGameMapFeatureAdded(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().features().at(index);
    InGameMapFeatureItem *item = new InGameMapFeatureItem(feature, scene(), mInGameMapFeatureParent);
    item->setAdjacent(true);
    item->synchWithFeature();
    mInGameMapFeatureItems.insert(index, item);

    setZOrder();
}

void AdjacentMap::inGameMapFeatureAboutToBeRemoved(WorldCell *cell, int index)
{
    if (cell != this->cell())
        return;

    InGameMapFeature *feature = cell->inGameMap().features().at(index);
    if (InGameMapFeatureItem *item = itemForFeature(feature)) {
        mInGameMapFeatureItems.removeAll(item);
        mScene->removeItem(item);
        delete item;

        setZOrder();
    }
}

void AdjacentMap::inGameMapPointMoved(WorldCell *cell, int featureIndex, int coordIndex, int pointIndex)
{
    Q_UNUSED(pointIndex)

    InGameMapFeature *feature = cell->inGameMap().features().at(featureIndex);
    if (InGameMapFeatureItem *item = itemForFeature(feature)) {
        item->synchWithFeature();
        setZOrder();
    }
}

void AdjacentMap::inGameMapPropertiesChanged(WorldCell *cell, int featureIndex)
{
    Q_UNUSED(cell)
    Q_UNUSED(featureIndex)

}

void AdjacentMap::inGameMapGeometryChanged(WorldCell *cell, int featureIndex)
{
    InGameMapFeature *feature = cell->inGameMap().features().at(featureIndex);
    if (InGameMapFeatureItem *item = itemForFeature(feature)) {
        item->synchWithFeature();
        setZOrder();
    }
}

void AdjacentMap::mapLoaded(MapInfo *mapInfo)
{
    if (mapInfo == mMapInfo) {
        int x = cell()->x() - scene()->cell()->x();
        int y = cell()->y() - scene()->cell()->y();
        scene()->mapComposite()->setAdjacentMap(x, y, mMapInfo);
        mMapComposite = scene()->mapComposite()->adjacentMap(x, y);
        scene()->mapCompositeNeedsSynch();

        foreach (WorldCellLot *lot, cell()->lots()) {
            MapInfo *subMapInfo = MapManager::instance()->loadMap(
                        lot->mapName(), QString(), true, MapManager::PriorityLow);
            if (subMapInfo && !alreadyLoading(lot)) {
                mSubMapsLoading += LoadingSubMap(lot, subMapInfo);
                if (!subMapInfo->isLoading())
                    mapLoaded(subMapInfo);
            }
        }

        qDeleteAll(mObjectItems);
        mObjectItems.clear();
        foreach (WorldCellObject *obj, cell()->objects()) {
            ObjectItem *item = obj->isSpawnPoint() ? new SpawnPointItem(obj, scene(), mObjectItemParent)
                                                   : new ObjectItem(obj, scene(), mObjectItemParent);
            item->setAdjacent(true);
//            scene()->addItem(item);
            item->synchWithObject(); // for ObjectLabelItem
            mObjectItems += item;
            mMapComposite->ensureMaxLevels(obj->level());
        }

        qDeleteAll(mInGameMapFeatureItems);
        mInGameMapFeatureItems.clear();
        for (InGameMapFeature* feature : cell()->inGameMap().features()) {
            InGameMapFeatureItem *item = new InGameMapFeatureItem(feature, scene(), mInGameMapFeatureParent);
            item->setAdjacent(true);
            item->synchWithFeature();
            mInGameMapFeatureItems += item;
        }

        setZOrder();

        sceneRectChanged();

        synchObjectItemVisibility();
    }

    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        LoadingSubMap &sm = mSubMapsLoading[i];
        if (sm.mapInfo == mapInfo) {

            // Update with most-recent information
            sm.lot->setMapName(sm.mapInfo->path());
            sm.lot->setWidth(sm.mapInfo->width());
            sm.lot->setHeight(sm.mapInfo->height());

            if (mMapComposite) {
                MapComposite *subMap = mMapComposite->addMap(sm.mapInfo, sm.lot->pos(),
                                                             sm.lot->level());
                mLotToMC[sm.lot] = subMap;

                QVector<MapComposite*> orderedMaps;
                for (WorldCellLot *lot : cell()->lots()) {
                    if (mLotToMC.contains(lot)) {
                        orderedMaps += mLotToMC[lot];
                    }
                }
                mMapComposite->sortSubMaps(orderedMaps);

                scene()->mapCompositeNeedsSynch();
                scene()->update(lotSceneBounds(sm.lot));
            }

            mSubMapsLoading.removeAt(i);

            --i;
        }
    }
}

void AdjacentMap::mapFailedToLoad(MapInfo *mapInfo)
{
    if (mapInfo == mMapInfo)
        mMapInfo = 0;

    for (int i = 0; i < mSubMapsLoading.size(); i++) {
        LoadingSubMap &sm = mSubMapsLoading[i];
        if (sm.mapInfo == mapInfo) {
            mSubMapsLoading.removeAt(i);
            --i;
        }
    }
}

bool AdjacentMap::shouldObjectItemBeVisible(ObjectItem *item)
{
    if (!Preferences::instance()->showObjects())
        return false;
    WorldCellObject *obj = item->object();
    CellDocument *mDocument = scene()->document();
    bool mHighlightCurrentLevel = Preferences::instance()->highlightCurrentLevel();
    return obj->isVisible() &&
            (!mHighlightCurrentLevel || (mDocument->currentLevel() == obj->level())) &&
            mDocument->isObjectGroupVisible(obj->group(), obj->level()) &&
            mDocument->isObjectLevelVisible(obj->level());
}

void AdjacentMap::synchObjectItemVisibility()
{
    foreach (ObjectItem *item, mObjectItems) {
        item->setVisible(shouldObjectItemBeVisible(item));
    }
}

void AdjacentMap::setTool(AbstractTool *tool)
{
    bool bFeatureToolActive = dynamic_cast<BaseInGameMapFeatureTool*>(tool) != nullptr;
    for (InGameMapFeatureItem *item : mInGameMapFeatureItems) {
        item->setVisible(bFeatureToolActive);
    }
}

void AdjacentMap::sceneRectChanged()
{
    int x = cell()->x() - scene()->cell()->x();
    int y = cell()->y() - scene()->cell()->y();
    QRectF r = scene()->renderer()->boundingRect(QRect(0, 0, 1, 1));
    QPointF offset((x - y) * (300 * r.width() / 2), (x + y) * (300 * r.height() / 2));
    mObjectItemParent->setPos(offset);

    foreach (ObjectItem *item, mObjectItems)
        item->synchWithObject();

    mInGameMapFeatureParent->setPos(offset);
    for (InGameMapFeatureItem *item : mInGameMapFeatureItems) {
        item->synchWithFeature();
}
}

void AdjacentMap::loadMap()
{
    if (cell()->mapFilePath().isEmpty()) {
        mMapInfo = MapManager::instance()->getEmptyMap();
    } else {
        mMapInfo = MapManager::instance()->loadMap(cell()->mapFilePath(), QString(), true,
                                                   MapManager::PriorityMedium);
    }
    if (mMapInfo && !mMapInfo->isLoading()) {
        mapLoaded(mMapInfo);
    }

    // FIXME: if !mMapInfo use a empty map
}

bool AdjacentMap::alreadyLoading(WorldCellLot *lot)
{
    foreach (LoadingSubMap sm, mSubMapsLoading) {
        if (sm.lot == lot)
            return true;
    }
    return false;
}

QRectF AdjacentMap::lotSceneBounds(WorldCellLot *lot)
{
    Q_ASSERT(mLotToMC.contains(lot));
    if (!mLotToMC.contains(lot)) return QRectF();
    return mLotToMC[lot]->boundingRect(scene()->renderer());
}

void AdjacentMap::setZOrder()
{
    int z = scene()->mapComposite()->maxLevel() + 1;
    mObjectItemParent->setZValue(z);
    mInGameMapFeatureParent->setZValue(z + 1);

    const ObjectGroupList &groups = cell()->world()->objectGroups();
    int objectIndex = 0;
    foreach (ObjectItem *item, mObjectItems) {
        WorldCellObject *obj = item->object();
        int groupIndex = groups.indexOf(obj->group());
        item->setZValue(0
                        + groups.size() * mObjectItems.size() * obj->level()
                        + groupIndex * mObjectItems.size()
                        + objectIndex++);
    }
}
