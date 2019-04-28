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

#include "bmptotmx.h"
#include "mapimage.h"
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
    , mPreviewMapImage(0)
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

    connect(button, &QAbstractButton::clicked, this, &MapsDock::browse);

    Preferences *prefs = Preferences::instance();
    connect(prefs, &Preferences::mapsDirectoryChanged, this, &MapsDock::onMapsDirectoryChanged);
    edit->setText(QDir::toNativeSeparators(prefs->mapsDirectory()));
    connect(edit, &QLineEdit::returnPressed, this, &MapsDock::editedMapsDirectory);

    connect(mMapsView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MapsDock::selectionChanged);

    connect(MapImageManager::instancePtr(), &MapImageManager::mapImageChanged,
            this, &MapsDock::onMapImageChanged);
    connect(MapImageManager::instancePtr(), &MapImageManager::mapImageFailedToLoad,
            this, &MapsDock::mapImageFailedToLoad);

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, &QDockWidget::visibilityChanged,
            mMapsView, &QWidget::setVisible);
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
    mDirectoryEdit->setText(QDir::toNativeSeparators(prefs->mapsDirectory()));
}

void MapsDock::selectionChanged()
{
    QModelIndexList selectedRows = mMapsView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        mPreviewLabel->setPixmap(QPixmap());
        mPreviewMapImage = nullptr;
        return;
    }
    QModelIndex index = selectedRows.first();
    QString path = mMapsView->model()->filePath(index);
    QFileInfo info(path);
    if (info.isDir())
        return;
    if (info.suffix() == QLatin1String("pzw"))
        return;
    MapImage *mapImage = MapImageManager::instance().getMapImage(path);
    if (mapImage) {
        if (mapImage->isReady()) {
            QImage image = mapImage->image().scaled(256, 123, Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation);
            mPreviewLabel->setPixmap(QPixmap::fromImage(image));
        }
    } else
        mPreviewLabel->setPixmap(QPixmap());
    mPreviewMapImage = mapImage;
}

void MapsDock::onMapImageChanged(MapImage *mapImage)
{
    if ((mapImage == mPreviewMapImage) && mapImage->isReady()) {
        QImage image = mapImage->image().scaled(256, 123, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);
        mPreviewLabel->setPixmap(QPixmap::fromImage(image));
    }
}

void MapsDock::mapImageFailedToLoad(MapImage *mapImage)
{
    if (mapImage == mPreviewMapImage) {
        mPreviewLabel->setPixmap(QPixmap());
    }
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
    connect(prefs, &Preferences::mapsDirectoryChanged, this, &MapsView::onMapsDirectoryChanged);

    QDir mapsDir(prefs->mapsDirectory());
    if (!mapsDir.exists())
        mapsDir.setPath(QDir::currentPath());

    QFileSystemModel *model = mFSModel = new QFileSystemModel;
    model->setRootPath(mapsDir.absolutePath());

    model->setFilter(QDir::AllDirs | QDir::NoDot | QDir::Files);
    QStringList filters;
    filters << QLatin1String("*.tmx")
            << QLatin1String("*.tbx")
            << QLatin1String("*.pzw");
    foreach (QString format, BMPToTMX::supportedImageFormats())
        filters << QLatin1String("*.") + format;
    model->setNameFilters(filters);
    model->setNameFilterDisables(false); // hide filtered files

    setModel(model);

    QHeaderView* hHeader = header();
    hHeader->hideSection(1); // Size
    hHeader->hideSection(2);
    hHeader->hideSection(3);

    setRootIndex(model->index(mapsDir.absolutePath()));
    
    header()->setStretchLastSection(false);
#if QT_VERSION >= 0x050000
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
#else
    header()->setResizeMode(0, QHeaderView::Stretch);
    header()->setResizeMode(1, QHeaderView::ResizeToContents);
#endif

    connect(this, &QAbstractItemView::activated, this, &MapsView::onActivated);
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
    if (fileInfo.suffix() == QLatin1String("pzw"))
        MainWindow::instance()->openFile(fileInfo.canonicalFilePath());
}
