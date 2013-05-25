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

#include "pathlayersdock.h"
#include "ui_pathlayersdock.h"

#include "documentmanager.h"
#include "pathdocument.h"
#include "path.h"
#include "pathlayersmodel.h"
#include "pathworld.h"
#include "worldchanger.h"

#include <QEvent>
#include <QToolBar>
#include <QVBoxLayout>

PathLayersDock::PathLayersDock(QWidget *parent) :
    QDockWidget(parent),
    mDocument(0),
    ui(new Ui::PathLayersDock())
{
    ui->setupUi(this);
    mView = ui->view;

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionNewPathLayer);
    toolBar->addAction(ui->actionRemovePathLayer);
    toolBar->addAction(ui->actionMoveLayerUp);
    toolBar->addAction(ui->actionMoveLayerDown);
    ui->toolbarLayout->addWidget(toolBar);

    retranslateUi();

    connect(ui->actionNewPathLayer, SIGNAL(triggered()), SLOT(newLayer()));
    connect(ui->actionRemovePathLayer, SIGNAL(triggered()), SLOT(removeLayer()));
    connect(ui->actionMoveLayerUp, SIGNAL(triggered()), SLOT(moveUp()));
    connect(ui->actionMoveLayerDown, SIGNAL(triggered()), SLOT(moveDown()));

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));

    connect(mView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateActions()));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,Document*)),
            SLOT(documentAboutToClose(int,Document*)));
}

void PathLayersDock::setDocument(PathDocument *doc)
{
    if (mDocument) {
        saveExpandedLevels(doc);
        mDocument->changer()->disconnect(this);
    }

    mDocument = doc;
    mView->setDocument(doc);

    if (mDocument) {
        restoreExpandedLevels(doc);
        connect(mDocument->changer(), SIGNAL(afterAddPathLayerSignal(WorldLevel*,int,WorldPathLayer*)),
                SLOT(updateActions()));
        connect(mDocument->changer(), SIGNAL(afterRemovePathLayerSignal(WorldLevel*,int,WorldPathLayer*)),
                SLOT(updateActions()));
        connect(mDocument->changer(), SIGNAL(afterReorderPathLayerSignal(WorldLevel*,WorldPathLayer*,int)),
                SLOT(updateActions()));
    }

    updateActions();
}

PathWorld *PathLayersDock::world() const
{
    return mDocument ? mDocument->world() : 0;
}

void PathLayersDock::changeEvent(QEvent *e)
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

void PathLayersDock::newLayer()
{
    WorldPathLayer *layer = world()->allocPathLayer();
    WorldLevel *wlevel = document()->currentLevel();
    int n = 1;
    QString name = tr("PathLayer%1").arg(n);
    while (wlevel->pathLayerByName(name))
        name = tr("PathLayer%1").arg(++n);
    layer->setName(name);
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doAddPathLayer(wlevel, wlevel->pathLayerCount(), layer);
    mDocument->changer()->endUndoCommand();
}

void PathLayersDock::removeLayer()
{
    WorldLevel *wlevel = document()->currentLevel();
    WorldPathLayer *layer = document()->currentPathLayer();
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doRemovePathLayer(wlevel, wlevel->indexOf(layer), layer);
    mDocument->changer()->endUndoCommand();
}

void PathLayersDock::moveUp()
{
    WorldLevel *wlevel = document()->currentLevel();
    WorldPathLayer *layer = document()->currentPathLayer();
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doReorderPathLayer(wlevel, layer, wlevel->indexOf(layer) + 1);
    mDocument->changer()->endUndoCommand();
}

void PathLayersDock::moveDown()
{
    WorldLevel *wlevel = document()->currentLevel();
    WorldPathLayer *layer = document()->currentPathLayer();
    mDocument->changer()->beginUndoCommand(mDocument->undoStack());
    mDocument->changer()->doReorderPathLayer(wlevel, layer, wlevel->indexOf(layer) - 1);
    mDocument->changer()->endUndoCommand();
}

void PathLayersDock::documentAboutToClose(int index, Document *doc)
{
    Q_UNUSED(index)
    if (PathDocument *pathDoc = doc->asPathDocument()) {
        if (mExpandedLevels.contains(pathDoc))
            mExpandedLevels.remove(pathDoc);
    }
}

void PathLayersDock::updateActions()
{
    WorldPathLayer *layer = mDocument ? mDocument->currentPathLayer() : 0;
    int index = layer ? layer->wlevel()->indexOf(layer) : -1;
    ui->actionRemovePathLayer->setEnabled(layer != 0);
    ui->actionMoveLayerUp->setEnabled(layer && (index + 1 < layer->wlevel()->pathLayerCount()));
    ui->actionMoveLayerDown->setEnabled(layer && (index > 0));
}

void PathLayersDock::retranslateUi()
{
    setWindowTitle(tr("Path Layers"));
}

void PathLayersDock::saveExpandedLevels(PathDocument *doc)
{
}

void PathLayersDock::restoreExpandedLevels(PathDocument *doc)
{
}

/////

PathLayersView::PathLayersView(QWidget *parent) :
    QTreeView(parent),
    mDocument(0),
    mSynching(false),
    mModel(new PathLayersModel(this))
{
    setRootIsDecorated(true);
    setHeaderHidden(true);
    setItemsExpandable(true);
    setUniformRowHeights(true);

    setModel(mModel);

    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setSelectionMode(QAbstractItemView::ExtendedSelection);

//    connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize PathLayersView::sizeHint() const
{
    return QSize(130, 100);
}

void PathLayersView::setDocument(PathDocument *doc)
{
    if (mDocument) {
        mDocument->disconnect(this);
    }

    mDocument = doc;
    mModel->setDocument(doc);

    if (mDocument) {
        currentPathLayerChanged(mDocument->currentPathLayer());
        connect(mDocument, SIGNAL(currentPathLayerChanged(WorldPathLayer*)),
                SLOT(currentPathLayerChanged(WorldPathLayer*)));
    }
}

void PathLayersView::selectionChanged(const QItemSelection &selected,
                                      const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);

    if (!mDocument || mSynching)
        return;

    QModelIndexList selectedRows = selectionModel()->selectedRows();
    int count = selectedRows.count();

    mSynching = true;
    if (count == 1) {
        QModelIndex index = selectedRows.first();
        if (WorldPathLayer *layer = model()->toPathLayer(index))
            mDocument->setCurrentPathLayer(layer);
        if (WorldLevel *level = model()->toLevel(index))
            mDocument->setCurrentLevel(level);
    } else if (!count) {
        mDocument->setCurrentPathLayer(0);
    }
    mSynching = false;
}

void PathLayersView::currentPathLayerChanged(WorldPathLayer *layer)
{
    if (mSynching)
        return;

    if (layer) {
        mSynching = true;
        setCurrentIndex(model()->index(layer));
        mSynching = false;
        return;
    }

    mSynching = true;
    setCurrentIndex(QModelIndex());
    mSynching = false;
}
