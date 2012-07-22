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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "documentmanager.h"
#include "layersdock.h"
#include "lotsdock.h"
#include "mapcomposite.h"
#include "mapsdock.h"
#include "objectsdock.h"
#include "objecttypesdialog.h"
#include "preferences.h"
#include "preferencesdialog.h"
#include "progress.h"
#include "propertiesdock.h"
#include "propertydefinitionsdialog.h"
#include "scenetools.h"
#include "templatesdialog.h"
#include "toolmanager.h"
#include "undodock.h"
#include "world.h"
#include "worlddocument.h"
#include "worldreader.h"
#include "worldscene.h"
#include "worldview.h"
#include "zoomable.h"

#include "layer.h"
#include "tileset.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QUndoGroup>
#include <QUndoStack>

// FIXME: add TilesetManager?
Tiled::TilesetImageCache *gTilesetImageCache = NULL;

using namespace Tiled;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mLayersDock(new LayersDock(this))
    , mLotsDock(new LotsDock(this))
    , mMapsDock(new MapsDock(this))
    , mObjectsDock(new ObjectsDock(this))
    , mPropertiesDock(new PropertiesDock(this))
    , mCurrentDocument(0)
    , mCurrentLevelMenu(new QMenu(this))
    , mZoomable(0)
{
    ui->setupUi(this);

    Preferences *prefs = Preferences::instance();

    setStatusBar(0);
    mZoomComboBox = ui->zoomComboBox;

    QString coordString = QString(QLatin1String("%1,%2")).arg(300).arg(300);
    int width = ui->coordinatesLabel->fontMetrics().width(coordString);
    ui->coordinatesLabel->setMinimumWidth(width + 8);

    ui->actionSave->setShortcuts(QKeySequence::Save);
    ui->actionSaveAs->setShortcuts(QKeySequence::SaveAs);
    ui->actionClose->setShortcuts(QKeySequence::Close);
    ui->actionQuit->setShortcuts(QKeySequence::Quit);

    ui->actionSnapToGrid->setChecked(prefs->snapToGrid());
    ui->actionShowCoordinates->setChecked(prefs->showCoordinates());
    ui->actionShowGrid->setChecked(prefs->showWorldGrid());
    ui->actionHighlightCurrentLevel->setChecked(prefs->highlightCurrentLevel());

    // Make sure Ctrl+= also works for zooming in
    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
//    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    ui->actionZoomIn->setShortcuts(keys);
    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);

    QUndoGroup *undoGroup = docman()->undoGroup();
    QAction *undoAction = undoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = undoGroup->createRedoAction(this, tr("Redo"));
    redoAction->setPriority(QAction::LowPriority);
    undoAction->setIconText(tr("Undo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setIconText(tr("Redo"));
    redoAction->setShortcuts(QKeySequence::Redo);
//    connect(undoGroup, SIGNAL(cleanChanged(bool)), SLOT(updateWindowTitle()));
    QAction *separator = ui->editMenu->actions().first();
    ui->editMenu->insertAction(separator, undoAction);
    ui->editMenu->insertAction(separator, redoAction);

    ui->mainToolBar->addAction(undoAction);
    ui->mainToolBar->addAction(redoAction);
    ui->mainToolBar->addSeparator();

    QIcon newIcon = ui->actionNew->icon();
    QIcon openIcon = ui->actionOpen->icon();
    QIcon saveIcon = ui->actionSave->icon();
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    newIcon.addFile(QLatin1String(":images/24x24/document-new.png"));
    openIcon.addFile(QLatin1String(":images/24x24/document-open.png"));
    saveIcon.addFile(QLatin1String(":images/24x24/document-save.png"));
    redoIcon.addFile(QLatin1String(":images/24x24/edit-redo.png"));
    undoIcon.addFile(QLatin1String(":images/24x24/edit-undo.png"));

    ui->actionNew->setIcon(newIcon);
    ui->actionOpen->setIcon(openIcon);
    ui->actionSave->setIcon(saveIcon);
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);

    mUndoDock = new UndoDock(undoGroup, this);

    ui->menuView->addAction(mLayersDock->toggleViewAction());
    ui->menuView->addAction(mLotsDock->toggleViewAction());
    ui->menuView->addAction(mMapsDock->toggleViewAction());
    ui->menuView->addAction(mObjectsDock->toggleViewAction());
    ui->menuView->addAction(mPropertiesDock->toggleViewAction());

    addDockWidget(Qt::LeftDockWidgetArea, mLotsDock);
    addDockWidget(Qt::LeftDockWidgetArea, mObjectsDock);
    addDockWidget(Qt::RightDockWidgetArea, mPropertiesDock);
    addDockWidget(Qt::RightDockWidgetArea, mLayersDock);
    addDockWidget(Qt::RightDockWidgetArea, mMapsDock);
    tabifyDockWidget(mPropertiesDock, mLayersDock);
    tabifyDockWidget(mLayersDock, mMapsDock);

    addDockWidget(Qt::RightDockWidgetArea, mUndoDock);

    connect(ui->actionNew, SIGNAL(triggered()), SLOT(newWorld()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(openFile()));
    connect(ui->actionEditCell, SIGNAL(triggered()), SLOT(editCell()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(saveFile()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(saveFileAs()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(closeFile()));
    connect(ui->actionCloseAll, SIGNAL(triggered()), SLOT(closeAllFiles()));
    connect(ui->actionQuit, SIGNAL(triggered()), SLOT(close()));

    connect(ui->actionCopyCells, SIGNAL(triggered()), SLOT(copyCellsToClipboard()));
    connect(ui->actionPasteCells, SIGNAL(triggered()), SLOT(pasteCellsFromClipboard()));
    connect(ui->actionPreferences, SIGNAL(triggered()), SLOT(preferencesDialog()));

    connect(ui->actionObjectTypes, SIGNAL(triggered()), SLOT(objectTypesDialog()));
    connect(ui->actionProperties, SIGNAL(triggered()), SLOT(properyDefinitionsDialog()));
    connect(ui->actionTemplates, SIGNAL(triggered()), SLOT(templatesDialog()));

    connect(ui->actionRemoveLot, SIGNAL(triggered()), SLOT(removeLot()));
    connect(ui->actionRemoveObject, SIGNAL(triggered()), SLOT(removeObject()));
    connect(ui->actionClearCell, SIGNAL(triggered()), SLOT(clearCells()));

    connect(ui->actionSnapToGrid, SIGNAL(toggled(bool)), prefs, SLOT(setSnapToGrid(bool)));
    connect(ui->actionShowCoordinates, SIGNAL(toggled(bool)), prefs, SLOT(setShowCoordinates(bool)));
    connect(ui->actionShowGrid, SIGNAL(toggled(bool)), SLOT(setShowGrid(bool)));
    connect(ui->actionHighlightCurrentLevel, SIGNAL(toggled(bool)), prefs, SLOT(setHighlightCurrentLevel(bool)));
    connect(ui->actionZoomIn, SIGNAL(triggered()), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()), SLOT(zoomOut()));
    connect(ui->actionZoomNormal, SIGNAL(triggered()), SLOT(zoomNormal()));

    connect(ui->actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    connect(docman(), SIGNAL(documentAdded(Document*)), SLOT(documentAdded(Document*)));
    connect(docman(), SIGNAL(documentAboutToClose(int,Document*)), SLOT(documentAboutToClose(int,Document*)));
    connect(docman(), SIGNAL(currentDocumentChanged(Document*)), SLOT(currentDocumentChanged(Document*)));

    ToolManager *toolManager = ToolManager::instance();
    toolManager->registerTool(WorldCellTool::instance());
    toolManager->registerTool(PasteCellsTool::instance());
    toolManager->addSeparator();
    toolManager->registerTool(SubMapTool::instance());
    toolManager->registerTool(ObjectTool::instance());
    toolManager->registerTool(CreateObjectTool::instance());
    addToolBar(toolManager->toolBar());

#if 0
    QFont font = mCurrentLevelMenu->font();
    font.setPointSize(font.pointSize() - 1);
    mCurrentLevelMenu->setFont(font);
#endif
    ui->currentLevelButton->setMenu(mCurrentLevelMenu);
//    ui->currentLevelButton->setPopupMode(QToolButton::InstantPopup);
    connect(mCurrentLevelMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowCurrentLevelMenu()));
    connect(mCurrentLevelMenu, SIGNAL(triggered(QAction*)), SLOT(currentLevelMenuTriggered(QAction*)));

    ui->documentTabWidget->clear(); // TODO: remove tabs from .ui
    ui->documentTabWidget->setDocumentMode(true);
    ui->documentTabWidget->setTabsClosable(true);

    connect(ui->documentTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentDocumentTabChanged(int)));
    connect(ui->documentTabWidget, SIGNAL(tabCloseRequested(int)),
            SLOT(documentCloseRequested(int)));

    gTilesetImageCache = new Tiled::TilesetImageCache();

    Progress::instance()->setMainWindow(this);

    mViewHint.valid = false;

    updateActions();

    readSettings();
}

MainWindow::~MainWindow()
{
    DocumentManager::deleteInstance();
    ToolManager::deleteInstance();
    Preferences::instance()->deleteInstance();
    delete gTilesetImageCache;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();

    if (confirmAllSave())
        event->accept();
    else
        event->ignore();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        retranslateUi();
        break;
    default:
        break;
    }
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("PZWorldEd"));
#if 0
    updateWindowTitle();

    mRandomButton->setToolTip(tr("Random Mode"));
    mLayerMenu->setTitle(tr("&Layer"));
    mActionHandler->retranslateUi();
#endif
}

void MainWindow::newWorld()
{
    // TODO: Let user specify the width/height of the new world

    World *newWorld = new World(100, 100);
    WorldDocument *newDoc = new WorldDocument(newWorld);
    docman()->addDocument(newDoc);
#if 0
    WorldDocument *doc = new WorldDocument(this);

    doc->world()->cellAt(50, 50)->setMapFilePath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/maptools/suburbs1.tmx"));
    doc->world()->cellAt(49, 48)->setMapFilePath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/maptools/Lot_Farmland.tmx"));
    doc->world()->cellAt(50, 47)->setMapFilePath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/maptools/roadNS_02.tmx"));
    doc->world()->cellAt(50, 48)->setMapFilePath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/maptools/roadNS_03.tmx"));
    doc->world()->cellAt(50, 49)->setMapFilePath(QLatin1String("C:/Users/Tim/Desktop/ProjectZomboid/maptools/roadNS.tmx"));

    {
        WorldCell *cell = doc->world()->cellAt(50, 49);
        cell->addLot(QLatin1String("Lot_Rural_Copse_00"), 50, 100, 0);
        cell->addLot(QLatin1String("Lot_Rural_TreeCluster_00"), 70, 120, 0);
        cell->addLot(QLatin1String("Lot_Rural_TreeCluster_00"), 80, 200, 0);
        cell->addLot(QLatin1String("Lot_Rural_Farmhouse_00"), 80, 70, 0);
        cell->addLot(QLatin1String("Lot_Rural_FieldCrop_00"), 77, 44, 0);
    }
    {
        WorldCell *cell = doc->world()->cellAt(50, 48);
        cell->addLot(QLatin1String("Lot_Rural_Copse_00"), 70, 200, 0);
        cell->addLot(QLatin1String("Lot_Rural_TreeCluster_00"), 70, 150, 0);
        cell->addLot(QLatin1String("Lot_Rural_TreeCluster_00"), 220, 100, 0);
        cell->addLot(QLatin1String("Lot_Rural_Farmhouse_00"), 230, 160, 0);
    }

    docman()->addDocument(doc);

#if 0
    CellDocument *doc2 = new CellDocument(doc, doc->world()->cellAt(50, 50));
    docman()->addDocument(doc2);
#endif

#endif
}

void MainWindow::editCell()
{
    Document *doc = docman()->currentDocument();
    if (WorldDocument *worldDoc = doc->asWorldDocument()) {
        if (worldDoc->selectedCellCount()) {
            WorldCell *cell = worldDoc->selectedCells().first();
            worldDoc->editCell(cell);
        }
    }
}

void MainWindow::setShowGrid(bool show)
{
    if (mCurrentDocument && mCurrentDocument->isWorldDocument())
        Preferences::instance()->setShowWorldGrid(show);
    else if (mCurrentDocument && mCurrentDocument->isCellDocument())
        Preferences::instance()->setShowCellGrid(show);
}

void MainWindow::documentAdded(Document *doc)
{
    if (CellDocument *cellDoc = doc->asCellDocument()) {
        CellView *view = new CellView(this);
        CellScene *scene = new CellScene(view);
        view->setScene(scene);
        scene->setDocument(cellDoc);
        // Handle failure to load the map.
        // Currently this never happens as a placeholder map
        // will be used in case of failure.
        if (scene->mapComposite() == 0) {
            delete scene;
            delete view;
            docman()->setFailedToAdd();
            return;
        }
        doc->setView(view);
        cellDoc->setScene(scene);

        ui->documentTabWidget->addTab(view,
            tr("Cell %1,%2").arg(cellDoc->cell()->x()).arg(cellDoc->cell()->y()));

        if (mViewHint.valid) {
            view->zoomable()->setScale(mViewHint.scale);
#if 1
            view->centerOn(mViewHint.scrollX, mViewHint.scrollY);
#else
            view->horizontalScrollBar()->setSliderPosition(mViewHint.scrollX);
            view->verticalScrollBar()->setSliderPosition(mViewHint.scrollY);
#endif
        }
        else
            view->centerOn(scene->sceneRect().center());
    }
    if (WorldDocument *worldDoc = doc->asWorldDocument()) {
        WorldView *view = new WorldView(this);
        WorldScene *scene = new WorldScene(worldDoc, view);
        view->setScene(scene);
        doc->setView(view);

        ui->documentTabWidget->addTab(view, tr("The World"));

        if (mViewHint.valid) {
            view->zoomable()->setScale(mViewHint.scale);
#if 1
            view->centerOn(mViewHint.scrollX, mViewHint.scrollY);
#else
            view->horizontalScrollBar()->setSliderPosition(mViewHint.scrollX);
            view->verticalScrollBar()->setSliderPosition(mViewHint.scrollY);
#endif
        } else
            view->centerOn(scene->cellToPixelCoords(worldDoc->world()->width() / 2.0,
                                                    worldDoc->world()->height() / 2.0));
    }
}

void MainWindow::documentAboutToClose(int index, Document *doc)
{
    // If there was an error adding the document (such as failure to
    // load the map of a cell) then there won't be a tab yet.

    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
    ui->documentTabWidget->removeTab(index);

    // Delete the QGraphicsView (and QGraphicsScene owned by the view)
    delete doc->view();
}

void MainWindow::currentDocumentTabChanged(int tabIndex)
{
    docman()->setCurrentDocument(tabIndex);
}

void MainWindow::currentDocumentChanged(Document *doc)
{
    if (mCurrentDocument) {
        mCurrentDocument->disconnect(this);
        mZoomable->connectToComboBox(0);
        mZoomable->disconnect(this);
        mZoomable = 0;
    }

    mCurrentDocument = doc;

    if (mCurrentDocument) {
        if (CellDocument *cellDoc = doc->asCellDocument()) {
            connect(cellDoc, SIGNAL(currentLevelChanged(int)), SLOT(updateActions()));
            connect(cellDoc, SIGNAL(selectedLotsChanged()), SLOT(updateActions()));
            connect(cellDoc, SIGNAL(selectedObjectsChanged()), SLOT(updateActions()));
            connect(cellDoc->view(), SIGNAL(statusBarCoordinatesChanged(int,int)),
                    SLOT(setStatusBarCoords(int,int)));
        }

        if (WorldDocument *worldDoc = doc->asWorldDocument()) {
            connect(worldDoc, SIGNAL(selectedCellsChanged()), SLOT(updateActions()));
            connect(worldDoc, SIGNAL(selectedLotsChanged()), SLOT(updateActions()));
            connect(worldDoc, SIGNAL(selectedObjectsChanged()), SLOT(updateActions()));
            connect(worldDoc->view(), SIGNAL(statusBarCoordinatesChanged(int,int)),
                    SLOT(setStatusBarCoords(int,int)));
        }

        mLotsDock->setDocument(doc);
        mObjectsDock->setDocument(doc);

        mZoomable = mCurrentDocument->view()->zoomable();
        mZoomable->connectToComboBox(mZoomComboBox);
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(updateZoom()));

        // May be a WorldDocument, that's ok
        CellDocument *cellDoc = mCurrentDocument->asCellDocument();
        mLayersDock->setCellDocument(cellDoc);
    } else {
        mLayersDock->setCellDocument(0);
        mLotsDock->clearDocument();
        mObjectsDock->clearDocument();
    }

    ToolManager::instance()->setScene(doc ? doc->view()->scene() : 0);
    mPropertiesDock->setDocument(doc);

    ui->documentTabWidget->setCurrentIndex(docman()->indexOf(doc));

    updateActions();
}

void MainWindow::documentCloseRequested(int tabIndex)
{
    Document *doc = docman()->documentAt(tabIndex);
    if (doc->isModified()) {
        docman()->setCurrentDocument(tabIndex);
        if (!confirmSave())
            return;
    }
    docman()->closeDocument(tabIndex);
}

void MainWindow::zoomIn()
{
    if (mZoomable)
        mZoomable->zoomIn();
}

void MainWindow::zoomOut()
{
    if (mZoomable)
        mZoomable->zoomOut();
}

void MainWindow::zoomNormal()
{
    if (mZoomable)
        mZoomable->resetZoom();
}

DocumentManager *MainWindow::docman() const
{
    return DocumentManager::instance();
}

void MainWindow::openFile()
{
    QString filter = tr("All Files (*)");
    filter += QLatin1String(";;");

    QString selectedFilter = tr("PZWorldEd world files (*.pzw)");
    filter += selectedFilter;

#if 0
    selectedFilter = mSettings.value(QLatin1String("lastUsedOpenFilter"),
                                     selectedFilter).toString();

    const PluginManager *pm = PluginManager::instance();
    QList<MapReaderInterface*> readers = pm->interfaces<MapReaderInterface>();
    foreach (const MapReaderInterface *reader, readers) {
        foreach (const QString &str, reader->nameFilters()) {
            if (!str.isEmpty()) {
                filter += QLatin1String(";;");
                filter += str;
            }
        }
    }
#endif

    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Open World"),
                                                    QDir::currentPath()/*fileDialogStartLocation()*/,
                                                    filter, &selectedFilter);
    if (fileNames.isEmpty())
        return;

#if 0
    // When a particular filter was selected, use the associated reader
    MapReaderInterface *mapReader = 0;
    foreach (MapReaderInterface *reader, readers) {
        if (reader->nameFilters().contains(selectedFilter))
            mapReader = reader;
    }

    mSettings.setValue(QLatin1String("lastUsedOpenFilter"), selectedFilter);
#endif

    foreach (const QString &fileName, fileNames)
        openFile(fileName/*, mapReader*/);
}

bool MainWindow::openFile(const QString &fileName)
{
    if (fileName.isEmpty())
        return false;

    // Select existing document if this file is already open
    int documentIndex = docman()->findDocument(fileName);
    if (documentIndex != -1) {
        ui->documentTabWidget->setCurrentIndex(documentIndex);
        return true;
    }

#if 0
    TmxMapReader tmxMapReader;

    if (!mapReader && !tmxMapReader.supportsFile(fileName)) {
        // Try to find a plugin that implements support for this format
        const PluginManager *pm = PluginManager::instance();
        QList<MapReaderInterface*> readers =
                pm->interfaces<MapReaderInterface>();

        foreach (MapReaderInterface *reader, readers) {
            if (reader->supportsFile(fileName)) {
                mapReader = reader;
                break;
            }
        }
    }

    if (!mapReader)
        mapReader = &tmxMapReader;
#endif

    QFileInfo fileInfo(fileName);
    PROGRESS progress(tr("Reading %1").arg(fileInfo.fileName()));

    WorldReader reader;
    World *world = reader.readWorld(fileName);
    if (!world) {
        QMessageBox::critical(this, tr("Error Reading World"),
                              reader.errorString());
        return false;
    }

    docman()->addDocument(new WorldDocument(world, fileName));
    if (docman()->failedToAdd())
        return false;
//    setRecentFile(fileName);
    return true;
}

void MainWindow::openLastFiles()
{
    PROGRESS progress(tr("Restoring session"));

    mSettings.beginGroup(QLatin1String("openFiles"));

    int count = mSettings.value(QLatin1String("count")).toInt();

    for (int i = 0; i < count; i++) {
        QString key = QString::number(i); // openFiles/N/...
        if (!mSettings.childGroups().contains(key))
            continue;
        mSettings.beginGroup(key);
        QString path = mSettings.value(QLatin1String("file")).toString();

        // Restore camera to the previous position
        qreal scale = mSettings.value(QLatin1String("scale")).toDouble();
        const int hor = mSettings.value(QLatin1String("scrollX")).toInt();
        const int ver = mSettings.value(QLatin1String("scrollY")).toInt();
        setDocumentViewHint(qMax(scale, 0.0625), hor, ver);

        // This "recent file" could be a world or just a cell.
        // We require that the world be already open when editing a cell.
        if (openFile(path)) {
            if (mSettings.contains(QLatin1String("cellX"))) {
                int cellX = mSettings.value(QLatin1String("cellX")).toInt();
                int cellY = mSettings.value(QLatin1String("cellY")).toInt();
                WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
                if (WorldCell *cell = worldDoc->world()->cellAt(cellX, cellY)) {
                    CellDocument *cellDoc = new CellDocument(worldDoc, cell);
                    docman()->addDocument(cellDoc); // switches document
                    // If the cell map couldn't be loaded, the document
                    // will have been deleted.
                    if (docman()->failedToAdd()) {
                        mSettings.endGroup();
                        continue;
                    }
                    int layerIndex = mSettings.value(QLatin1String("currentLayer")).toInt();
                    if (layerIndex >= 0 && layerIndex < cellDoc->scene()->map()->layerCount())
                        cellDoc->setCurrentLayerIndex(layerIndex);
                } else {
                    mSettings.endGroup();
                    continue;
                }
            }
#if 0
            BaseGraphicsView *view = mCurrentDocument->view();

            // Restore camera to the previous position
            qreal scale = mSettings.value(QLatin1String("scale")).toDouble();
            if (scale > 0)
                view->zoomable()->setScale(scale);

            const int hor = mSettings.value(QLatin1String("scrollX")).toInt();
            const int ver = mSettings.value(QLatin1String("scrollY")).toInt();
            view->horizontalScrollBar()->setSliderPosition(hor);
            view->verticalScrollBar()->setSliderPosition(ver);
#endif
        }
        mSettings.endGroup();
    }
    mViewHint.valid = false;

    int lastActiveDocument =
            mSettings.value(QLatin1String("lastActive"), -1).toInt();
    if (lastActiveDocument >= 0 && lastActiveDocument < docman()->documentCount())
        ui->documentTabWidget->setCurrentIndex(lastActiveDocument);

    mSettings.endGroup();
}

void MainWindow::setStatusBarCoords(int x, int y)
{
    ui->coordinatesLabel->setText(QString(QLatin1String("%1,%2")).arg(x).arg(y));
}

void MainWindow::aboutToShowCurrentLevelMenu()
{
    mCurrentLevelMenu->clear();
    CellDocument *cellDoc = mCurrentDocument->asCellDocument();
    if (!cellDoc)
        return;
    QStringList items;
    foreach (CompositeLayerGroup *layerGroup, cellDoc->scene()->mapComposite()->sortedLayerGroups())
        items.prepend(QString::number(layerGroup->level()));
    foreach (QString item, items) {
        QAction *action = mCurrentLevelMenu->addAction(item);
        if (item.toInt() == cellDoc->currentLevel()) {
            action->setCheckable(true);
            action->setChecked(true);
            action->setEnabled(false);
        }
    }
}

void MainWindow::currentLevelMenuTriggered(QAction *action)
{
    CellDocument *cellDoc = mCurrentDocument->asCellDocument();
    if (!cellDoc)
        return;
    int level = action->text().toInt();
    cellDoc->setCurrentLevel(level);
}

bool MainWindow::saveFile()
{
    if (!mCurrentDocument)
        return false;

    const QString currentFileName = mCurrentDocument->fileName();

    if (currentFileName.endsWith(QLatin1String(".pzw"), Qt::CaseInsensitive))
        return saveFile(currentFileName);
    else
        return saveFileAs();
}

bool MainWindow::saveFileAs()
{
    QString suggestedFileName;
    if (mCurrentDocument && !mCurrentDocument->fileName().isEmpty()) {
        const QFileInfo fileInfo(mCurrentDocument->fileName());
        suggestedFileName = fileInfo.path();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += fileInfo.completeBaseName();
        suggestedFileName += QLatin1String(".pzw");
    } else {
        suggestedFileName = QDir::currentPath();//fileDialogStartLocation();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += tr("untitled.pzw");
    }

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), suggestedFileName,
                                         tr("PZWorldEd world files (*.pzw)"));
    if (!fileName.isEmpty())
        return saveFile(fileName);
    return false;
}

void MainWindow::closeFile()
{
    if (confirmSave())
        docman()->closeCurrentDocument();
}

void MainWindow::closeAllFiles()
{
    if (confirmAllSave())
        docman()->closeAllDocuments();
}

void MainWindow::preferencesDialog()
{
    if (!mCurrentDocument)
        return;
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument())
        worldDoc = cellDoc->worldDocument();
    PreferencesDialog dialog(worldDoc, this);
    dialog.exec();
}

void MainWindow::objectTypesDialog()
{
    if (!mCurrentDocument)
        return;
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument())
        worldDoc = cellDoc->worldDocument();
    ObjectTypesDialog dialog(worldDoc, this);
    dialog.exec();
}

void MainWindow::properyDefinitionsDialog()
{
    if (!mCurrentDocument)
        return;
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument())
        worldDoc = cellDoc->worldDocument();
    PropertyDefinitionsDialog dialog(worldDoc, this);
    dialog.exec();
}

void MainWindow::templatesDialog()
{
    if (!mCurrentDocument)
        return;
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument())
        worldDoc = cellDoc->worldDocument();
    TemplatesDialog dialog(worldDoc, this);
    dialog.exec();
}

void MainWindow::copyCellsToClipboard()
{
    Q_ASSERT(mCurrentDocument && mCurrentDocument->isWorldDocument());
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    worldDoc->copyCellsToClipboard(worldDoc->selectedCells());
    updateActions();
}

void MainWindow::pasteCellsFromClipboard()
{
    Q_ASSERT(mCurrentDocument && mCurrentDocument->isWorldDocument());
    WorldDocument *worldDoc = mCurrentDocument->asWorldDocument();
    ((WorldScene *)worldDoc->view()->scene())->pasteCellsFromClipboard();
}

void MainWindow::removeLot()
{
    Q_ASSERT(mCurrentDocument);
    WorldDocument *worldDoc = 0;
    WorldCell *cell = 0;
    QList<WorldCellLot*> lots;
    if ((worldDoc = mCurrentDocument->asWorldDocument())) {
        Q_ASSERT(worldDoc->selectedCellCount());
        cell = worldDoc->selectedCells().first();
        lots = worldDoc->selectedLots();
    }
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument()) {
        cell = cellDoc->cell();
        lots = cellDoc->selectedLots();
        worldDoc = cellDoc->worldDocument();
    }
    int count = lots.size();
    if (!worldDoc || !cell || !count) // none of these should ever be true
        return;
    QUndoStack *undoStack = worldDoc->undoStack();
    undoStack->beginMacro(tr("Remove %1 Lot%2").arg(count).arg((count > 1) ? QLatin1String("s") : QLatin1String("")));
    foreach (WorldCellLot *lot, lots) {
        int index = cell->lots().indexOf(lot);
        Q_ASSERT(index >= 0);
        worldDoc->removeCellLot(cell, index);
    }
    undoStack->endMacro();
}

void MainWindow::removeObject()
{
    Q_ASSERT(mCurrentDocument);
    WorldDocument *worldDoc = 0;
    WorldCell *cell = 0;
    QList<WorldCellObject*> objects;
    if ((worldDoc = mCurrentDocument->asWorldDocument())) {
        Q_ASSERT(worldDoc->selectedCellCount());
        cell = worldDoc->selectedCells().first();
        objects = worldDoc->selectedObjects();
    }
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument()) {
        cell = cellDoc->cell();
        objects = cellDoc->selectedObjects();
        worldDoc = cellDoc->worldDocument();
    }
    int count = objects.size();
    if (!worldDoc || !cell || !count) // none of these should ever be true
        return;
    QUndoStack *undoStack = worldDoc->undoStack();
    undoStack->beginMacro(tr("Remove %1 Object%2").arg(count).arg(count ? QLatin1String("s") : QLatin1String("")));
    foreach (WorldCellObject *obj, objects) {
        int index = cell->objects().indexOf(obj);
        Q_ASSERT(index >= 0);
        worldDoc->removeCellObject(cell, index);
    }
    undoStack->endMacro();
}

void MainWindow::clearCells()
{
    Q_ASSERT(mCurrentDocument);
    WorldDocument *worldDoc = 0;
    QList<WorldCell*> cells;
    if ((worldDoc = mCurrentDocument->asWorldDocument())) {
        Q_ASSERT(worldDoc->selectedCellCount());
        cells = worldDoc->selectedCells();
    }
    if (CellDocument *cellDoc = mCurrentDocument->asCellDocument()) {
        cells += cellDoc->cell();
        worldDoc = cellDoc->worldDocument();
    }
    int count = cells.size();
    if (!worldDoc || !count) // none of these should ever be true
        return;
    QUndoStack *undoStack = worldDoc->undoStack();
    undoStack->beginMacro(tr("Clear %1 Cell%2").arg(count).arg((count > 1) ? QLatin1String("s") : QLatin1String("")));
    foreach (WorldCell *cell, cells)
        worldDoc->clearCell(cell);
    undoStack->endMacro();
}

bool MainWindow::confirmSave()
{
    if (!mCurrentDocument || !mCurrentDocument->isModified())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return saveFile();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

bool MainWindow::confirmAllSave()
{
    foreach (Document *doc, docman()->documents()) {
        if (!doc->isModified())
            continue;
        docman()->setCurrentDocument(doc);
        if (!confirmSave())
            return false;
    }

    return true;
}

void MainWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("mainwindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.endGroup();

    mSettings.beginGroup(QLatin1String("openFiles"));
    mSettings.remove(QLatin1String(""));
    if (mCurrentDocument)
        mSettings.setValue(QLatin1String("lastActive"),
                           docman()->indexOf(mCurrentDocument));

    mSettings.setValue(QLatin1String("count"), docman()->documentCount());

    int i = 0;
    foreach (Document *doc, docman()->documents()) {
        mSettings.beginGroup(QString::number(i)); // openFiles/N/...
#if 0
        ui->documentTabWidget->setCurrentWidget(doc->view());
//        mDocumentManager->switchToDocument(i);
#endif
        mSettings.setValue(QLatin1String("file"), doc->fileName());
        BaseGraphicsView *view = doc->view();

        mSettings.setValue(QLatin1String("scale"),
                           QString::number(view->zoomable()->scale()));
#if 1
        QPointF centerScenePos = view->mapToScene(view->width() / 2, view->height() / 2);
        mSettings.setValue(QLatin1String("scrollX"),
                           QString::number(int(centerScenePos.x())));
        mSettings.setValue(QLatin1String("scrollY"),
                           QString::number(int(centerScenePos.y())));
#else
        mSettings.setValue(QLatin1String("scrollX"), QString::number(
                       view->horizontalScrollBar()->sliderPosition()));
        mSettings.setValue(QLatin1String("scrollY"), QString::number(
                       view->verticalScrollBar()->sliderPosition()));
#endif
        if (CellDocument *cellDoc = doc->asCellDocument()) {
            mSettings.setValue(QLatin1String("cellX"), QString::number(cellDoc->cell()->x()));
            mSettings.setValue(QLatin1String("cellY"), QString::number(cellDoc->cell()->y()));
            const int currentLayerIndex = cellDoc->currentLayerIndex();
            mSettings.setValue(QLatin1String("currentLayer"), QString::number(currentLayerIndex));
        } else {
        }
        mSettings.endGroup();
        ++i;
    }

    mSettings.endGroup();
}

void MainWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("mainwindow"));
    QByteArray geom = mSettings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(800, 600);
    restoreState(mSettings.value(QLatin1String("state"),
                                 QByteArray()).toByteArray());
    mSettings.endGroup();
//    updateRecentFiles();
}

bool MainWindow::saveFile(const QString &fileName)
{
    if (!mCurrentDocument)
        return false;

    QString error;
    if (!mCurrentDocument->save(fileName, error)) {
        QMessageBox::critical(this, tr("Error Saving Map"), error);
        return false;
    }

//    setRecentFile(fileName);
    return true;
}

void MainWindow::updateActions()
{
    Document *doc = mCurrentDocument;
    bool hasDoc = doc != 0;
    CellDocument *cellDoc = hasDoc ? doc->asCellDocument() : 0;
    WorldDocument *worldDoc = hasDoc ? doc->asWorldDocument() : 0;

    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionClose->setEnabled(hasDoc);
    ui->actionCloseAll->setEnabled(hasDoc);

    ui->actionCopyCells->setEnabled(worldDoc && worldDoc->selectedCellCount());
    ui->actionPasteCells->setEnabled(worldDoc && worldDoc->cellsInClipboardCount());

    ui->actionEditCell->setEnabled(false);
    ui->actionObjectTypes->setEnabled(hasDoc);
    ui->actionProperties->setEnabled(hasDoc);
    ui->actionTemplates->setEnabled(hasDoc);

    bool removeLot = (cellDoc && cellDoc->selectedLotCount())
            || (worldDoc && worldDoc->selectedLotCount());
    ui->actionRemoveLot->setEnabled(removeLot);

    bool removeObject = (cellDoc && cellDoc->selectedObjectCount())
            || (worldDoc && worldDoc->selectedObjectCount());
    ui->actionRemoveObject->setEnabled(removeObject);

    ui->actionClearCell->setEnabled(false);

    ui->actionSnapToGrid->setEnabled(cellDoc != 0);
    ui->actionShowCoordinates->setEnabled(worldDoc != 0);

    Preferences *prefs = Preferences::instance();
    ui->actionShowGrid->setChecked(worldDoc ? prefs->showWorldGrid() : cellDoc ? prefs->showCellGrid() : false);
    ui->actionShowGrid->setEnabled(hasDoc);

    ui->actionHighlightCurrentLevel->setEnabled(cellDoc != 0);

    updateZoom();

    if (worldDoc) {
        WorldCell *cell = worldDoc->selectedCellCount() ? worldDoc->selectedCells().first() : 0;
        if (cell) {
            ui->actionEditCell->setEnabled(true);
            ui->actionClearCell->setEnabled(true);
            ui->currentCellLabel->setText(tr("Current cell: %1,%2").arg(cell->x()).arg(cell->y()));
        } else
            ui->currentCellLabel->setText(tr("Current cell: <none>"));
        ui->currentLevelButton->setText(tr("Current level: ? ")); // extra space cuz of down-arrow placement on Windows
        ui->currentLevelButton->setEnabled(false);
    } else if (cellDoc) {
        ui->actionClearCell->setEnabled(true);
        WorldCell *cell = cellDoc->cell();
        ui->currentCellLabel->setText(tr("Current cell: %1,%2").arg(cell->x()).arg(cell->y()));
        int level = cellDoc->currentLevel();
        ui->currentLevelButton->setText(tr("Current level: %1 ").arg(level)); // extra space cuz of down-arrow placement on Windows
        ui->currentLevelButton->setEnabled(true);
    } else {
        ui->coordinatesLabel->clear();
        ui->currentCellLabel->setText(tr("Current cell: <none>"));
        ui->currentLevelButton->setText(tr("Current level: ? ")); // extra space cuz of down-arrow placement on Windows
        ui->currentLevelButton->setEnabled(false);
    }
}

void MainWindow::updateZoom()
{
    const qreal scale = mZoomable ? mZoomable->scale() : 1;

    ui->actionZoomIn->setEnabled(mZoomable && mZoomable->canZoomIn());
    ui->actionZoomOut->setEnabled(mZoomable && mZoomable->canZoomOut());
    ui->actionZoomNormal->setEnabled(scale != 1);

    if (mZoomable) {
        mZoomComboBox->setEnabled(true);
    } else {
        int index = mZoomComboBox->findData((qreal)1.0);
        mZoomComboBox->setCurrentIndex(index);
        mZoomComboBox->setEnabled(false);
    }
}
