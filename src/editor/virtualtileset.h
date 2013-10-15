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

#ifndef VIRTUALTILESET_H
#define VIRTUALTILESET_H

#include "singleton.h"

#include <QCoreApplication>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <QVector3D>

class QGLPixelBuffer;

namespace Tiled {
class Tileset;

namespace Internal {

class TextureInfo;
class VirtualTileset;

class TileShapeFace
{
public:
    QVector<QVector3D> mGeom;
    QPolygonF mUV;
};

class TileShapeXform
{
public:
    enum Type {
        Invalid,
        Rotate,
        Translate
    };
    TileShapeXform() : mType(Invalid) {}
    TileShapeXform(Type type) : mType(type) {}

    Type mType;
    QVector3D mRotate;
    QVector3D mTranslate;
};

class TileShape
{
public:
    TileShape(const QString &name) :
        mName(name),
        mSameAs(0)
    {}

    TileShape(const TileShape *other) :
        mName(other->name()),
        mFaces(other->mFaces),
        mSameAs(other->mSameAs),
        mXform(other->mXform)
    {
    }

    void copy(const TileShape *other)
    {
        mName = other->name();
        mFaces = other->mFaces;
        mSameAs = other->mSameAs;
        mXform = other->mXform;
    }

    QString name() const { return mName; }

    void fromSameAs();

    QString mName;
    QList<TileShapeFace> mFaces;

    TileShape *mSameAs;
    QList<TileShapeXform> mXform;
};

class TileShapeGroup
{
public:
    TileShapeGroup(const QString &label, int columnCount, int rowCount) :
        mLabel(label),
        mColumnCount(columnCount),
        mRowCount(rowCount),
        mShapes(mColumnCount * mRowCount)
    {
    }

    TileShapeGroup(const TileShapeGroup *other) :
        mLabel(other->mLabel),
        mColumnCount(other->mColumnCount),
        mRowCount(other->mRowCount),
        mShapes(other->mShapes)
    {
    }

    void copy(const TileShapeGroup *other)
    {
        mLabel = other->mLabel;
        mColumnCount = other->mColumnCount;
        mRowCount = other->mRowCount;
        mShapes = other->mShapes;
    }

    QString label() const { return mLabel; }
    int columnCount() const { return mColumnCount; }
    int rowCount() const { return mRowCount; }
    int count() const { return mColumnCount * mRowCount; }

    void setLabel(const QString &label) { mLabel = label; }
    void resize(int columnCount, int rowCount);

    void setShape(int col, int row, TileShape *shape)
    {
        if (contains(col, row))
            mShapes[col + row * mColumnCount] = shape;
    }

    void setShape(int index, TileShape *shape)
    {
        if (contains(index))
            mShapes[index] = shape;
    }

    TileShape *shapeAt(int col, int row)
    {
        return contains(col, row) ? mShapes[col + row * mColumnCount] : 0;
    }

    TileShape *shapeAt(int index)
    {
        return contains(index) ? mShapes[index] : 0;
    }

    bool hasShape(TileShape *shape) const
    { return mShapes.contains(shape); }

    bool contains(int col, int row)
    {
        return QRect(0, 0, mColumnCount, mRowCount).contains(col, row);
    }

    bool contains(int index)
    {
        return index >= 0 && index < mShapes.size();
    }

private:
    QString mLabel;
    int mColumnCount;
    int mRowCount;
    QVector<TileShape*> mShapes;
};

class VirtualTile
{
public:
    VirtualTile(VirtualTileset *vts, int x, int y);
    VirtualTile(VirtualTileset *vts, int x, int y, const QString &imageSource,
                int srcX, int srcY, TileShape *shape);
    VirtualTile(const VirtualTile *other);

    VirtualTileset *tileset() const { return mTileset; }

    void copy(const VirtualTile *other);

    int x() const { return mX; }
    int y() const { return mY; }
    int index() const;

    void setImageSource(const QString &imageSource, int srcX, int srcY)
    {
        mImageSource = imageSource;
        mSrcX = srcX, mSrcY = srcY;
    }
    QString imageSource() const { return mImageSource; }
    int srcX() const { return mSrcX; }
    int srcY() const { return mSrcY; }

    void setShape(TileShape *shape) { mShape = shape; }
    TileShape *shape() const { return mShape; }
    bool usesShape(TileShape *shape);

    void setImage(const QImage &image) { mImage = image; }
    QImage image();

    void clear();

private:
    VirtualTileset *mTileset;
    int mX;
    int mY;
    QString mImageSource;
    int mSrcX;
    int mSrcY;
    TileShape *mShape;
    QImage mImage;
};

class VirtualTileset
{
public:
    VirtualTileset(const QString &name, int columnCount, int rowCount);
    ~VirtualTileset();

    void setName(const QString &name) { mName = name; }
    QString name() const { return mName; }

    int columnCount() const { return mColumnCount; }
    int rowCount() const { return mRowCount; }
    int tileCount() const { return mColumnCount * mRowCount; }

    const QVector<VirtualTile*> &tiles() const { return mTiles; }
    VirtualTile *tileAt(int n);
    VirtualTile *tileAt(int x, int y);

    void resize(int columnCount, int rowCount, QVector<VirtualTile *> &tiles);

    void tileChanged() { mImage = QImage(); }

    QImage image();

private:
    QString mName;
    int mColumnCount;
    int mRowCount;
    QVector<VirtualTile*> mTiles;
    QImage mImage;
};

class VirtualTilesetMgr : public QObject, public Singleton<VirtualTilesetMgr>
{
    Q_OBJECT
public:
    VirtualTilesetMgr();
    ~VirtualTilesetMgr();

    QString txtName() const { return QLatin1String("VirtualTilesets.txt"); }
    QString txtPath() const;

    bool readTxt();
    bool writeTxt();

    QList<VirtualTileset*> tilesets() const { return mTilesetByName.values(); }
    VirtualTileset *tileset(const QString &name)
    {
        return mTilesetByName.contains(name) ? mTilesetByName[name] : 0;
    }

    void addTileset(VirtualTileset *vts);
    void removeTileset(VirtualTileset *vts);
    void renameTileset(VirtualTileset *vts, const QString &name);
    void resizeTileset(VirtualTileset *vts, QSize &size,
                       QVector<VirtualTile*> &tiles);

    QString imageSource(VirtualTileset *vts);
    bool resolveImageSource(QString &imageSource);
    VirtualTileset *tilesetFromPath(const QString &path);

    QImage originalIsoImage(VirtualTileset *vts);

    void changeVTile(VirtualTile *vtile, VirtualTile &other);

    QImage renderIsoTile(VirtualTile *vtile);

    void addShape(TileShape *shape);
    void removeShape(TileShape *shape);
    void changeShape(TileShape *shape, TileShape &other);

    QList<TileShape*> tileShapes() const { return mShapeByName.values(); }
    TileShape *tileShape(const QString &name);

    void insertShapeGroup(int index, TileShapeGroup *g);
    TileShapeGroup *removeShapeGroup(int index);
    void editShapeGroup(TileShapeGroup *g, TileShapeGroup &other);
    void assignShape(TileShapeGroup *g, int col, int row, TileShape *shape);

    QList<TileShapeGroup*> shapeGroups() const { return mShapeGroups; }
    TileShapeGroup *shapeGroupAt(int index)
    {
        return (index >= 0 && index < mShapeGroups.size()) ? mShapeGroups[index] : 0;
    }
    QStringList shapeGroupLabels() const
    {
        QStringList labels;
        foreach (TileShapeGroup *group, mShapeGroups)
            labels += group->label();
        return labels;
    }
    TileShapeGroup *ungroupedGroup() const { return mUngroupedGroup; }

    void emitTilesetChanged(VirtualTileset *vts)
    { emit tilesetChanged(vts); }

    QImage checkerboard() const { return mCheckerboard; }

    QString errorString() const
    { return mError; }

private:
    void initPixelBuffer();
    uint loadGLTexture(const QString &imageSource, int srcX, int srcY);
    void setUngroupedGroup();

signals:
    void tilesetAdded(VirtualTileset *vts);
    void tilesetAboutToBeRemoved(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);

    void tilesetChanged(VirtualTileset *vts);

    void shapeGroupAdded(int index, TileShapeGroup *g);
    void shapeGroupRemoved(int index, TileShapeGroup *g);
    void shapeGroupChanged(TileShapeGroup *g);
    void shapeAssigned(TileShapeGroup *g, int col, int row);
    void shapeAdded(TileShape *shape);
    void shapeRemoved(TileShape *shape);
    void shapeChanged(TileShape *shape);

private slots:
    void textureAdded(TextureInfo *tex);
    void textureRemoved(TextureInfo *tex);
    void textureChanged(TextureInfo *tex);

private:
    QMap<QString,VirtualTileset*> mTilesetByName;
    QList<VirtualTileset*> mRemovedTilesets;
    QGLPixelBuffer *mPixelBuffer;

    QMap<QString,TileShape*> mShapeByName;
    QList<TileShapeGroup*> mShapeGroups;
    TileShapeGroup *mUngroupedGroup;

    QMap<QString,QImage> mOriginalIsoImages;
    QImage mCheckerboard;

//    int mSourceRevision;
//    int mRevision;
    QString mError;
};

/**
  * This class represents a single binary *.vts file.
  */
class VirtualTilesetsFile
{
    Q_DECLARE_TR_FUNCTIONS(VirtualTilesetsFile)
public:
    VirtualTilesetsFile();
    ~VirtualTilesetsFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<VirtualTileset*> &tilesets);

    QString directory() const;

    void addTileset(VirtualTileset *vts)
    { mTilesets += vts; mTilesetByName[vts->name()] = vts; }

    void removeTileset(VirtualTileset *vts);

    const QList<VirtualTileset*> &tilesets() const
    { return mTilesets; }

    QList<VirtualTileset*> takeTilesets()
    {
        QList<VirtualTileset*> ret = mTilesets;
        mTilesets.clear();
        mTilesetByName.clear();
        return ret;
    }

    VirtualTileset *tileset(const QString &name);

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }

    QString errorString() const
    { return mError; }

private:
    QList<VirtualTileset*> mTilesets;
    QMap<QString,VirtualTileset*> mTilesetByName;
    QString mFileName;
    QString mError;
};

#if 0
class OldVirtualTilesetsTxtFile
{
    Q_DECLARE_TR_FUNCTIONS(OldVirtualTilesetsTxtFile)

public:
    OldVirtualTilesetsTxtFile();
    ~OldVirtualTilesetsTxtFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<VirtualTileset*> &tilesets);

    void setRevision(int revision, int sourceRevision)
    { mRevision = revision, mSourceRevision = sourceRevision; }

    int revision() const { return mRevision; }
    int sourceRevision() const { return mSourceRevision; }

    const QList<VirtualTileset*> &tilesets() const
    { return mTilesets; }

    QList<VirtualTileset*> takeTilesets()
    {
        QList<VirtualTileset*> ret = mTilesets;
        mTilesets.clear();
        mTilesetByName.clear();
        return ret;
    }

    void addTileset(VirtualTileset *vts)
    { mTilesets += vts; mTilesetByName[vts->name()] = vts; }

    QString typeToName(VirtualTile::IsoType isoType)
    { return mTypeToName[isoType]; }

    QString errorString() const
    { return mError; }

private:
    bool parse2Ints(const QString &s, int *pa, int *pb);
    bool parseIsoType(const QString &s, VirtualTile::IsoType &isoType);

private:
    QList<VirtualTileset*> mTilesets;
    QMap<QString,VirtualTileset*> mTilesetByName;
    QMap<QString,VirtualTile::IsoType> mNameToType;
    QMap<VirtualTile::IsoType,QString> mTypeToName;

    int mVersion;
    int mRevision;
    int mSourceRevision;
    QString mError;
};
#endif

/**
  * This class represents the TileShapes.txt file.
  */
class TileShapesFile
{
    Q_DECLARE_TR_FUNCTIONS(TileShapesFile)
public:
    TileShapesFile();
    ~TileShapesFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<TileShape*> &shapes,
               const QList<TileShapeGroup *> &groups);

    const QList<TileShape*> &shapes() const
    { return mShapes; }

    QList<TileShape*> takeShapes()
    {
        QList<TileShape*> ret = mShapes;
        mShapes.clear();
        mShapeByName.clear();
        return ret;
    }

    TileShape *shape(const QString &name)
    { return mShapeByName.contains(name) ? mShapeByName[name] : 0; }

    QStringList shapeNames() const
    { return mShapeByName.keys(); }


    const QList<TileShapeGroup*> &groups() const
    { return mGroups; }

    QList<TileShapeGroup*> takeGroups()
    {
        QList<TileShapeGroup*> ret = mGroups;
        mGroups.clear();
        mGroupByName.clear();
        return ret;
    }

    TileShapeGroup *group(const QString &name)
    { return mGroupByName.contains(name) ? mGroupByName[name] : 0; }

    QStringList groupNames() const
    { return mGroupByName.keys(); }

    bool parse2Ints(const QString &s, int *pa, int *pb);
    bool parsePointF(const QString &s, QPointF &p);
    bool parsePointFList(const QString &s, QList<QPointF> &out);
    bool parseVector3D(const QString &s, QVector3D &v);
    bool parseVector3DList(const QString &s, QList<QVector3D> &v);
    bool parseDoubles(const QString &s, int stride, QList<qreal> &out);
    QString toString(const QVector3D &v);

    QString errorString() const
    { return mError; }

private:
    QList<TileShape*> mShapes;
    QMap<QString,TileShape*> mShapeByName;
    QList<TileShapeGroup*> mGroups;
    QMap<QString,TileShapeGroup*> mGroupByName;
    QString mFileName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // VIRTUALTILESET_H
