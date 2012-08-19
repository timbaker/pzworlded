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

#ifndef LEVELSDOCK_H
#define LEVELSDOCK_H

#include <QDockWidget>
#include <QTreeView>

class CellDocument;
class CompositeLayerGroup;
class Document;
class LayersModel;
class LayersView;
class WorldCell;

namespace Tiled {
class ZTileLayerGroup;
}

class QLabel;
class QSlider;

class LayersDock : public QDockWidget
{
    Q_OBJECT

public:
    LayersDock(QWidget *parent = 0);

    void setCellDocument(CellDocument *doc);

protected:
    void changeEvent(QEvent *e);

private slots:
    void documentAboutToClose(int index, Document *doc);
    void cellMapFileAboutToChange(WorldCell *cell);
    void opacitySliderValueChanged(int value);
    void updateOpacitySlider();

private:
    void retranslateUi();

    void saveExpandedLevels(CellDocument *mapComposite);
    void restoreExpandedLevels(CellDocument *mapComposite);

    LayersView *mView;
    CellDocument *mCellDocument;
    QMap<CellDocument*,QList<CompositeLayerGroup*> > mExpandedLevels;
    QLabel *mOpacityLabel;
    QSlider *mOpacitySlider;
};

class LayersView : public QTreeView
{
    Q_OBJECT

public:
    LayersView(QWidget *parent = 0);

    QSize sizeHint() const;

    void setCellDocument(CellDocument *doc);
    LayersModel *model() const { return mModel; }

protected slots:
    virtual void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private slots:
    void currentLevelOrLayerIndexChanged(int index);
    void onActivated(const QModelIndex &index);

private:
    CellDocument *mCellDocument;
    bool mSynching;
    LayersModel *mModel;
};

#endif // LEVELSDOCK_H
