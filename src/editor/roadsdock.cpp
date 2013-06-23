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

#include "roadsdock.h"

#include "celldocument.h"
#include "undoredo.h"
#include "scenetools.h"
#include "world.h"
#include "worlddocument.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

/////

#include "preferences.h"
#include <QFileInfo>
#include <QImage>
#include <QPixmap>

class TilesetManager
{
public:
    class Tileset
    {
    public:
        int mColumnCount;
        QVector<QPixmap> mTiles;
    };

    QPixmap parseTileDescription(const QString &desc)
    {
        int n = desc.lastIndexOf(QLatin1Char('_'));
        if (n < 0)
            return QPixmap();
        QString tilesetName = desc.mid(0, n);
        int tileNum = desc.mid(n + 1).toInt();

        if (Tileset *tileset = getTileset(tilesetName))
            return tileset->mTiles.at(tileNum);
        return QPixmap();
    }

    Tileset *getTileset(const QString &name)
    {
        if (mTilesets.contains(name))
            return mTilesets[name];

        Preferences *prefs = Preferences::instance();
        QFileInfo info(prefs->tilesDirectory() + QLatin1Char('/') + name + QLatin1String(".png"));
        if (info.exists()) {
            mTilesets[name] = loadTileset(info.absoluteFilePath());
            return mTilesets[name];
        }
        return 0;
    }

    Tileset *loadTileset(const QString &path)
    {
        Tileset *ts = new Tileset;
        QImage image = QImage(path);
        for (int y = 0; y < image.height(); y += 128) {
            for (int x = 0; x < image.width(); x += 64) {
                QImage tileImage = image.copy(x, y, 64, 128);
                ts->mTiles += QPixmap::fromImage(tileImage);
            }
        }
        return ts;
    }

    QMap<QString,Tileset*> mTilesets;
};

/////

TilesetManager gTilesetManager;

/////

RoadsDock::RoadsDock(QWidget *parent)
    : QDockWidget(parent)
    , mCellDoc(0)
    , mWorldDoc(0)
    , mRoadWidthSpinBox(0)
    , mTrafficLineComboBox(0)
    , mRoadTypeView(0)
    , mSynching(false)
{
    setObjectName(QLatin1String("RoadsDock"));
    retranslateUi();

    QWidget *w = new QWidget(this);

    /////

    mNumSelectedLabel = new QLabel(w);
    mNumSelectedLabel->setText(tr("No roads selected."));

    /////

    QLabel *label = new QLabel(w);
    label->setText(tr("Width:"));

    mRoadWidthSpinBox = new QSpinBox(w);
    mRoadWidthSpinBox->setMinimum(1);
    mRoadWidthSpinBox->setMaximum(50);
    mRoadWidthSpinBox->setValue(1);

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->addWidget(label);
    hlayout->addWidget(mRoadWidthSpinBox);

    connect(mRoadWidthSpinBox, SIGNAL(valueChanged(int)),
            SLOT(roadWidthSpinBoxValueChanged(int)));

    /////

    label = new QLabel(w);
    label->setText(tr("Lines:"));

    mTrafficLineComboBox = new QComboBox(w);
    foreach (TrafficLines *lines, RoadTemplates::instance()->trafficLines()) {
        mTrafficLineComboBox->addItem(lines->name);
    }

    QHBoxLayout *hlayout2 = new QHBoxLayout();
    hlayout2->addWidget(label);
    hlayout2->addWidget(mTrafficLineComboBox);

    connect(mTrafficLineComboBox, SIGNAL(currentIndexChanged(int)),
            SLOT(trafficLineComboBoxActivated(int)));

    /////

    mRoadTypeView = new RoadTypeView(w);

    connect(mRoadTypeView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(roadTypeSelected()));

    /////

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(mNumSelectedLabel);
    layout->addLayout(hlayout);
    layout->addLayout(hlayout2);
    layout->addWidget(mRoadTypeView);
    w->setLayout(layout);

    setWidget(w);

#if 0
    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));
#endif
}

void RoadsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void RoadsDock::retranslateUi()
{
    setWindowTitle(tr("Roads"));
}

void RoadsDock::setDocument(Document *doc)
{
    if (mCellDoc)
        mCellDoc->disconnect(this);
    if (mWorldDoc)
        mWorldDoc->disconnect(this);

    mCellDoc = doc ? doc->asCellDocument() : 0;
    mWorldDoc = doc ? doc->asWorldDocument() : 0;

    WorldDocument *worldDoc = mWorldDoc ? mWorldDoc
                                        : (mCellDoc ? mCellDoc->worldDocument()
                                                    : 0);

    if (worldDoc) {
        mRoadWidthSpinBox->setValue(WorldCreateRoadTool::instance()->currentRoadWidth());
        connect(worldDoc, SIGNAL(selectedRoadsChanged()),
                SLOT(selectedRoadsChanged()));
        connect(worldDoc, SIGNAL(roadWidthChanged(int)),
                SLOT(selectedRoadsChanged()));
        connect(worldDoc, SIGNAL(roadTileNameChanged(int)),
                SLOT(selectedRoadsChanged()));
        connect(worldDoc, SIGNAL(roadLinesChanged(int)),
                SLOT(selectedRoadsChanged()));
    }
}

void RoadsDock::clearDocument()
{
    setDocument(0);
}

void RoadsDock::selectedRoadsChanged()
{
    WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : (mCellDoc ? mCellDoc->worldDocument() : 0);
    if (!worldDoc)
        return;
    QList<Road*> selectedRoads = worldDoc->selectedRoads();
    if (selectedRoads.count() > 0) {
        Road *road = selectedRoads.first();
        mSynching = true;
        mRoadWidthSpinBox->setValue(road->width());
        mTrafficLineComboBox->setCurrentIndex(RoadTemplates::instance()->trafficLines().indexOf(road->trafficLines()));
        mRoadTypeView->selectTileForRoad(road);
        mSynching = false;
    } else {
    }

    mNumSelectedLabel->setText(tr("%1 roads selected").arg(selectedRoads.count()));
}

void RoadsDock::roadWidthSpinBoxValueChanged(int newValue)
{
    WorldCreateRoadTool::instance()->setCurrentRoadWidth(newValue);
    if (mSynching)
        return;
    WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : (mCellDoc ? mCellDoc->worldDocument() : 0);
    if (!worldDoc)
        return;
    QList<Road*> roads;
    foreach (Road *road, worldDoc->selectedRoads()) {
        if (road->width() == newValue)
            continue;
        roads += road;
    }
    int count = roads.count();
    if (!count)
        return;
    if (count > 1)
        worldDoc->undoStack()->beginMacro(tr("Change %1 Roads' Width to %2")
                                           .arg(count).arg(newValue));
    foreach (Road *road, roads) {
        worldDoc->changeRoadWidth(road, newValue);
    }
    if (count > 1)
        worldDoc->undoStack()->endMacro();
}

void RoadsDock::trafficLineComboBoxActivated(int index)
{
    TrafficLines *lines = RoadTemplates::instance()->trafficLines().at(index);
    WorldCreateRoadTool::instance()->setCurrentTrafficLines(lines);
    if (mSynching)
        return;
    WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : (mCellDoc ? mCellDoc->worldDocument() : 0);
    if (worldDoc) {
        QList<Road*> roads;
        foreach (Road *road, worldDoc->selectedRoads()) {
            if (road->trafficLines() != lines)
                roads += road;
        }
        if (int count = roads.count()) {
            if (count > 1)
                worldDoc->undoStack()->beginMacro(tr("Change %1 Roads' Lines").arg(count));
            foreach (Road *road, roads)
                worldDoc->changeRoadLines(road, lines);
            if (count > 1)
                worldDoc->undoStack()->endMacro();
        }
    }
}

void RoadsDock::roadTypeSelected()
{
    QModelIndex index = mRoadTypeView->currentIndex();
    int tileNum = mRoadTypeView->model()->tileAt(index);
    if (tileNum != -1) {
        QString tileName = RoadTemplates::instance()->roadTiles().at(tileNum);
        WorldCreateRoadTool::instance()->setCurrentTileName(tileName);
        if (mSynching)
            return;
        WorldDocument *worldDoc = mWorldDoc ? mWorldDoc : (mCellDoc ? mCellDoc->worldDocument() : 0);
        if (worldDoc) {
            QList<Road*> roads;
            foreach (Road *road, worldDoc->selectedRoads()) {
                if (road->tileName() !=  tileName)
                    roads += road;
            }
            if (int count = roads.count()) {
                if (count > 1)
                    worldDoc->undoStack()->beginMacro(tr("Change %1 Roads' Tile").arg(count));
                foreach (Road *road, roads)
                    worldDoc->changeRoadTileName(road, tileName);
                if (count > 1)
                    worldDoc->undoStack()->endMacro();
            }
        }
    }
}

/////

RoadTypeModel::RoadTypeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RoadTypeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    int numTiles = RoadTemplates::instance()->roadTiles().count();
    int rows = numTiles / columnCount();
    if (numTiles % columnCount() > 0)
        ++rows;
    return rows;
}

int RoadTypeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    int numTiles = RoadTemplates::instance()->roadTiles().count();
    if (numTiles > 4)
        return 4;
    return numTiles;
}

QVariant RoadTypeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole) {
        int n = index.row() * columnCount() + index.column();
        if (n < RoadTemplates::instance()->roadTiles().count()) {
            QString desc = RoadTemplates::instance()->roadTiles().at(n);
            return gTilesetManager.parseTileDescription(desc);
        }
    }

    return QVariant();
}

QVariant RoadTypeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)

    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex RoadTypeModel::indexOfTile(int tileNum)
{
    int row = tileNum / columnCount();
    int col = tileNum % columnCount();
    return index(row, col);
}

int RoadTypeModel::tileAt(const QModelIndex &index)
{
    if (index.isValid()) {
        int tileNum = index.column() + index.row() * columnCount();
        if (tileNum < RoadTemplates::instance()->roadTiles().count())
            return tileNum;
    }
    return -1;
}

/////

#include <QAbstractItemDelegate>
#include <QPainter>

class RoadTileDelegate : public QAbstractItemDelegate
{
public:
    RoadTileDelegate(QObject *parent = 0)
        : QAbstractItemDelegate(parent)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const
    {
        const QVariant display = index.model()->data(index, Qt::DisplayRole);
        const QPixmap tilePixmap = display.value<QPixmap>();
        painter->drawPixmap(option.rect.adjusted(0, 0, -2, -2), tilePixmap);

        // Overlay with highlight color when selected
        if (option.state & QStyle::State_Selected) {
            const qreal opacity = painter->opacity();
            painter->setOpacity(0.5);
            painter->fillRect(option.rect.adjusted(0, 0, -2, -2),
                              option.palette.highlight());
            painter->setOpacity(opacity);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(64 + 2, 128 + 2);
    }
};

RoadTypeView::RoadTypeView(QWidget *parent)
    : QTableView(parent)
    , mModel(new RoadTypeModel)
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new RoadTileDelegate(this));
    setShowGrid(false);

    QHeaderView *header = horizontalHeader();
    header->hide();
#if QT_VERSION >= 0x050000
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    header->setResizeMode(QHeaderView::ResizeToContents);
#endif
    header->setMinimumSectionSize(1);

    header = verticalHeader();
    header->hide();
#if QT_VERSION >= 0x050000
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    header->setResizeMode(QHeaderView::ResizeToContents);
#endif
    header->setMinimumSectionSize(1);

    // Hardcode this view on 'left to right' since it doesn't work properly
    // for 'right to left' languages.
    setLayoutDirection(Qt::LeftToRight);

    setSelectionMode(SingleSelection);

    setModel(mModel);
}

void RoadTypeView::selectTileForRoad(Road *road)
{
    QString tile = road->tileName();
    int n = RoadTemplates::instance()->roadTiles().indexOf(tile);
    if (n >= 0) {
        selectionModel()->setCurrentIndex(model()->indexOfTile(n),
                                          QItemSelectionModel::ClearAndSelect);
    }
}
