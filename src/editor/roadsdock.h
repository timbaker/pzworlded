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

#ifndef ROADSDOCK_H
#define ROADSDOCK_H

#include <QAbstractListModel>
#include <QDockWidget>
#include <QTableView>

class CellDocument;
class Document;
class Road;
class WorldDocument;

class QComboBox;
class QLabel;
class QSpinBox;

class RoadTypeView;

class RoadsDock : public QDockWidget
{
    Q_OBJECT

public:
    RoadsDock(QWidget *parent = 0);

    void changeEvent(QEvent *e);
    void retranslateUi();

    void setDocument(Document *doc);
    void clearDocument();

private slots:
    void selectedRoadsChanged();
    void roadWidthSpinBoxValueChanged(int newValue);
    void trafficLineComboBoxActivated(int index);
    void roadTypeSelected();

private:
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
    QLabel *mNumSelectedLabel;
    QSpinBox *mRoadWidthSpinBox;
    QComboBox *mTrafficLineComboBox;
    RoadTypeView *mRoadTypeView;
    bool mSynching;
};

class RoadTypeModel : public QAbstractListModel
{
public:
    RoadTypeModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex indexOfTile(int tileNum);

    int tileAt(const QModelIndex &index);
};

class RoadTypeView : public QTableView
{
public:
    RoadTypeView(QWidget *parent = 0);

    RoadTypeModel *model() const
    { return mModel; }

    QSize sizeHint() { return QSize((64+2)*3, 128+2); }

    void setDocument(Document *doc);
    void clearDocument();

    void selectTileForRoad(Road *road);

private:
    RoadTypeModel *mModel;
    CellDocument *mCellDoc;
    WorldDocument *mWorldDoc;
};

#endif // ROADSDOCK_H
