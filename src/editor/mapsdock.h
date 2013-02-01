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

#ifndef MAPSDOCK_H
#define MAPSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class MapImage;

class QFileSystemModel;
class QLabel;
class QLineEdit;
class QModelIndex;
class QTreeView;

class MapsView;

class MapsDock : public QDockWidget
{
    Q_OBJECT

public:
    MapsDock(QWidget *parent = 0);

private slots:
    void browse();
    void editedMapsDirectory();
    void onMapsDirectoryChanged();
    void selectionChanged();
    void onMapImageChanged(MapImage *mapImage);
    void mapImageFailedToLoad(MapImage *mapImage);

protected:
    void changeEvent(QEvent *e);

private:
    void retranslateUi();

    QLabel *mPreviewLabel;
    MapImage *mPreviewMapImage;
    QLineEdit *mDirectoryEdit;
    MapsView *mMapsView;
};

class MapsView : public QTreeView
{
    Q_OBJECT

public:
    MapsView(QWidget *parent = 0);

    QSize sizeHint() const;

    void mousePressEvent(QMouseEvent *event);

    QFileSystemModel *model() const { return mFSModel; }

private slots:
    void onMapsDirectoryChanged();
    void onActivated(const QModelIndex &index);

private:
    QFileSystemModel *mFSModel;
};

#endif // MAPSDOCK_H
