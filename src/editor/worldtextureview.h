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

#ifndef WORLDTEXTUREVIEW_H
#define WORLDTEXTUREVIEW_H

#include <QFutureWatcher>
#include <QTableView>

class WorldTexture;

class WorldTextureModel : public QAbstractListModel
{
    Q_OBJECT
public:
    WorldTextureModel(QObject *parent = 0);
    ~WorldTextureModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    void setColumnCount(int count);

    void setTextures(const QList<WorldTexture*> &textures);

    WorldTexture *textureAt(const QModelIndex &index) const;

    QModelIndex indexOfTexture(WorldTexture *tex);

private slots:
    void imageLoaded();

private:
    class Pixmaps
    {
    public:
        QMap<QString,QPixmap> d;
        QList<QFuture<QImage> *> f;
        QList<QFutureWatcher<QImage> *> fw;
        QList<QString> fileName;
    };

    QList<WorldTexture*> mTextures;
    int mColumnCount;
    Pixmaps *mPixmaps; // work around 'const'
};

class WorldTextureView : public QTableView
{
    Q_OBJECT
public:
    explicit WorldTextureView(QWidget *parent = 0);

    void setTextures(const QList<WorldTexture*> &textures);

    WorldTextureModel *model() const
    { return mModel; }
    
protected:
    void resizeEvent(QResizeEvent *event);

private:
    WorldTextureModel *mModel;
};

#endif // WORLDTEXTUREVIEW_H
