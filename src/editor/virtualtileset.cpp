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

#include "virtualtileset.h"

#include "preferences.h"
#include "texturemanager.h"
#include "tilesetmanager.h"

#include "BuildingEditor/buildingtiles.h"
#include "simplefile.h"

#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QDir>
#include <QGLPixelBuffer>
#include <QImageReader>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>

using namespace Tiled;
using namespace Internal;

void TileShape::fromSameAs()
{
    if (!mSameAs)
        return;

    mFaces.clear();
    foreach (TileShapeFace f2, mSameAs->mFaces) {
        TileShapeFace f;
        f.mGeom = f2.mGeom;
        f.mUV = f2.mUV;
        foreach (TileShapeXform xform, mXform) {
            QMatrix4x4 m;
            m.rotate(xform.mRotate.x(), QVector3D(1, 0, 0));
            m.rotate(xform.mRotate.y(), QVector3D(0, 1, 0));
            m.rotate(xform.mRotate.z(), QVector3D(0, 0, 1));
            for (int i = 0; i < f.mGeom.size(); i++)
                f.mGeom[i] = m.mapVector(f.mGeom[i]) + xform.mTranslate;
        }
        mFaces += f;
    }
}

/////

void TileShapeGroup::resize(int columnCount, int rowCount)
{
    QVector<TileShape*> shapes(columnCount * rowCount);
    QRect oldR(0, 0, mColumnCount, mRowCount);
    QRect newR(0, 0, columnCount, rowCount);
    QRect same = oldR & newR;

    for (int y = 0; y <= same.bottom(); y++)
        for (int x = 0; x <= same.right(); x++)
            shapes[x + y * columnCount] = shapeAt(x, y);

    mShapes = shapes;
    mColumnCount = columnCount;
    mRowCount = rowCount;
}

/////

VirtualTile::VirtualTile(VirtualTileset *vts, int x, int y) :
    mTileset(vts),
    mX(x),
    mY(y),
    mSrcX(-1),
    mSrcY(-1),
    mShape(0)
{

}

VirtualTile::VirtualTile(VirtualTileset *vts, int x, int y, const QString &imageSource,
                         int srcX, int srcY, TileShape *shape) :
    mTileset(vts),
    mImageSource(imageSource),
    mX(x),
    mY(y),
    mSrcX(srcX),
    mSrcY(srcY),
    mShape(shape)
{

}

VirtualTile::VirtualTile(const VirtualTile *other) :
    mImageSource(other->mImageSource),
    mSrcX(other->mSrcX),
    mSrcY(other->mSrcY),
    mShape(other->mShape)
{

}

void VirtualTile::copy(const VirtualTile *other)
{
    mImageSource = other->mImageSource;
    mSrcX = other->mSrcX;
    mSrcY = other->mSrcY;
    mShape = other->mShape;
}

int VirtualTile::index() const
{
    return mX + mTileset->columnCount() * mY;
}

bool VirtualTile::usesShape(TileShape *shape)
{
    return mShape == shape || (mShape && (mShape->mSameAs == shape));
}

QImage VirtualTile::image()
{
    if (mImage.isNull())
        mImage = VirtualTilesetMgr::instance().renderIsoTile(this);
    return mImage;
}

void VirtualTile::clear()
{
    mImageSource.clear();
    mSrcX = mSrcY = -1;
    mShape = 0;
    mImage = QImage();
}

/////

VirtualTileset::VirtualTileset(const QString &name, int columnCount, int rowCount) :
    mName(name),
    mColumnCount(columnCount),
    mRowCount(rowCount),
    mTiles(mColumnCount * mRowCount)
{
    for (int y = 0; y < rowCount; y++)
        for (int x = 0; x < columnCount; x++)
            mTiles[y * columnCount + x] = new VirtualTile(this, x, y);
}

VirtualTileset::~VirtualTileset()
{
    qDeleteAll(mTiles);
}

VirtualTile *VirtualTileset::tileAt(int n)
{
    if (n >= 0 && n < mTiles.size())
        return mTiles.at(n);
    return 0;
}

VirtualTile *VirtualTileset::tileAt(int x, int y)
{
    if (!QRect(0, 0, mColumnCount, mRowCount).contains(x, y))
        return 0;
    return tileAt(y * mColumnCount + x);
}

void VirtualTileset::resize(int columnCount, int rowCount, QVector<VirtualTile *> &tiles)
{
    Q_ASSERT(tiles.size() == columnCount * rowCount);
    QVector<VirtualTile*> old = mTiles;
    mTiles = tiles;
    mColumnCount = columnCount;
    mRowCount = rowCount;
    tiles = old;
}

QImage VirtualTileset::image()
{
    if (mImage.isNull()) {
        mImage = QImage(columnCount() * 64, rowCount() * 128, QImage::Format_ARGB32);
        mImage.fill(Qt::transparent);
        QPainter painter(&mImage);
        for (int i = 0; i < tileCount(); i++) {
            VirtualTile *vtile = tileAt(i);
            if (vtile->shape()) {
                painter.drawImage(vtile->x() * 64, vtile->y() * 128, vtile->image());
            } else {
                // Use the original iso tile when there's no virtual one.
                QImage orig = VirtualTilesetMgr::instance().originalIsoImage(this);
                if (!orig.isNull())
                    painter.drawImage(vtile->x() * 64, vtile->y() * 128, orig, vtile->x() * 64, vtile->y() * 128, 64, 128);
            }
        }
        painter.end();
    }
    return mImage;
}

/////

SINGLETON_IMPL(VirtualTilesetMgr)

//#include "textureunpacker.h"
VirtualTilesetMgr::VirtualTilesetMgr() :
    mUngroupedGroup(new TileShapeGroup(tr("<Ungrouped>"), 0, 0))
{
    initPixelBuffer();

    mCheckerboard = QImage(32, 96, QImage::Format_ARGB32);
    QPainter p(&mCheckerboard);
    for (int y = 0; y < mCheckerboard.height() / 4; y += 1)
        for (int x = 0; x < mCheckerboard.width() / 4; x += 1)
            p.fillRect(x * 4, y * 4, 4, 4,
                       ((x & 1) == (y & 1)) ? Qt::darkGray : Qt::lightGray);

#if 0
    TextureUnpacker unpacker;
    unpacker.unpack(QLatin1String("ntiles_0"));
    unpacker.unpack(QLatin1String("ntiles_1"));
    unpacker.unpack(QLatin1String("ntiles_2"));
    unpacker.unpack(QLatin1String("ntiles_3"));
    unpacker.createImages();
    unpacker.writeImages(Preferences::instance()->tilesDirectory() + QLatin1String("/Textures"));

    QList<TextureInfo*> texs;
    foreach (QString tilesetName, unpacker.mTilesetSize.keys())
        texs += new TextureInfo(tilesetName, QDir(Preferences::instance()->texturesDirectory()).filePath(tilesetName + QLatin1String(".png")),
                                8, 16, unpacker.mTilesetSize[tilesetName].width(),
                                unpacker.mTilesetSize[tilesetName].height());
    TexturesFile texFile;
    texFile.write(TextureMgr::instance().txtPath(), texs);
#endif

    connect(TextureMgr::instancePtr(), SIGNAL(textureAdded(TextureInfo*)),
            SLOT(textureAdded(TextureInfo*)));
    connect(TextureMgr::instancePtr(), SIGNAL(textureRemoved(TextureInfo*)),
            SLOT(textureRemoved(TextureInfo*)));
    connect(TextureMgr::instancePtr(), SIGNAL(textureChanged(TextureInfo*)),
            SLOT(textureChanged(TextureInfo*)));
}

VirtualTilesetMgr::~VirtualTilesetMgr()
{
    delete mPixelBuffer;
    // FIXME: delet all groups/shapes
}

QString VirtualTilesetMgr::txtPath() const
{
    return Preferences::instance()->configPath(txtName());
}

bool VirtualTilesetMgr::readTxt()
{
    QString fileName = Preferences::instance()->tilesDirectory() + QLatin1String("/virtualtilesets.vts");
#if 0
    if (!QFileInfo(fileName).exists()) {

        QFileInfo info(txtPath());
        if (!info.exists()) {
            mError = tr("The %1 file doesn't exist.").arg(txtName());
            return false;
        }

        OldVirtualTilesetsTxtFile file;
        if (!file.read(txtPath())) {
            mError = file.errorString();
            return false;
        }

        mRevision = file.revision();
        mSourceRevision = file.sourceRevision();

        VirtualTilesetsFile binFile;
        foreach (VirtualTileset *vts, file.takeTilesets())
            binFile.addTileset(vts);

        if (!binFile.write(fileName)) {
            mError = binFile.errorString();
            return false;
        }
    }
#endif
    TileShapesFile shapesFile;
    if (!shapesFile.read(Preferences::instance()->configPath(QLatin1String("TileShapes.txt")))) {
        mError = shapesFile.errorString();
        return false;
    }
    foreach (TileShape *shape, shapesFile.takeShapes())
        mShapeByName[shape->name()] = shape;
    mShapeGroups = shapesFile.takeGroups();
    setUngroupedGroup();

    VirtualTilesetsFile binFile;
    if (!binFile.read(fileName)) {
        mError = binFile.errorString();
        return false;
    }

    foreach (VirtualTileset *vts, binFile.takeTilesets())
        addTileset(vts);

    return true;
}

bool VirtualTilesetMgr::writeTxt()
{
#if 0
    OldVirtualTilesetsTxtFile file;
    file.setRevision(++mRevision, mSourceRevision);
    if (!file.write(txtPath(), tilesets())) {
        mError = file.errorString();
        return false;
    }
#endif

    TileShapesFile shapesFile;
    (void) shapesFile.write(Preferences::instance()->configPath(QLatin1String("TileShapes.txt")),
                            tileShapes(), shapeGroups());

    QString fileName = Preferences::instance()->tilesDirectory() + QLatin1String("/virtualtilesets.vts");
    VirtualTilesetsFile binFile;
    return binFile.write(fileName, tilesets());

    return true;
}

void VirtualTilesetMgr::addTileset(VirtualTileset *vts)
{
    Q_ASSERT(mTilesetByName.contains(vts->name()) == false);
    mTilesetByName[vts->name()] = vts;
    mRemovedTilesets.removeAll(vts);
    emit tilesetAdded(vts);
}

void VirtualTilesetMgr::removeTileset(VirtualTileset *vts)
{
    Q_ASSERT(mTilesetByName.contains(vts->name()));
    Q_ASSERT(mRemovedTilesets.contains(vts) == false);
    emit tilesetAboutToBeRemoved(vts);
    mTilesetByName.remove(vts->name());
    emit tilesetRemoved(vts);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += vts;
}

void VirtualTilesetMgr::renameTileset(VirtualTileset *vts, const QString &name)
{
    removeTileset(vts);
    vts->setName(name);
    addTileset(vts);
}

void VirtualTilesetMgr::resizeTileset(VirtualTileset *vts, QSize &size,
                                      QVector<VirtualTile*> &tiles)
{
    removeTileset(vts);
    QSize oldSize(vts->columnCount(), vts->rowCount());
    vts->resize(size.width(), size.height(), tiles);
    size = oldSize;
    vts->tileChanged();
    addTileset(vts);
}

QString VirtualTilesetMgr::imageSource(VirtualTileset *vts)
{
    QString tilesDir = Preferences::instance()->tilesDirectory();
    if (QFileInfo(tilesDir).exists())
        tilesDir = QFileInfo(tilesDir).canonicalFilePath();
    return QDir(tilesDir).filePath(vts->name() + QLatin1String(".png"));
}

bool VirtualTilesetMgr::resolveImageSource(QString &imageSource)
{
    if (imageSource.isEmpty())
        return false;
    QFileInfo info(imageSource);
    if (info.isRelative())
        return false;
    if (!mTilesetByName.contains(info.completeBaseName()))
        return false;
    QString tilesDir = Preferences::instance()->tilesDirectory();
    if (tilesDir.isEmpty() || QFileInfo(tilesDir).isRelative())
        return false;
    // FIXME: use QFileInfo::operator==() if both exist
    if (QDir::cleanPath(tilesDir) == QDir::cleanPath(info.absolutePath())) {
        if (QFileInfo(tilesDir).exists())
            tilesDir = QFileInfo(tilesDir).canonicalFilePath();
        imageSource = QDir(tilesDir).filePath(info.fileName());
        return true;
    }
    return false;
}

VirtualTileset *VirtualTilesetMgr::tilesetFromPath(const QString &path)
{
    QString canonical = path;
    if (resolveImageSource(canonical))
        return tileset(QFileInfo(canonical).completeBaseName());
    return 0;
}

QImage VirtualTilesetMgr::originalIsoImage(VirtualTileset *vts)
{
    if (mOriginalIsoImages.contains(vts->name()))
        return mOriginalIsoImages[vts->name()];
    QString fileName = imageSource(vts);
    return mOriginalIsoImages[vts->name()] = QImage(fileName); // may be null
}

void VirtualTilesetMgr::changeVTile(VirtualTile *vtile, VirtualTile &other)
{
    VirtualTile old(vtile);
    vtile->copy(&other);
    other.copy(&old);

    vtile->setImage(QImage());
    vtile->tileset()->tileChanged();

    emit tilesetChanged(vtile->tileset());
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
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4)
    {
        counts += 4;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        colors += QVector3D(1, 1, 1);
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
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3)
    {
        counts += 3;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2;
        textureids += textureid;
        colors += QVector3D(1, 1, 1);
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
#if QT_VERSION >= 0x050000
#else
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
#endif
    }

    QVector<char> counts;
    QVector<GLushort> indices;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLuint> textureids;
    QVector<QVector3D> colors;

};

} // namespace anon

void VirtualTilesetMgr::initPixelBuffer()
{
    int width = 64, height = 128; // Size of one iso tile

    QGLFormat pbufferFormat;
    pbufferFormat.setSampleBuffers(false);
    mPixelBuffer = new QGLPixelBuffer(QSize(width, height), pbufferFormat);

    mPixelBuffer->makeCurrent();

    glViewport(0, 0, width, height);
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
    qreal ratio = width / qreal(height);
    GLfloat m[16] = { 1.0F,  0.5F * ratio, 0.0F,   0.0F,
                      0.0F,  1.0F * ratio, -0.05F, 0.0F,
                      -1.0F, 0.5F * ratio, 0.0F,   0.0F,
                      0.0F,  0.0F, 0.0F,   1.0F };
    glLoadMatrixf(m);
    glRotatef(-90,1,0,0);
    glRotatef(90,0,0,1); // x-right, y-to-camera, z-up same as TileShapeEditor
    glTranslatef(0,0,-1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if QT_VERSION >= 0x050000
#else
    glShadeModel(GL_FLAT);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(1,1,1,0);
#if QT_VERSION >= 0x050000
#else
    glClearDepth(1.0f);
#endif
}

uint VirtualTilesetMgr::loadGLTexture(const QString &imageSource, int srcX, int srcY)
{
    QImage b;
    if (imageSource.isEmpty()) {
        b = mCheckerboard;
    } else if (TextureInfo *tex = TextureMgr::instance().texture(imageSource)) {
        Tileset *ts = TextureMgr::instance().tileset(tex);
        if (!ts->isLoaded())
             return 0;
        if (Tile *tile = ts->tileAt(srcY * ts->columnCount() + srcX))
            b = tile->image();
    }
    if (b.isNull())
        return 0;

    QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
    QPainter painter(&fixedImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(fixedImage.rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, b);
    painter.end();

    b = QGLWidget::convertToGLFormat( fixedImage );
    GLuint textureID;
    glGenTextures( 1, &textureID );
    glBindTexture( GL_TEXTURE_2D, textureID );
    glTexImage2D( GL_TEXTURE_2D, 0, 4, b.width(), b.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, b.bits() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    return textureID;
}

void VirtualTilesetMgr::setUngroupedGroup()
{
    QList<TileShape*> ungrouped;
    foreach (TileShape *shape, tileShapes()) {
        bool used = false;
        foreach (TileShapeGroup *group, shapeGroups()) {
            if (group->hasShape(shape)) {
                used = true;
                break;
            }
        }
        if (!used)
            ungrouped += shape;
    }
    int cc = 8, rc = (ungrouped.size() + 7) / 8;
    mUngroupedGroup->resize(cc, rc);
    for (int i = 0; i < ungrouped.size(); i++)
        mUngroupedGroup->setShape(i % cc, i / cc, ungrouped[i]);
    for (int i = ungrouped.size(); i < mUngroupedGroup->count(); i++)
        mUngroupedGroup->setShape(i % cc, i / cc, 0);
}

void VirtualTilesetMgr::textureAdded(TextureInfo *tex)
{
    textureChanged(tex);
}

void VirtualTilesetMgr::textureRemoved(TextureInfo *tex)
{
    textureChanged(tex);
}

// Called when a texture tileset image is loaded or when texture-tileset tile size changes
void VirtualTilesetMgr::textureChanged(TextureInfo *tex)
{
    foreach (VirtualTileset *vts, tilesets()) {
        bool changed = false;
        foreach (VirtualTile *vtile, vts->tiles()) {
            if (vtile->imageSource() == tex->name()) {
                vtile->setImage(QImage());
                // not calling vtile->setImageSource
                changed = true;
            }
        }
        if (changed) {
            vts->tileChanged();
            emit tilesetChanged(vts);
        }
    }
}

#define toGL(v) v

QImage VirtualTilesetMgr::renderIsoTile(VirtualTile *vtile)
{
    if (vtile->shape() == 0 || vtile->shape()->mFaces.isEmpty())
        return QImage();

    const QGLContext *context = QGLContext::currentContext();
    mPixelBuffer->makeCurrent();

    GLuint textureID = loadGLTexture(vtile->imageSource(), vtile->srcX(), vtile->srcY());
    if (!textureID) {
        if (context)
            const_cast<QGLContext*>(context)->makeCurrent();
        return QImage();
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawElements de;
    foreach (TileShapeFace e, vtile->shape()->mFaces) {
        if (e.mGeom.size() == 4)
            de.add(textureID,
                   QVector2D(e.mUV[0]), QVector2D(e.mUV[1]), QVector2D(e.mUV[2]), QVector2D(e.mUV[3]),
                    toGL(e.mGeom[0]), toGL(e.mGeom[1]), toGL(e.mGeom[2]), toGL(e.mGeom[3]));
        if (e.mGeom.size() == 3)
            de.add(textureID,
                   QVector2D(e.mUV[0]), QVector2D(e.mUV[1]), QVector2D(e.mUV[2]),
                    toGL(e.mGeom[0]), toGL(e.mGeom[1]), toGL(e.mGeom[2]));

        QVector3D cross = QVector3D::normal(e.mGeom[0], e.mGeom[1], e.mGeom[2]);
        if (cross.x() > 0 && cross.y() == 0 /*cross == QVector3D(1, 0, 0)*/)
            de.color(0.8f,0.8f,0.8f);
    }
    de.flush();
#if ISO_RENDERxxx
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
    glFlush();
#endif
    /////

    glDeleteTextures(1, &textureID);
    mPixelBuffer->doneCurrent();

    QImage img = mPixelBuffer->toImage();

    if (context)
        const_cast<QGLContext*>(context)->makeCurrent();

    return img;
}

void VirtualTilesetMgr::addShape(TileShape *shape)
{
    Q_ASSERT(!mShapeByName.contains(shape->name()));
    Q_ASSERT(!mShapeByName.values().contains(shape));
    mShapeByName[shape->name()] = shape;
    emit shapeAdded(shape);

    setUngroupedGroup();
    emit shapeGroupChanged(mUngroupedGroup);
}

void VirtualTilesetMgr::removeShape(TileShape *shape)
{
    Q_ASSERT(mShapeByName.contains(shape->name()));
    Q_ASSERT(mShapeByName.values().contains(shape));
    mShapeByName.remove(shape->name());
#if 1 // all this should have been done by undo/redo
    foreach (TileShape *shape2, tileShapes())
        Q_ASSERT(shape2->mSameAs != shape);
    foreach (TileShapeGroup *group, mShapeGroups)
        Q_ASSERT(!group->hasShape(shape));
    foreach (VirtualTileset *vts, tilesets()) {
        for (int i = 0; i < vts->tileCount(); i++)
            Q_ASSERT(vts->tileAt(i)->shape() != shape);
    }
#else
    QList<TileShape*> shapes;
    shapes += shape;
    foreach (TileShape *shape2, tileShapes()) {
        if (shape2->mSameAs == shape) {
            shape2->mSameAs = 0;
            shape2->mFaces.clear();
            shape2->mXform.clear();
            shapes += shape2;
        }
    }

    foreach (TileShapeGroup *group, mShapeGroups) {
        bool changed = false;
        for (int i = 0; i < group->count(); i++)
            if (group->shapeAt(i) == shape) {
                group->setShape(i, 0);
                changed = true;
            }
        if (changed)
            emit shapeGroupChanged(group);
    }

    foreach (VirtualTileset *vts, tilesets()) {
        bool changed = false;
        for (int i = 0; i < vts->tileCount(); i++) {
            VirtualTile *vtile = vts->tileAt(i);
            if (vtile->shape() == shape) {
                vtile->setShape(0);
                changed = true;
            }
            if (vtile->shape() && shapes.contains(vtile->shape())) {
                vtile->setImage(QImage());
                changed = true;
            }
        }
        if (changed) {
            vts->tileChanged();
            emit tilesetChanged(vts);
        }
    }
#endif

    if (mUngroupedGroup->hasShape(shape)) {
        setUngroupedGroup();
        emit shapeGroupChanged(mUngroupedGroup);
    }

    emit shapeRemoved(shape);
}

void VirtualTilesetMgr::changeShape(TileShape *shape, TileShape &other)
{
    TileShape old(shape);
    shape->copy(&other);
    other.copy(&old);

    shape->fromSameAs();

    foreach (TileShape *shape2, tileShapes()) {
        if (shape2->mSameAs == shape)
            shape2->fromSameAs();
    }

    if (shape->name() != old.name()) {
        Q_ASSERT(mShapeByName.contains(old.name()));
        Q_ASSERT(!mShapeByName.contains(shape->name()));
        mShapeByName.remove(old.name());
        mShapeByName[shape->name()] = shape;
    }

    emit shapeChanged(shape);

    foreach (VirtualTileset *vts, tilesets()) {
        bool changed = false;
        foreach (VirtualTile *vtile, vts->tiles()) {
            if ((vtile->shape() == shape) ||
                    (vtile->shape() && vtile->shape()->mSameAs == shape)) {
                vtile->setImage(QImage());
                changed = true;
            }
        }
        if (changed) {
            vts->tileChanged();
            emit tilesetChanged(vts);
        }
    }
}

TileShape *VirtualTilesetMgr::tileShape(const QString &name)
{
    if (mShapeByName.contains(name))
        return mShapeByName[name];
    return 0;
}

void VirtualTilesetMgr::insertShapeGroup(int index, TileShapeGroup *g)
{
    Q_ASSERT(!mShapeGroups.contains(g));
    mShapeGroups.insert(index, g);
    emit shapeGroupAdded(index, g);

    setUngroupedGroup();
    emit shapeGroupChanged(mUngroupedGroup);
}

TileShapeGroup *VirtualTilesetMgr::removeShapeGroup(int index)
{
    TileShapeGroup *g = mShapeGroups.takeAt(index);
    emit shapeGroupRemoved(index, g);

    setUngroupedGroup();
    emit shapeGroupChanged(mUngroupedGroup);

    return g;
}

void VirtualTilesetMgr::editShapeGroup(TileShapeGroup *g, TileShapeGroup &other)
{
    TileShapeGroup old(g);
    g->copy(&other);
    other.copy(&old);
    emit shapeGroupChanged(g);
}

void VirtualTilesetMgr::assignShape(TileShapeGroup *g, int col, int row, TileShape *shape)
{
    g->setShape(col, row, shape);
    setUngroupedGroup();
    emit shapeAssigned(g, col, row);
}

/////

VirtualTilesetsFile::VirtualTilesetsFile()
{
}

VirtualTilesetsFile::~VirtualTilesetsFile()
{
    qDeleteAll(mTilesets);
}

static QString ReadString(QDataStream &in)
{
    QString str;
    quint8 c = ' ';
    while (c != '\n') {
        in >> c;
        if (c != '\n')
            str += QLatin1Char(c);
    }
    return str;
}

#define VERSION1 1
#define VERSION2 2 // imageSrc string -> index
#define VERSION3 3 // added shape data
#define VERSION_LATEST VERSION3

bool VirtualTilesetsFile::read(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Error opening file for reading.\n%1").arg(fileName);
        return false;
    }

    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint8 tdef[4] = {0};
    in >> tdef[0];
    in >> tdef[1];
    in >> tdef[2];
    in >> tdef[3];
    qint32 version = VERSION1;
    if (memcmp(tdef, "vtsf", 4) == 0) {
        in >> version;
        if (version < 1 || version > VERSION_LATEST) {
            mError = tr("Unknown version number %1 in .vts file.\n%2")
                    .arg(version).arg(fileName);
            return false;
        }
    } else {
        mError = tr("This isn't a .vts file.\n%1").arg(fileName);
        return false;
    }

    qint32 numShapes;
    in >> numShapes;
    QStringList shapes;
    for (int i = 0; i < numShapes; i++) {
        shapes += ReadString(in);
        if (version > VERSION2) {
            qint32 numFaces;
            in >> numFaces;
            for (int j = 0; j < numFaces; j++) {
                qint32 numPts;
                in >> numPts;
                for (int k = 0; k < numPts; k++) {
                    double x, y, z;
                    in >> x >> y >> z;
                }
                for (int k = 0; k < numPts; k++) {
                    double u, v;
                    in >> u >> v;
                }
            }
        }
    }

    QStringList srcNames;
    if (version > VERSION1) {
        qint32 numSrcNames;
        in >> numSrcNames;
        for (int i = 0; i < numSrcNames; i++)
            srcNames += ReadString(in);
    }

    qint32 numTilesets;
    in >> numTilesets;
    for (int i = 0; i < numTilesets; i++) {
        QString name = ReadString(in);
        qint32 columns, rows;
        in >> columns;
        in >> rows;

        VirtualTileset *vts = new VirtualTileset(name, columns, rows);

        qint32 tileCount;
        in >> tileCount;
        for (int j = 0; j < tileCount; j++) {
            qint32 tileIndex;
            in >> tileIndex;
            VirtualTile *vtile = vts->tileAt(tileIndex);
            QString imageSrc;
            if (version == VERSION1)
                imageSrc = ReadString(in);
            else {
                qint32 srcIndex;
                in >> srcIndex;
                imageSrc = srcNames.at(srcIndex);
            }
            qint32 srcX, srcY;
            in >> srcX;
            in >> srcY;
            vtile->setImageSource(imageSrc, srcX, srcY);
            qint32 shapeIndex;
            in >> shapeIndex;
            QString shapeName = shapes.at(shapeIndex);
            TileShape *shape = VirtualTilesetMgr::instance().tileShape(shapeName);
            if (shape == 0) {
                mError = tr("Unknown tile shape '%1'.").arg(shapeName);
                return false;
            }
            vtile->setShape(shape);
        }

        addTileset(vts);
    }

    mFileName = fileName;

    return true;
}

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++)
        out << quint8(str[i].toLatin1());
    out << quint8('\n');
}

bool VirtualTilesetsFile::write(const QString &fileName)
{
    return write(fileName, mTilesets);
}

bool VirtualTilesetsFile::write(const QString &fileName, const QList<VirtualTileset *> &tilesets)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        mError = tr("Error opening file for writing.\n%1").arg(fileName);
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    out << quint8('v') << quint8('t') << quint8('s') << quint8('f');
    out << qint32(VERSION_LATEST);

    QMap<TileShape*,int> shapeMap;
    int i = 0;
    foreach (TileShape *shape, VirtualTilesetMgr::instance().tileShapes())
        shapeMap[shape] = i++;
    out << qint32(shapeMap.size());
    foreach (TileShape *shape, VirtualTilesetMgr::instance().tileShapes()) {
        SaveString(out, shape->name());
        out << qint32(shape->mFaces.size());
        foreach (TileShapeFace e, shape->mFaces) {
            Q_ASSERT(e.mGeom.size() == e.mUV.size());
            out << qint32(e.mGeom.size());
            foreach (QVector3D v, e.mGeom)
                out << v.x() << v.y() << v.z();
            foreach (QPointF uv, e.mUV)
                out << uv.x() << uv.y();
        }
    }

    QMap<QString,int> srcNameMap;
    foreach (VirtualTileset *ts, tilesets) {
        foreach (VirtualTile *vtile, ts->tiles()) {
            if (!vtile->imageSource().isEmpty())
                srcNameMap[vtile->imageSource()] = 1;
        }
    }
    int srcNameCount = srcNameMap.values().size();
    out << qint32(srcNameCount);
    int n = 0;
    foreach (QString name, srcNameMap.keys()) {
        srcNameMap[name] = n++;
        SaveString(out, name);
    }

    out << qint32(tilesets.size());
    foreach (VirtualTileset *ts, tilesets) {
        SaveString(out, ts->name());
        out << qint32(ts->columnCount());
        out << qint32(ts->rowCount());
        QList<VirtualTile*> nonEmptyTiles;
        foreach (VirtualTile *vtile, ts->tiles()) {
            if (vtile->shape() != 0 && srcNameMap.contains(vtile->imageSource()))
                nonEmptyTiles += vtile;
        }
        out << qint32(nonEmptyTiles.size());
        foreach (VirtualTile *vtile, nonEmptyTiles) {
            out << qint32(vtile->index());
            out << qint32(srcNameMap[vtile->imageSource()]);
            out << qint32(vtile->srcX());
            out << qint32(vtile->srcY());
            out << qint32(shapeMap[vtile->shape()]);
        }
    }

    return true;
}

/////

#if 0
#include "BuildingEditor/simplefile.h"

#define VERSION0 0

#define VERSION_LATEST VERSION0

OldVirtualTilesetsTxtFile::OldVirtualTilesetsTxtFile() :
    mVersion(0),
    mRevision(0),
    mSourceRevision(0)
{
    mNameToType[QLatin1String("floor")] = VirtualTile::Floor;

    mNameToType[QLatin1String("wall_w")] = VirtualTile::WallW;
    mNameToType[QLatin1String("wall_n")] = VirtualTile::WallN;
    mNameToType[QLatin1String("wall_nw")] = VirtualTile::WallNW;
    mNameToType[QLatin1String("wall_se")] = VirtualTile::WallSE;
    mNameToType[QLatin1String("wall_window_w")] = VirtualTile::WallWindowW;
    mNameToType[QLatin1String("wall_window_n")] = VirtualTile::WallWindowN;
    mNameToType[QLatin1String("wall_door_w")] = VirtualTile::WallDoorW;
    mNameToType[QLatin1String("wall_door_n")] = VirtualTile::WallDoorN;

    mNameToType[QLatin1String("wall_short_w")] = VirtualTile::WallShortW;
    mNameToType[QLatin1String("wall_short_n")] = VirtualTile::WallShortN;
    mNameToType[QLatin1String("wall_short_nw")] = VirtualTile::WallShortNW;
    mNameToType[QLatin1String("wall_short_se")] = VirtualTile::WallShortSE;

    mNameToType[QLatin1String("roof_slope_s1")] = VirtualTile::SlopeS1;
    mNameToType[QLatin1String("roof_slope_s2")] = VirtualTile::SlopeS2;
    mNameToType[QLatin1String("roof_slope_s3")] = VirtualTile::SlopeS3;
    mNameToType[QLatin1String("roof_slope_e1")] = VirtualTile::SlopeE1;
    mNameToType[QLatin1String("roof_slope_e2")] = VirtualTile::SlopeE2;
    mNameToType[QLatin1String("roof_slope_e3")] = VirtualTile::SlopeE3;

    mNameToType[QLatin1String("roof_shallow_n1")] = VirtualTile::ShallowSlopeN1;
    mNameToType[QLatin1String("roof_shallow_n2")] = VirtualTile::ShallowSlopeN2;
    mNameToType[QLatin1String("roof_shallow_s1")] = VirtualTile::ShallowSlopeS1;
    mNameToType[QLatin1String("roof_shallow_s2")] = VirtualTile::ShallowSlopeS2;
    mNameToType[QLatin1String("roof_shallow_w1")] = VirtualTile::ShallowSlopeW1;
    mNameToType[QLatin1String("roof_shallow_w2")] = VirtualTile::ShallowSlopeW2;
    mNameToType[QLatin1String("roof_shallow_e1")] = VirtualTile::ShallowSlopeE1;
    mNameToType[QLatin1String("roof_shallow_e2")] = VirtualTile::ShallowSlopeE2;

    mNameToType[QLatin1String("roof_slope_pt5s")] = VirtualTile::SlopePt5S;
    mNameToType[QLatin1String("roof_slope_1pt5s")] = VirtualTile::SlopeOnePt5S;
    mNameToType[QLatin1String("roof_slope_2pt5s")] = VirtualTile::SlopeTwoPt5S;
    mNameToType[QLatin1String("roof_slope_pt5e")] = VirtualTile::SlopePt5E;
    mNameToType[QLatin1String("roof_slope_1pt5e")] = VirtualTile::SlopeOnePt5E;
    mNameToType[QLatin1String("roof_slope_2pt5e")] = VirtualTile::SlopeTwoPt5E;

    mNameToType[QLatin1String("roof_trim_inner")] = VirtualTile::TrimInner;
    mNameToType[QLatin1String("roof_trim_outer")] = VirtualTile::TrimOuter;
    mNameToType[QLatin1String("roof_trim_s")] = VirtualTile::TrimS;
    mNameToType[QLatin1String("roof_trim_e")] = VirtualTile::TrimE;
    mNameToType[QLatin1String("roof_trim_corner_sw")] = VirtualTile::TrimCornerSW;
    mNameToType[QLatin1String("roof_trim_corner_ne")] = VirtualTile::TrimCornerNE;

    mNameToType[QLatin1String("roof_inner1")] = VirtualTile::Inner1;
    mNameToType[QLatin1String("roof_inner2")] = VirtualTile::Inner2;
    mNameToType[QLatin1String("roof_inner3")] = VirtualTile::Inner3;
    mNameToType[QLatin1String("roof_outer1")] = VirtualTile::Outer1;
    mNameToType[QLatin1String("roof_outer2")] = VirtualTile::Outer2;
    mNameToType[QLatin1String("roof_outer3")] = VirtualTile::Outer3;

    mNameToType[QLatin1String("roof_inner_pt5")] = VirtualTile::InnerPt5;
    mNameToType[QLatin1String("roof_inner_1pt5")] = VirtualTile::InnerOnePt5;
    mNameToType[QLatin1String("roof_inner_2pt5")] = VirtualTile::InnerTwoPt5;
    mNameToType[QLatin1String("roof_outer_p5")] = VirtualTile::OuterPt5;
    mNameToType[QLatin1String("roof_outer_1pt5")] = VirtualTile::OuterOnePt5;
    mNameToType[QLatin1String("roof_outer_2pt5")] = VirtualTile::OuterTwoPt5;

    mNameToType[QLatin1String("roof_corner_sw1")] = VirtualTile::CornerSW1;
    mNameToType[QLatin1String("roof_corner_sw2")] = VirtualTile::CornerSW2;
    mNameToType[QLatin1String("roof_corner_sw3")] = VirtualTile::CornerSW3;
    mNameToType[QLatin1String("roof_corner_ne1")] = VirtualTile::CornerNE1;
    mNameToType[QLatin1String("roof_corner_ne2")] = VirtualTile::CornerNE2;
    mNameToType[QLatin1String("roof_corner_ne3")] = VirtualTile::CornerNE3;

    mNameToType[QLatin1String("roof_top_n1")] = VirtualTile::RoofTopN1;
    mNameToType[QLatin1String("roof_top_n2")] = VirtualTile::RoofTopN2;
    mNameToType[QLatin1String("roof_top_n3")] = VirtualTile::RoofTopN3;
    mNameToType[QLatin1String("roof_top_w1")] = VirtualTile::RoofTopW1;
    mNameToType[QLatin1String("roof_top_w2")] = VirtualTile::RoofTopW2;
    mNameToType[QLatin1String("roof_top_w3")] = VirtualTile::RoofTopW3;

    foreach (QString name, mNameToType.keys())
        mTypeToName[mNameToType[name]] = name;
}

OldVirtualTilesetsTxtFile::~OldVirtualTilesetsTxtFile()
{
    qDeleteAll(mTilesets);
}

bool OldVirtualTilesetsTxtFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mVersion = simple.version();
    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    if (mVersion > VERSION_LATEST) {
        mError = tr("%1 is from a newer version of TileZed").arg(info.fileName());
        return false;
    }

    qDeleteAll(mTilesets);
    mTilesets.clear();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tileset")) {
            QString name = block.value("name");
            if (name.isEmpty()) {
                mError = tr("Line %1: Missing tileset name")
                        .arg(block.lineNumber);
                return false;
            }
            if (mTilesetByName.contains(name)) {
                mError = tr("Line %1: Duplicate tileset name '%2'")
                        .arg(block.lineNumber)
                        .arg(name);
                return false;
            }
            int columnCount, rowCount;
            if (!parse2Ints(block.value("size"), &columnCount, &rowCount)) {
                mError = tr("Line %1: Invalid 'size' attribute").arg(block.lineNumber);
                return false;
            }
            VirtualTileset *vts = new VirtualTileset(name, columnCount, rowCount);

            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("tile")) {
                    // FIXME: The Tiles Directory could be unset/invalid here
                    QString file = block2.value("file");
#if 0
                    if (QDir::isRelativePath(file))
                        file = Preferences::instance()->tilesDirectory() + QLatin1String("/") + file;
#endif
                    int x, y;
                    if (!parse2Ints(block2.value("xy"), &x, &y) ||
                            !QRect(0, 0, columnCount, rowCount).contains(x, y)) {
                        mError = tr("Line %1: Invalid 'xy' attribute").arg(block2.lineNumber);
                        return false;
                    }
                    int srcX, srcY;
                    if (!parse2Ints(block2.value("file-xy"), &srcX, &srcY)) {
                        mError = tr("Line %1: Invalid 'file-xy' attribute").arg(block2.lineNumber);
                        return false;
                    }
                    QString shape = block2.value("shape");
                    VirtualTile::IsoType isoType = VirtualTile::Invalid;
                    if (!parseIsoType(shape, isoType)) {
                        mError = tr("Line %1: Invalid 'shape' attribute '%2'.")
                                .arg(block2.lineNumber)
                                .arg(shape);
                        return false;
                    }
                    vts->tileAt(x, y)->setImageSource(file, srcX, srcY);
                    vts->tileAt(x, y)->setShape(isoType);
                } else {
                    mError = tr("Line %1: Unknown block name '%1'\n%2")
                            .arg(block2.lineNumber)
                            .arg(block2.name)
                            .arg(path);
                    return false;
                }
            }
            addTileset(vts);
        } else {
            mError = tr("Line %1: Unknown block name '%1'.\n%2")
                    .arg(block.lineNumber)
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

bool OldVirtualTilesetsTxtFile::write(const QString &fileName)
{
    return write(fileName, mTilesets);
}

bool OldVirtualTilesetsTxtFile::write(const QString &fileName,
                                const QList<VirtualTileset *> &tilesets)
{
    SimpleFile simpleFile;

    QDir tilesDir(Preferences::instance()->tilesDirectory());

    foreach (VirtualTileset *vts, tilesets) {
        SimpleFileBlock block;
        block.name = QLatin1String("tileset");
        block.addValue("name", vts->name());
        block.addValue("size", QString::fromLatin1("%1,%2")
                       .arg(vts->columnCount())
                       .arg(vts->rowCount()));
        for (int i = 0; i < vts->tileCount(); i++) {
            VirtualTile *vtile = vts->tileAt(i);
            if (vtile->type() == VirtualTile::Invalid)
                continue;
            SimpleFileBlock block2;
            block2.name = QLatin1String("tile");
            block2.addValue("xy", QString::fromLatin1("%1,%2")
                           .arg(vtile->x())
                           .arg(vtile->y()));
            QString relativePath = tilesDir.relativeFilePath(vtile->imageSource());
            block2.addValue("file", relativePath);
            block2.addValue("file-xy", QString::fromLatin1("%1,%2")
                            .arg(vtile->srcX())
                            .arg(vtile->srcY()));
            block2.addValue("shape", mTypeToName[vtile->type()]);
            block.blocks += block2;
        }

        simpleFile.blocks += block;
    }

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }

    return true;
}

bool OldVirtualTilesetsTxtFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}

bool OldVirtualTilesetsTxtFile::parseIsoType(const QString &s, VirtualTile::IsoType &isoType)
{
    if (mNameToType.contains(s)) {
        isoType = mNameToType[s];
        return true;
    }
    return false;
}
#endif

/////

TileShapesFile::TileShapesFile()
{

}

TileShapesFile::~TileShapesFile()
{
    qDeleteAll(mShapes);
}

#undef VERSION_LATEST
#define VERSION_LATEST 1

bool TileShapesFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(info.fileName());
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    int version = simple.version();

    if (version < 1 || version > VERSION_LATEST) {
        mError = tr("Unknown version number %1 in %2.").arg(version).arg(info.fileName());
        return false;
    }

    qDeleteAll(mShapes);
    mShapes.clear();

    qDeleteAll(mGroups);
    mGroups.clear();

    struct SameAs {
        SameAs() : shapeName(QString()), lineNumber(-1) {}
        SameAs(QString shapeName, int lineNumber) :
            shapeName(shapeName),
            lineNumber(lineNumber)
        {}
        QString shapeName;
        int lineNumber;
    };
    QMap<TileShape*,SameAs> sameAsMap;

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("shape")) {
            QString name = block.value("name");
            if (name.isEmpty()) {
                mError = tr("Line %1: Missing shape name")
                        .arg(block.lineNumber);
                return false;
            }
            if (mShapeByName.contains(name)) {
                mError = tr("Line %1: Duplicate shape name '%2'.")
                        .arg(block.lineNumber)
                        .arg(name);
                return false;
            }
            TileShape *shape = new TileShape(name);

            SimpleFileKeyValue kv;
            if (block.keyValue("sameAs", kv)) {
                if (block.findBlock(QLatin1String("face")) != -1) {
                    mError = tr("Line %1: face not allowed with sameAs.")
                            .arg(block.lineNumber);
                    return false;
                }
                sameAsMap[shape] = SameAs(kv.value, kv.lineNumber);
            }

            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("face")) {
                    if (sameAsMap.contains(shape)) {
                        mError = tr("Line %1: face not allowed with sameAs.")
                                .arg(block.lineNumber);
                        return false;
                    }
                    TileShapeFace e;

                    QString geom = block2.value("geom");
                    QList<QVector3D> xyz;
                    if (!parseVector3DList(geom, xyz)) {
                        mError = tr("Line %1: Expected X Y Z triplets.").arg(block2.lineNumber);
                        return false;
                    }
                    e.mGeom = xyz.toVector();

                    QString uv = block2.value("uv");
                    QList<QPointF> uvs;
                    if (!parsePointFList(uv, uvs)) {
                        mError = tr("Line %1: Expected U V pairs.").arg(block2.lineNumber);
                        return false;
                    }
                    if (uvs.size() != xyz.size()) {
                        mError = tr("Line %1: %2 uv values but %3 geom values.")
                                .arg(block2.lineNumber)
                                .arg(uvs.size())
                                .arg(xyz.size());
                        return false;
                    }
                    e.mUV = uvs.toVector();

                    shape->mFaces += e;
                } else if (block2.name == QLatin1String("xform")) {
                    if (!block.hasValue("sameAs")) {
                        mError = tr("Line %1: xform not allowed without sameAs.")
                                .arg(block.lineNumber);
                        return false;
                    }
                    foreach (SimpleFileKeyValue kv, block2.values) {
                        if (kv.name == QLatin1String("rotate")) {
                            TileShapeXform xform(TileShapeXform::Rotate);
                            if (!parseVector3D(kv.value, xform.mRotate)) {
                                mError = tr("Line %1: Expected x,y,z but got '%2'.")
                                        .arg(kv.lineNumber).arg(kv.value);
                                return false;
                            }
                            shape->mXform += xform;
                        } else if (kv.name == QLatin1String("translate")) {
                            TileShapeXform xform(TileShapeXform::Translate);
                            if (!parseVector3D(kv.value, xform.mTranslate)) {
                                mError = tr("Line %1: Expected x,y,z but got '%2'.")
                                        .arg(kv.lineNumber).arg(kv.value);
                                return false;
                            }
                            shape->mXform += xform;
                        } else {
                            mError = tr("Line %1: Unknown value name '%2'.")
                                    .arg(kv.lineNumber)
                                    .arg(kv.name);
                            return false;
                        }
                    }
                } else {
                    mError = tr("Line %1: Unknown block name '%2'.")
                            .arg(block2.lineNumber)
                            .arg(block2.name);
                    return false;
                }
            }
            mShapes += shape;
            mShapeByName[shape->name()] = shape;
        } else if (block.name == QLatin1String("group")) {
            QString label = block.value("label");
            QString sizeStr = block.value("size");
            int columnCount, rowCount;
            if (!parse2Ints(sizeStr, &columnCount, &rowCount) ||
                    !QRect(0, 0, 99, 99).contains(columnCount, rowCount)) {
                mError = tr("Line %1: Invalid group size '%2'.").arg(block.lineNumber).arg(sizeStr);
                return false;
            }
            TileShapeGroup *group = new TileShapeGroup(label, columnCount, rowCount);
            mGroups += group;
            mGroupByName[label] = group; // label needn't be unique

            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("shape")) {
                    QStringList values = kv.values();
                    if (values.size() != 2) {
                        mError = tr("Line %1: Expected x,y shape.").arg(kv.lineNumber);
                        return false;
                    }
                    int col, row;
                    if (!parse2Ints(values[0], &col, &row)) {
                        mError = tr("Line %1: Expected col,row but got '%2'.")
                                .arg(kv.lineNumber)
                                .arg(values[0]);
                        return false;
                    }
                    if (!group->contains(col, row)) {
                        mError = tr("Line %1: Invalid col,row=%2,%3.")
                                .arg(kv.lineNumber)
                                .arg(col).arg(row);
                        return false;
                    }
                    QString shapeName = values[1];
                    TileShape *shape = this->shape(shapeName);
                    if (shape == 0) {
                        mError = tr("Line %1: Unknown shape '%2'.")
                                .arg(kv.lineNumber)
                                .arg(shapeName);
                        return false;
                    }
                    group->setShape(col, row, shape);
                } else if (kv.name != QLatin1String("label") &&
                           kv.name != QLatin1String("size")) {
                    mError = tr("Line %1: Unknown value name '%2'.")
                            .arg(kv.lineNumber)
                            .arg(kv.name);
                    return false;
                }
            }
        } else {
            mError = tr("Line %1: Unknown block name '%1'.")
                    .arg(block.lineNumber)
                    .arg(block.name);
            return false;
        }
    }

    // Now resolve sameAs
    foreach (TileShape *shape, sameAsMap.keys()) {
        SameAs sameAs = sameAsMap[shape];
        if (TileShape *shape2 = this->shape(sameAs.shapeName)) {
            shape->mSameAs = shape2;
            shape->fromSameAs();
        } else {
            mError = tr("Line %1: Unknown shape '%2'.")
                    .arg(sameAs.lineNumber)
                    .arg(sameAs.shapeName);
        }
    }

    return true;
}

bool TileShapesFile::write(const QString &fileName)
{
    return write(fileName, mShapes, mGroups);
}

bool TileShapesFile::write(const QString &fileName, const QList<TileShape *> &shapes,
                           const QList<TileShapeGroup*> &groups)
{
    SimpleFile simpleFile;
    foreach (TileShape *shape, shapes) {
        SimpleFileBlock shapeBlock;
        shapeBlock.name = QLatin1String("shape");
        shapeBlock.addValue("name", shape->name());
        if (shape->mSameAs) {
            shapeBlock.addValue("sameAs", shape->mSameAs->mName);
            if (shape->mXform.size()) {
                SimpleFileBlock xformBlock;
                xformBlock.name = QLatin1String("xform");
                foreach (TileShapeXform xform, shape->mXform) {
                    if (xform.mType == xform.Rotate)
                        xformBlock.addValue("rotate", toString(xform.mRotate));
                    else if (xform.mType == xform.Translate)
                        xformBlock.addValue("translate", toString(xform.mTranslate));
                    else
                        Q_ASSERT(false);
                }
                shapeBlock.blocks += xformBlock;
            }
            simpleFile.blocks += shapeBlock;
            continue;
        }
        foreach (TileShapeFace e, shape->mFaces) {
            SimpleFileBlock faceBlock;
            faceBlock.name = QLatin1String("face");
            QString geom;
            foreach (QVector3D v, e.mGeom)
                geom += toString(v) + QLatin1String(" ");
            QString uv;
            foreach (QPointF v, e.mUV)
                uv += QString::fromLatin1("%1,%2 ").arg(v.x()).arg(v.y());
            faceBlock.addValue("geom", geom);
            faceBlock.addValue("uv", uv);
            shapeBlock.blocks += faceBlock;
        }
        simpleFile.blocks += shapeBlock;
    }
    foreach (TileShapeGroup *group, groups) {
        SimpleFileBlock groupBlock;
        groupBlock.name = QLatin1String("group");
        groupBlock.addValue("label", group->label());
        groupBlock.addValue("size", QString::fromLatin1("%1,%2")
                            .arg(group->columnCount())
                            .arg(group->rowCount()));
        for (int y = 0; y < group->rowCount(); y++) {
            for (int x = 0; x < group->columnCount(); x++) {
                if (TileShape *shape = group->shapeAt(x, y))
                    groupBlock.addValue("shape", QString::fromLatin1("%1,%2 %3")
                                        .arg(x).arg(y).arg(shape->name()));
            }
        }
        simpleFile.blocks += groupBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }

    return true;
}

bool TileShapesFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}

bool TileShapesFile::parsePointF(const QString &s, QPointF &p)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    double x = coords[0].toDouble(&ok);
    if (!ok) return false;
    double y = coords[1].toDouble(&ok);
    if (!ok) return false;
    p = QPointF(x, y);
    return true;
}

bool TileShapesFile::parsePointFList(const QString &s, QList<QPointF> &out)
{
    QStringList values = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
    out.clear();
    for (int i = 0; i < values.size(); i++) {
        QPointF p;
        if (!parsePointF(values[i], p))
            return false;
        out += p;
    }
    return true;
}

bool TileShapesFile::parseVector3D(const QString &s, QVector3D &v)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 3)
        return false;
    bool ok;
    double x = coords[0].toDouble(&ok);
    if (!ok) return false;
    double y = coords[1].toDouble(&ok);
    if (!ok) return false;
    double z = coords[2].toDouble(&ok);
    if (!ok) return false;
    v = QVector3D(x, y, z);
    return true;
}

bool TileShapesFile::parseVector3DList(const QString &s, QList<QVector3D> &out)
{
    QStringList values = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
    out.clear();
    for (int i = 0; i < values.size(); i++) {
        QVector3D v;
        if (!parseVector3D(values[i], v))
            return false;
        out += v;
    }
    return true;
}

bool TileShapesFile::parseDoubles(const QString &s, int stride, QList<qreal> &out)
{
    QStringList values = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (values.size() % stride)
        return false;
    out.clear();
    for (int i = 0; i < values.size(); i++) {
        bool ok;
        double d = values[i].toDouble(&ok);
        if (!ok)
            return false;
        out += d;
    }
    return true;
}

QString TileShapesFile::toString(const QVector3D &v)
{
    return QString::fromLatin1("%1,%2,%3").arg(v.x()).arg(v.y()).arg(v.z());
}


