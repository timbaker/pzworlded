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

#include "worldtextureview.h"

#include "path.h"
#include "pathworld.h"

#include <QDebug>
#include <QHeaderView>

/////

WorldTextureModel::WorldTextureModel(QObject *parent) :
    QAbstractListModel(parent),
    mColumnCount(2),
    mPixmaps(new Pixmaps)
{
}

WorldTextureModel::~WorldTextureModel()
{
    delete mPixmaps;
}

int WorldTextureModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    int numTiles = mTextures.size();
    int rows = numTiles / columnCount();
    if (numTiles % columnCount() > 0)
        ++rows;
    return rows;
}

int WorldTextureModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mColumnCount;
}

Qt::ItemFlags WorldTextureModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (!textureAt(index))
        rc &= ~Qt::ItemIsEnabled;
    return rc;
}

#include <QtConcurrentRun>
static QImage loadImage(const QString &fileName)
{
    QImage image = QImage(fileName);
    if (!image.isNull())
        return image.scaled(256, 256, Qt::KeepAspectRatio);
    qDebug() << "WorldTextureModel failed to load image" << fileName;
    return image;
}

QVariant WorldTextureModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole) {
        int n = index.row() * columnCount() + index.column();
        if (n < mTextures.size()) {
            WorldTexture *tex = mTextures[n];
            if (!mPixmaps->d.contains(tex->mFileName)) {
#if 1
                mPixmaps->f += new QFuture<QImage>(QtConcurrent::run(loadImage, tex->mFileName));
                mPixmaps->fw += new QFutureWatcher<QImage>();
                connect(mPixmaps->fw.last(), SIGNAL(finished()), SLOT(imageLoaded()));
                mPixmaps->fw.last()->setFuture(*mPixmaps->f.last());
                mPixmaps->fileName += tex->mFileName;
#else
                mPixmaps->d[tex->mFileName] = QPixmap::fromImage(QImage(tex->mFileName).scaled(256, 256, Qt::KeepAspectRatio));
#endif
            }
            return mPixmaps->d[tex->mFileName];
        }
    }

    return QVariant();
}

QVariant WorldTextureModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)

    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

void WorldTextureModel::setColumnCount(int count)
{
    mColumnCount = count;
    reset();
}

void WorldTextureModel::setTextures(const QList<WorldTexture *> &textures)
{
    mTextures = textures;

    reset();
}

QModelIndex WorldTextureModel::indexOfTexture(WorldTexture *tex)
{
    int tileNum = mTextures.indexOf(tex);
    int row = tileNum / columnCount();
    int col = tileNum % columnCount();
    return index(row, col);
}

void WorldTextureModel::imageLoaded()
{
    QFutureWatcher<QImage> *fw = (QFutureWatcher<QImage>*)sender();
    int n = mPixmaps->fw.indexOf(fw);
    QString fileName = mPixmaps->fileName[n];
    mPixmaps->d[fileName] = QPixmap::fromImage(fw->result());

    delete mPixmaps->f.takeAt(n);
    delete mPixmaps->fw.takeAt(n);
    mPixmaps->fileName.takeAt(n);

    foreach (WorldTexture *tex, mTextures)
        if (tex->mFileName == fileName) {
            emit dataChanged(indexOfTexture(tex), indexOfTexture(tex));
            break;
        }
}

WorldTexture *WorldTextureModel::textureAt(const QModelIndex &index) const
{
    if (index.isValid()) {
        int tileNum = index.column() + index.row() * columnCount();
        if (tileNum < mTextures.size())
            return mTextures[tileNum];
    }
    return 0;
}

/////

#include <QAbstractItemDelegate>
#include <QPainter>

class WorldTextureDelegate : public QAbstractItemDelegate
{
public:
    WorldTextureDelegate(QObject *parent = 0) :
        QAbstractItemDelegate(parent),
        mBorderWidth(2)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const
    {
        WorldTextureModel *m = (WorldTextureModel*) index.model();
        if (!m->textureAt(index)) return;

        int bw = mBorderWidth;
        painter->fillRect(option.rect.adjusted(bw, bw, -bw, -bw), Qt::black);

        const QVariant display = index.model()->data(index, Qt::DisplayRole);
        const QPixmap tilePixmap = display.value<QPixmap>();
        painter->drawPixmap(option.rect.topLeft() + QPoint(mBorderWidth, mBorderWidth),
                            tilePixmap);

#if 1
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QPen(option.palette.highlight(), bw));
            painter->drawRect(option.rect.adjusted(bw/2, bw/2, -bw/2, -bw/2));
        }
#else
        // Overlay with highlight color when selected
        if (option.state & QStyle::State_Selected) {
            const qreal opacity = painter->opacity();
            painter->setOpacity(0.5);
            painter->fillRect(option.rect.adjusted(bw, bw, -bw, -bw),
                              option.palette.highlight());
            painter->setOpacity(opacity);
        }
#endif
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(mBorderWidth + 256 + mBorderWidth,
                     mBorderWidth + 256 + mBorderWidth);
    }

    int mBorderWidth;
};

WorldTextureView::WorldTextureView(QWidget *parent) :
    QTableView(parent),
    mModel(new WorldTextureModel)
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new WorldTextureDelegate(this));
    setShowGrid(false);
#if 0
    QPalette p = palette();
    p.setColor(QPalette::Base, QColor("black"));
    setPalette(p);
#endif
    QHeaderView *header = horizontalHeader();
    header->hide();
    header->setResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(1);

    header = verticalHeader();
    header->hide();
    header->setResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(1);

    // Hardcode this view on 'left to right' since it doesn't work properly
    // for 'right to left' languages.
    setLayoutDirection(Qt::LeftToRight);

    setSelectionMode(SingleSelection);

    setModel(mModel);
}

void WorldTextureView::setTextures(const QList<WorldTexture *> &textures)
{
    model()->setTextures(textures);
}

void WorldTextureView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    model()->setColumnCount(qMax(1, width() / 256));
}

