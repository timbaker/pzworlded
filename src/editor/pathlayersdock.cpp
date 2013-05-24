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

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,Document*)),
            SLOT(documentAboutToClose(int,Document*)));
}

void PathLayersDock::setDocument(PathDocument *doc)
{
    if (mDocument) {
        saveExpandedLevels(doc);
    }

    mDocument = doc;
    mView->setDocument(doc);

    if (mDocument) {
        restoreExpandedLevels(doc);
    }
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
//    mDocument->changer()->doAddPathLayer(world()->pathLayerCount(), layer);
}

void PathLayersDock::removeLayer()
{
}

void PathLayersDock::moveUp()
{
}

void PathLayersDock::moveDown()
{
}

void PathLayersDock::documentAboutToClose(int index, Document *doc)
{
    Q_UNUSED(index)
    if (PathDocument *pathDoc = doc->asPathDocument()) {
        if (mExpandedLevels.contains(pathDoc))
            mExpandedLevels.remove(pathDoc);
    }
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
    }

    mDocument = doc;
    mModel->setDocument(doc);

    if (mDocument) {
    }
}

void PathLayersView::selectionChanged(const QItemSelection &selected,
                                      const QItemSelection &deselected)
{
}
