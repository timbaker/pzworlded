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

#ifndef PATHLAYERSDOCK_H
#define PATHLAYERSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class Document;
class PathDocument;
class PathLayersModel;
class PathWorld;
class WorldPathLayer;

class PathLayersView;

namespace Ui {
class PathLayersDock;
}

class PathLayersDock : public QDockWidget
{
    Q_OBJECT
public:
    PathLayersDock(QWidget *parent = 0);

    void setDocument(PathDocument *doc);
    PathDocument *document() const
    { return mDocument; }

    PathWorld *world() const;

protected:
    void changeEvent(QEvent *e);

private slots:
    void newLayer();
    void removeLayer();
    void moveUp();
    void moveDown();

    void documentAboutToClose(int index, Document *doc);

private:
    void retranslateUi();

    void saveExpandedLevels(PathDocument *doc);
    void restoreExpandedLevels(PathDocument *doc);

    PathLayersView *mView;
    PathDocument *mDocument;
    QMap<PathDocument*,QList<WorldPathLayer*> > mExpandedLevels;
    Ui::PathLayersDock *ui;
};

class PathLayersView : public QTreeView
{
    Q_OBJECT
public:
    PathLayersView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setDocument(PathDocument *doc);
    PathLayersModel *model() const { return mModel; }

protected slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
//    void currentLevelOrLayerIndexChanged(int index);
//    void onActivated(const QModelIndex &index);

private:
    PathDocument *mDocument;
    bool mSynching;
    PathLayersModel *mModel;
};

#endif // PATHLAYERSDOCK_H
