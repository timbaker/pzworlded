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

#include "mapsdock.h"

#include "mapimagemanager.h"
#include "mainwindow.h"
#include "preferences.h"

#include <QBoxLayout>
#include <QCompleter>
#include <QDirModel>
#include <QEvent>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QToolButton>

MapsDock::MapsDock(QWidget *parent)
    : QDockWidget(parent)
    , mPreviewLabel(new QLabel(this))
    , mMapsView(new MapsView(this))
{
    setObjectName(QLatin1String("MapsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(2);

    mPreviewLabel->setFrameShape(QFrame::StyledPanel);
    mPreviewLabel->setFrameShadow(QFrame::Plain);
    mPreviewLabel->setMinimumHeight(128);
    mPreviewLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *dirLayout = new QHBoxLayout;
    QLabel *label = new QLabel(tr("Folder:"));

    // QDirModel is obsolete, but I could not get QFileSystemModel to work here
    QLineEdit *edit = mDirectoryEdit = new QLineEdit();
    QDirModel *model = new QDirModel(this);
    model->setFilter(QDir::AllDirs | QDir::Dirs | QDir::Drives | QDir::NoDotAndDotDot);
    QCompleter *completer = new QCompleter(model, this);
    edit->setCompleter(completer);

    QToolButton *button = new QToolButton();
    button->setText(QLatin1String("..."));
//    button->setIcon(QIcon(QLatin1String(":/images/16x16/document-properties.png")));
    button->setToolTip(tr("Choose Folder"));
    dirLayout->addWidget(label);
    dirLayout->addWidget(edit);
    dirLayout->addWidget(button);

    layout->addWidget(mMapsView);
    layout->addWidget(mPreviewLabel);
    layout->addLayout(dirLayout);

    setWidget(widget);
    retranslateUi();

    connect(button, SIGNAL(clicked()), this, SLOT(browse()));

    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(mapsDirectoryChanged()), this, SLOT(onMapsDirectoryChanged()));
    edit->setText(prefs->mapsDirectory());
    connect(edit, SIGNAL(returnPressed()), this, SLOT(editedMapsDirectory()));

    connect(mMapsView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mMapsView, SLOT(setVisible(bool)));
}

void MapsDock::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the Maps Folder"),
        mDirectoryEdit->text());
    if (!f.isEmpty()) {
        Preferences *prefs = Preferences::instance();
        prefs->setMapsDirectory(f);
    }
}

void MapsDock::editedMapsDirectory()
{
    Preferences *prefs = Preferences::instance();
    prefs->setMapsDirectory(mDirectoryEdit->text());
}

void MapsDock::onMapsDirectoryChanged()
{
    Preferences *prefs = Preferences::instance();
    mDirectoryEdit->setText(prefs->mapsDirectory());
}

void MapsDock::selectionChanged()
{
    QModelIndexList selectedRows = mMapsView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        mPreviewLabel->setPixmap(QPixmap());
        return;
    }
    QModelIndex index = selectedRows.first();
    QString path = mMapsView->model()->filePath(index);
    if (QFileInfo(path).isDir())
        return;
    MapImage *mapImage = MapImageManager::instance()->getMapImage(path);
    if (mapImage)
        mPreviewLabel->setPixmap(QPixmap::fromImage(mapImage->image().scaled(256, 123, Qt::KeepAspectRatio)));
    else
        mPreviewLabel->setPixmap(QPixmap());
}

void MapsDock::changeEvent(QEvent *e)
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

void MapsDock::retranslateUi()
{
    setWindowTitle(tr("Maps"));
}

///// ///// ///// ///// /////

MapsView::MapsView(QWidget *parent)
    : QTreeView(parent)
{
    setRootIsDecorated(false);
    setHeaderHidden(true);
    setItemsExpandable(false);
    setUniformRowHeights(true);
    setDragEnabled(true);
    setDefaultDropAction(Qt::MoveAction);

    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(mapsDirectoryChanged()), this, SLOT(onMapsDirectoryChanged()));

    QDir mapsDir(prefs->mapsDirectory());
    if (!mapsDir.exists())
        mapsDir.setPath(QDir::currentPath());

    QFileSystemModel *model = mFSModel = new QFileSystemModel;
    model->setRootPath(mapsDir.absolutePath());

    model->setFilter(QDir::AllDirs | QDir::NoDot | QDir::Files);
    model->setNameFilters(QStringList(QLatin1String("*.tmx")));
    model->setNameFilterDisables(false); // hide filtered files

    setModel(model);

    QHeaderView* hHeader = header();
    hHeader->hideSection(1); // Size
    hHeader->hideSection(2);
    hHeader->hideSection(3);

    setRootIndex(model->index(mapsDir.absolutePath()));
    
    header()->setStretchLastSection(false);
    header()->setResizeMode(0, QHeaderView::Stretch);
    header()->setResizeMode(1, QHeaderView::ResizeToContents);

#if 0
    // This mixes folders with files
    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
#endif

    connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize MapsView::sizeHint() const
{
    return QSize(130, 100);
}

void MapsView::mousePressEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (index.isValid()) {
        // Prevent drag-and-drop starting when clicking on an unselected item.
        setDragEnabled(selectionModel()->isSelected(index));

        // Hack: disable dragging folders.
        // FIXME: the correct way to do this would be to override the flags()
        // method of QFileSystemModel.
        if (model()->isDir(index))
            setDragEnabled(false);
    }

    QTreeView::mousePressEvent(event);
}

void MapsView::onMapsDirectoryChanged()
{
    Preferences *prefs = Preferences::instance();
    QDir mapsDir(prefs->mapsDirectory());
    if (!mapsDir.exists())
        mapsDir.setPath(QDir::currentPath());
    model()->setRootPath(mapsDir.canonicalPath());
    setRootIndex(model()->index(mapsDir.absolutePath()));
}

void MapsView::onActivated(const QModelIndex &index)
{
    QString path = model()->filePath(index);
    QFileInfo fileInfo(path);
    if (fileInfo.isDir()) {
        Preferences *prefs = Preferences::instance();
        prefs->setMapsDirectory(fileInfo.canonicalFilePath());
    }
}
