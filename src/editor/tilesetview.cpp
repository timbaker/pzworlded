/*
 * tilesetview.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "tilesetview.h"

#include "map.h"
#ifndef WORLDED
#include "mapdocument.h"
#endif
#include "preferences.h"
#ifndef WORLDED
#include "propertiesdialog.h"
#include "tmxmapwriter.h"
#endif
#include "tile.h"
#include "tileset.h"
#include "tilesetmodel.h"
#ifdef ZOMBOID
#include "mapcomposite.h"
#include "tilelayer.h"
#include "tilesetmanager.h"
#endif
#ifndef WORLDED
#include "utils.h"
#endif
#include "zoomable.h"

#include <QAbstractItemDelegate>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QUndoCommand>
#include <QWheelEvent>
#ifdef ZOMBOID
#include <QScrollBar>
#endif

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

/**
 * The delegate for drawing tile items in the tileset view.
 */
class TileDelegate : public QAbstractItemDelegate
{
public:
    TileDelegate(TilesetView *tilesetView, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mTilesetView(tilesetView)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    TilesetView *mTilesetView;
};

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    // Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int extra = mTilesetView->drawGrid() ? 1 : 0;

    if (mTilesetView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

#ifdef ZOMBOID
    const QFontMetrics fm = painter->fontMetrics();
#ifdef ZOMBOID_TILE_LAYER_NAMES
    const int labelHeight = mTilesetView->showLayerNames() ? fm.lineSpacing() : 0;
#else
    const int labelHeight = 0;
#endif
    const TilesetModel *m = static_cast<const TilesetModel*>(index.model());
    if (Tile *tile = m->tileAt(index)) {
        const QMargins margins = tile->drawMargins(mTilesetView->zoomable()->scale());
        painter->drawPixmap(option.rect.adjusted(margins.left(), margins.top(), -extra - margins.right(), -extra - labelHeight - margins.bottom()), tileImage);
    }
#ifdef ZOMBOID_TILE_LAYER_NAMES
    if (mTilesetView->showLayerNames()) {
        const QVariant decoration = index.model()->data(index, Qt::DecorationRole);
        QString layerName = decoration.toString();
        if (layerName.isEmpty())
            layerName = QLatin1String("???");

        QString name = fm.elidedText(layerName, Qt::ElideRight, option.rect.width());
        painter->drawText(option.rect.left(), option.rect.bottom() - labelHeight, option.rect.width(), labelHeight, Qt::AlignHCenter, name);
    }
#endif
    if (mTilesetView->drawGrid()) {
        painter->fillRect(option.rect.right(), option.rect.top(), 1, option.rect.height(), Qt::lightGray);
        painter->fillRect(option.rect.left(), option.rect.bottom(), option.rect.width(), 1, Qt::lightGray);
    }
#else
    painter->drawPixmap(option.rect.adjusted(0, 0, -extra, -extra), tileImage);
#endif

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(0, 0, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem & option,
                             const QModelIndex &index) const
{
    if (!index.isValid())
        return QSize();
    const TilesetModel *m = static_cast<const TilesetModel*>(index.model());
    const Tileset *tileset = m->tileset();
    const qreal zoom = mTilesetView->zoomable()->scale();
    const int extra = mTilesetView->drawGrid() ? 1 : 0;
#ifdef ZOMBOID
    const QFontMetrics &fm = option.fontMetrics;
#ifdef ZOMBOID_TILE_LAYER_NAMES
    const int labelHeight = mTilesetView->showLayerNames() ? fm.lineSpacing() : 0;
#else
    const int labelHeight = 0;
#endif
    return QSize(tileset->tileWidth() * zoom + extra,
                 tileset->tileHeight() * zoom + extra + labelHeight);
#else
    return QSize(tileset->tileWidth() * zoom + extra,
                 tileset->tileHeight() * zoom + extra);
#endif
}



} // anonymous namespace

#ifdef ZOMBOID
// I added this constructor for QtDesigner, all the code is the same as the
// other constructor (only the new Zoomable is different).
TilesetView::TilesetView(QWidget *parent)
    : QTableView(parent)
    , mZoomable(new Zoomable(this))
    , mMapDocument(0)
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(32);
    setItemDelegate(new TileDelegate(this, this));
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

    Preferences *prefs = Preferences::instance();
#ifndef WORLDED
    mDrawGrid = prefs->showTilesetGrid();
#endif

    connect(mZoomable, &Zoomable::scaleChanged, this, &TilesetView::adjustScale);
#ifndef WORLDED
    connect(prefs, &Preferences::showTilesetGridChanged,
            this, &TilesetView::setDrawGrid);
#ifdef ZOMBOID
    mShowLayerNames = prefs->autoSwitchLayer();
    connect(prefs, &Preferences::autoSwitchLayerChanged,
            this, &TilesetView::autoSwitchLayerChanged);
#endif
#endif // WORLDED
}

TilesetView::TilesetView(Zoomable *zoomable, QWidget *parent)
    : QTableView(parent)
    , mZoomable(zoomable)
    , mMapDocument(0)
#else
TilesetView::TilesetView(MapDocument *mapDocument, Zoomable *zoomable, QWidget *parent)
    : QTableView(parent)
    , mZoomable(zoomable)
    , mMapDocument(mapDocument)
#endif
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(32);
    setItemDelegate(new TileDelegate(this, this));
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

    Preferences *prefs = Preferences::instance();
#ifndef WORLDED
    mDrawGrid = prefs->showTilesetGrid();
#endif

    connect(mZoomable, &Zoomable::scaleChanged, this, &TilesetView::adjustScale);
#ifndef WORLDED
    connect(prefs, &Preferences::showTilesetGridChanged,
            this, &TilesetView::setDrawGrid);
#ifdef ZOMBOID
    mShowLayerNames = prefs->autoSwitchLayer();
    connect(prefs, &Preferences::autoSwitchLayerChanged,
            this, &TilesetView::autoSwitchLayerChanged);
#endif
#endif // WORLDED
}

#ifdef ZOMBOID
#ifndef WORLDED
void TilesetView::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
    tilesetModel()->setMapDocument(mapDocument);

    if (mMapDocument) {
    }
}
#endif
#endif

QSize TilesetView::sizeHint() const
{
    return QSize(130, 100);
}

#ifdef ZOMBOID
#ifndef WORLDED
bool TilesetView::showLayerNames() const
{
    return Preferences::instance()->autoSwitchLayer() && mShowLayerNames;
}
#endif
#endif

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void TilesetView::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if ((event->modifiers() & Qt::ControlModifier) && (numDegrees.y() != 0))
    {
        QPoint numSteps = numDegrees / 15;
        mZoomable->handleWheelDelta(numSteps.y() * 120);
        return;
    }

    QTableView::wheelEvent(event);
}

#ifdef ZOMBOID
#ifndef WORLDED
class ChangeTileLayerName : public QUndoCommand
{
public:
    ChangeTileLayerName(MapDocument *mapDocument,
                  Tile *tile,
                  const QString &oldName,
                  const QString &newName)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Tile Layer Name"))
        , mMapDocument(mapDocument)
        , mTile(tile)
        , mOldName(oldName)
        , mNewName(newName)
    {
        redo();
    }

    void undo()
    {
        mMapDocument->setTileLayerName(mTile, mOldName);
    }

    void redo()
    {
        mMapDocument->setTileLayerName(mTile, mNewName);
    }

private:
    MapDocument *mMapDocument;
    Tile *mTile;
    QString mOldName;
    QString mNewName;
};
#endif // WORLDED
#endif // ZOMBOID

/**
 * Allow changing tile properties through a context menu.
 */
void TilesetView::contextMenuEvent(QContextMenuEvent *event)
{
#ifndef WORLDED
    const QModelIndex index = indexAt(event->pos());
    const TilesetModel *m = tilesetModel();
    Tile *tile = m->tileAt(index);

#ifdef ZOMBOID
    const bool isExternal = (m->tileset() != nullptr) && m->tileset()->isExternal();
#else
    const bool isExternal = m->tileset()->isExternal();
#endif
    QMenu menu;

    QIcon propIcon(QLatin1String(":images/16x16/document-properties.png"));

#ifdef ZOMBOID
    QAction *actionProperties = 0;
    if (tile) {
        actionProperties = menu.addAction(propIcon,
                                          tr("Tile &Properties..."));
        actionProperties->setEnabled(!isExternal);
        Utils::setThemeIcon(actionProperties, "document-properties");
        menu.addSeparator();
    }
#else
    if (tile) {
        // Select this tile to make sure it is clear that only the properties
        // of a single tile are being edited.
        selectionModel()->setCurrentIndex(index,
                                          QItemSelectionModel::SelectCurrent |
                                          QItemSelectionModel::Clear);

        QAction *tileProperties = menu.addAction(propIcon,
                                                 tr("Tile &Properties..."));
        tileProperties->setEnabled(!isExternal);
        Utils::setThemeIcon(tileProperties, "document-properties");
        menu.addSeparator();

        connect(tileProperties, SIGNAL(triggered()),
                SLOT(editTileProperties()));
    }
#endif

#ifdef ZOMBOID
    QVector<QAction*> layerActions;
    QStringList layerNames;
    if (tile) {
        menu.addSeparator();

        // Get a list of layer names from the current map
        QSet<QString> set;
        foreach (TileLayer *tl, mMapDocument->map()->tileLayers()) {
            if (tl->group()) {
                QString name = MapComposite::layerNameWithoutPrefix(tl);
                set.insert(name);
            }
        }

        // Get a list of layer names for the current tileset
        for (int i = 0; i < m->tileset()->tileCount(); i++) {
            Tile *tile = m->tileset()->tileAt(i);
            QString layerName = TilesetManager::instance()->layerName(tile);
            if (!layerName.isEmpty())
                set.insert(layerName);
        }
        layerNames = QStringList(set.begin(), set.end());
        layerNames.sort();

        QMenu *layersMenu = menu.addMenu(QLatin1String("Default Layer"));
        layerActions += layersMenu->addAction(tr("<None>"));
        foreach (QString layerName, layerNames)
            layerActions += layersMenu->addAction(layerName);
    }
#endif

    menu.addSeparator();
    QAction *toggleGrid = menu.addAction(tr("Show &Grid"));
    toggleGrid->setCheckable(true);
    toggleGrid->setChecked(mDrawGrid);

    Preferences *prefs = Preferences::instance();
    connect(toggleGrid, &QAction::toggled,
            prefs, &Preferences::setShowTilesetGrid);

#ifdef ZOMBOID
    QAction *action = menu.exec(event->globalPos());

    if (action && action == actionProperties) {
       // Select this tile to make sure it is clear that only the properties
        // of a single tile are being edited.
        selectionModel()->setCurrentIndex(index,
                                          QItemSelectionModel::SelectCurrent |
                                          QItemSelectionModel::Clear);
        editTileProperties();
    }

    else if (action && layerActions.contains(action)) {
        int index = layerActions.indexOf(action);
        QString layerName = index ? layerNames[index - 1] : QString();
        QModelIndexList indexes = selectionModel()->selectedIndexes();
        QMap<Tile*,QString> changed;
        foreach (QModelIndex index, indexes) {
            tile = m->tileAt(index);
            QString oldName = TilesetManager::instance()->layerName(tile);
            if (oldName != layerName)
                changed[tile] = oldName;
        }
        if (!changed.size())
            return;
        QUndoStack *undoStack = mMapDocument->undoStack();
        undoStack->beginMacro(tr("Change Tile Layer Name (x%n)", "", changed.size()));
        foreach (Tile *tile, changed.keys()) {
            ChangeTileLayerName *undo =
                    new ChangeTileLayerName(mMapDocument, tile,
                                            changed[tile], layerName);
            mMapDocument->undoStack()->push(undo);
        }
        undoStack->endMacro();
    }
#else
    menu.exec(event->globalPos());
#endif
#endif // WORLDED
}

#ifndef WORLDED
void TilesetView::editTileProperties()
{
    const TilesetModel *m = tilesetModel();
    Tile *tile = m->tileAt(selectionModel()->currentIndex());
    if (!tile)
        return;

    PropertiesDialog propertiesDialog(tr("Tile"),
                                      tile,
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void TilesetView::setDrawGrid(bool drawGrid)
{
    mDrawGrid = drawGrid;
#ifdef ZOMBOID
    tilesetModel()->redisplay();
#else
    tilesetModel()->tilesetChanged();
#endif
}
#endif // WORLDED

void TilesetView::adjustScale()
{
#ifdef ZOMBOID
    tilesetModel()->redisplay();
#else
    tilesetModel()->tilesetChanged();
#endif
}

#ifdef ZOMBOID
void TilesetView::autoSwitchLayerChanged(bool enabled)
{
    mShowLayerNames = enabled;
    tilesetModel()->redisplay();
}
#endif
