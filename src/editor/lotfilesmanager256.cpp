/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#include "lotfilesmanager256.h"

#include "exportlotsprogressdialog.h"
#include "generatelotsfailuredialog.h"
#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "progress.h"
#include "tiledeffile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "world.h"
#include "worldcell.h"
#include "worldconstants.h"
#include "worlddocument.h"

#include "InGameMap/clipper.hpp"

#include "navigation/chunkdatafile256.h"
#include "navigation/isogridsquare256.h"

#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>
#include <QRandomGenerator>

using namespace Tiled;

static int VERSION1 = 1; // Added 4-byte 'LOTH' at start of .lotheader files.
                         // Added 4-byte 'LOTP' at start of .lotpack files, followed by 4-byte version number.

static int VERSION_LATEST = VERSION1;

static QString nameOfTileset(const Tileset *tileset)
{
    QString name = tileset->imageSource();
    if (name.contains(QLatin1String("/")))
        name = name.mid(name.lastIndexOf(QLatin1String("/")) + 1);
    name.replace(QLatin1String(".png"), QLatin1String(""));
    return name;
}

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++) {
        if (str[i].toLatin1() == '\n') continue;
        out << quint8(str[i].toLatin1());
    }
    out << quint8('\n');
}

/////

LotFilesManager256 *LotFilesManager256::mInstance = nullptr;

LotFilesManager256 *LotFilesManager256::instance()
{
    if (mInstance == nullptr) {
        mInstance = new LotFilesManager256();
    }
    return mInstance;
}

void LotFilesManager256::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

LotFilesManager256::LotFilesManager256(QObject *parent) :
    QObject(parent),
    mProgressDialog(nullptr)
{
    mJumboZoneList += new JumboZone(QStringLiteral("DeepForest"), 100);
    mJumboZoneList += new JumboZone(QStringLiteral("Farm"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("FarmLand"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("Forest"), 50);
    mJumboZoneList += new JumboZone(QStringLiteral("TownZone"), 80);
    mJumboZoneList += new JumboZone(QStringLiteral("Vegitation"), 10);
}

LotFilesManager256::~LotFilesManager256()
{
//    stopThreads();
}

void LotFilesManager256::startThreads(int numberOfThreads)
{
    stopThreads();

    mWorkerThreads.resize(numberOfThreads);
    mWorkers.resize(numberOfThreads);
    for (int i = 0; i < numberOfThreads; i++) {
        mWorkerThreads[i] = new InterruptibleThread;
        connect(mWorkerThreads[i], &QThread::finished, [this, i]() {
            this->workerThreadFinished(i);
        });
        mWorkers[i] = new LotFilesWorker256(this, mWorkerThreads[i]);
        mWorkers[i]->moveToThread(mWorkerThreads[i]);
        mWorkerThreads[i]->start();
    }

    mThreadsStopped = false;
}

void LotFilesManager256::stopThreads()
{
    for (int i = 0; i < mWorkerThreads.size(); i++) {
        qDebug() << "LotFilesManager256: quitting thread #" << i;
        mWorkerThreads[i]->interrupt();
        mWorkerThreads[i]->quit();
    }
#if 0
    while (true) {
        bool bAnyAlive = false;
        for (int i = 0; i < mWorkerThreads.size(); i++) {
            if (mWorkers[i] != nullptr) {
                qDebug() << "LotFilesManager256: bAnyAlive #" << i;
                bAnyAlive = true;
                break;
            }
        }
        if (bAnyAlive == false) {
            break;
        }
        qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
        QThread::msleep(1000 / 30);
    }
    mWorkerThreads.clear();
    mWorkers.clear();
#endif
}

bool LotFilesManager256::generateWorld(WorldDocument *worldDoc, GenerateMode mode)
{
    mWorldDoc = worldDoc;
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    mCellBounds256 = CombinedCellMaps::toCellRect256(QRect(lotSettings.worldOrigin, QSize(mWorldDoc->world()->size())));

    mDialog = new ExportLotsProgressDialog(MainWindow::instance());
    ExportLotsProgressDialog& progress = *mDialog;
    progress.setModal(true);
    mProgressDialog = &progress;
    connect(mProgressDialog, &ExportLotsProgressDialog::cancelled, this, &LotFilesManager256::cancel);
    progress.show();
    progress.activateWindow();
    progress.raise();
    qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
    progress.setWorldSize(mCellBounds256.width(), mCellBounds256.height());
    progress.setPrompt(QLatin1String("Reading Zombie Spawn Map"));

    mCancel = false;

    QString spawnMap = lotSettings.zombieSpawnMap;
    if (!QFileInfo(spawnMap).exists()) {
        mError = tr("Couldn't find the Zombie Spawn Map image.\n%1")
                .arg(spawnMap);
        delete mDialog;
        mDialog = nullptr;
        return false;
    }
    ZombieSpawnMap = QImage(spawnMap);
    if (ZombieSpawnMap.isNull()) {
        mError = tr("Couldn't read the Zombie Spawn Map image.\n%1")
                .arg(spawnMap);
        delete mDialog;
        mDialog = nullptr;
        return false;
    }

    if (!Navigate::IsoGridSquare256::loadTileDefFiles(lotSettings, mError)) {
        delete mDialog;
        mDialog = nullptr;
        return false;
    }

    QString tilesDirectory = TileMetaInfoMgr::instance()->tilesDirectory();
    if (tilesDirectory.isEmpty() || !QFileInfo(tilesDirectory).exists()) {
        mError = tr("The Tiles Directory could not be found.  Please set it in the Tilesets Dialog in TileZed.");
        delete mDialog;
        mDialog = nullptr;
        return false;
    }
#if 0
    if (!TileMetaInfoMgr::instance()->readTxt()) {
        mError = tr("%1\n(while reading %2)")
                .arg(TileMetaInfoMgr::instance()->errorString())
                .arg(TileMetaInfoMgr::instance()->txtName());
        return false;
    }
#endif

    mStats.reset();

    progress.setPrompt(QLatin1String("Generating .lot files"));

    World *world = worldDoc->world();

    // A single 300x300 cell may overlap 4, 6, or 9 256x256 cells.
    mDoneCells256.clear();
    mCell256Queue.clear();

    mFailures.clear();

    startThreads(lotSettings.numberOfThreads);

    if (mode == GenerateSelected) {
        for (WorldCell *cell : worldDoc->selectedCells()) {
            if (cell->mapFilePath().isEmpty()) {
                continue;
            }
            int cell300X = lotSettings.worldOrigin.x() + cell->x();
            int cell300Y = lotSettings.worldOrigin.y() + cell->y();
            QRect cellBounds256 = CombinedCellMaps::toCellRect256(QRect(cell300X, cell300Y, 1, 1));
            for (int cell256Y = cellBounds256.top(); cell256Y <= cellBounds256.bottom(); cell256Y++) {
                for (int cell256X = cellBounds256.left(); cell256X <= cellBounds256.right(); cell256X++) {
                    mProgressDialog->setCellStatus(cell256X - mCellBounds256.left(), cell256Y - mCellBounds256.top(), ExportLotsProgressDialog::CellStatus::Pending);
                }
            }
        }
        for (WorldCell *cell : worldDoc->selectedCells()) {
            if (mCancel) {
                break;
            }
            if (!generateCell(cell)) {
                if (mError.isEmpty() == false) { // mError is empty when cancelling
                    mFailures += GenerateCellFailure(cell, mError);
                }
//                return false;
            }
        }
    } else {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell* cell = world->cellAt(x, y);
                if (cell->mapFilePath().isEmpty()) {
                    continue;
                }
                QRect cellBounds256 = CombinedCellMaps::toCellRect256(QRect(lotSettings.worldOrigin.x() + x, lotSettings.worldOrigin.y() + y, 1, 1));
                for (int cell256Y = cellBounds256.top(); cell256Y <= cellBounds256.bottom(); cell256Y++) {
                    for (int cell256X = cellBounds256.left(); cell256X <= cellBounds256.right(); cell256X++) {
                        mProgressDialog->setCellStatus(cell256X - mCellBounds256.x(), cell256Y - mCellBounds256.y(), ExportLotsProgressDialog::CellStatus::Pending);
                    }
                }
            }
        }
        for (int y = 0; y < world->height(); y++) {
            if (mCancel) {
                break;
            }
            for (int x = 0; x < world->width(); x++) {
                WorldCell* cell = world->cellAt(x, y);
                if (mCancel) {
                    break;
                }
                if (!generateCell(cell)) {
                    if (mError.isEmpty() == false) { // mError is empty when cancelling
                        mFailures += GenerateCellFailure(cell, mError);
                    }
//                    return false;
                }
            }
        }
    }
#if 1
    // The application should continue normally with the modal dialog box visible until everything finishes.
    mTimer.setInterval(1000 / 30);
    connect(&mTimer, &QTimer::timeout, this, &LotFilesManager256::workTimerTimeout);
    mTimer.start();
#else
    while (true) {
        qDebug() << "LotFilesManager256: Waiting for workers to finish";
        updateWorkers();
        if (getBusyWorker() == nullptr) {
            break;
        }
        qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
        Sleep::msleep(1000 / 30);
    }

    qDebug() << "LotFilesManager256: Stopping threads";
    stopThreads();

    qDebug() << "LotFilesManager256: Stopped threads";
    qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
    progress.setVisible(false);

    if (!mFailures.isEmpty()) {
        QStringList errorList;
        for (GenerateCellFailure failure : mFailures) {
            errorList += QString(QStringLiteral("Cell %1,%2: %3")).arg(failure.cell->x()).arg(failure.cell->y()).arg(failure.error);
        }
        GenerateLotsFailureDialog dialog(errorList, MainWindow::instance());
        dialog.exec();
    }

    QString stats = tr("Finished!\n\nBuildings: %1\nRooms: %2\nRoom rects: %3\nRoom objects: %4")
            .arg(mStats.numBuildings)
            .arg(mStats.numRooms)
            .arg(mStats.numRoomRects)
            .arg(mStats.numRoomObjects);
    QMessageBox::information(MainWindow::instance(),
                             tr("Generate Lot Files"), stats);
#endif
    return true;
}

bool LotFilesManager256::generateCell(WorldCell *cell)
{
    if (cell == nullptr)
        return true;

    if (cell->mapFilePath().isEmpty())
        return true;

    if ((cell->x() + 1) * CHUNKS_PER_CELL > ZombieSpawnMap.width() ||
            (cell->y() + 1) * CHUNKS_PER_CELL > ZombieSpawnMap.height()) {
        mError = tr("The Zombie Spawn Map doesn't cover cell %1,%2.")
                .arg(cell->x()).arg(cell->y());
        return false;
    }

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    int cell300X = lotSettings.worldOrigin.x() + cell->x();
    int cell300Y = lotSettings.worldOrigin.y() + cell->y();
    QRect cellBounds256 = CombinedCellMaps::toCellRect256(QRect(cell300X, cell300Y, 1, 1));
#if 1
    for (int cell256Y = cellBounds256.top(); cell256Y <= cellBounds256.bottom(); cell256Y++) {
        for (int cell256X = cellBounds256.left(); cell256X <= cellBounds256.right(); cell256X++) {
            CellJob cellJob;
            cellJob.cell = cell;
            cellJob.cell256X = cell256X;
            cellJob.cell256Y = cell256Y;
            mCell256Queue += cellJob;
        }
    }
#else
    for (int cell256Y = cellBounds256.top(); cell256Y <= cellBounds256.bottom(); cell256Y++) {
        if (mCancel) {
            break;
        }
        for (int cell256X = cellBounds256.left(); cell256X <= cellBounds256.right(); cell256X++) {
            if (mCancel) {
                break;
            }
            QPair<int, int> doneCell(cell256X, cell256Y);
            if (mDoneCells256.contains(doneCell)) {
                continue;
            }
            mDoneCells256.insert(doneCell);
            if (generateCell(cell, cell256X, cell256Y) == false) {
                mProgressDialog->setCellStatus(cell256X - mCellBounds256.left(), cell256Y - mCellBounds256.top(), ExportLotsProgressDialog::CellStatus::Failed);
                return false;
            }
        }
    }
#endif
    return true;
}

void LotFilesManager256::updateWorkers()
{
    if (mCancel) {
        mCell256Queue.clear();
    }
    for (int i = 0; i < mWorkers.size(); i++) {
        LotFilesWorker256 *worker = mWorkers[i];
        if (worker == nullptr) {
            continue; // thread exited
        }
        if (worker->mStatus == LotFilesWorker256::Status::Idle) {
            if (mCell256Queue.isEmpty()) {
                // if all workers are idle, we are finished
            } else {
                CellJob cellJob = mCell256Queue.front();
                mCell256Queue.pop_front();
                QPair<int, int> doneCell(cellJob.cell256X, cellJob.cell256Y);
                if (mDoneCells256.contains(doneCell)) {
                    continue;
                }
                mDoneCells256.insert(doneCell);
                generateCell(worker, cellJob.cell, cellJob.cell256X, cellJob.cell256Y);
            }
        }
        if (worker->mStatus == LotFilesWorker256::Status::LoadingMaps) {
            int loadStatus = worker->mCombinedCellMaps->checkLoading(mWorldDoc);
            if (loadStatus == -1) {
                mFailures += GenerateCellFailure(worker->mCell, worker->mError);
                mProgressDialog->setCellStatus(worker->mCombinedCellMaps->mCell256X - mCellBounds256.left(),
                                               worker->mCombinedCellMaps->mCell256Y - mCellBounds256.top(),
                                               ExportLotsProgressDialog::CellStatus::Failed);
                delete worker->mCombinedCellMaps;
                worker->mCombinedCellMaps = nullptr;
                worker->mStatus = LotFilesWorker256::Status::Idle;
            }
            if (loadStatus == 1) {
                if (mCancel) {
                    delete worker->mCombinedCellMaps;
                    worker->mCombinedCellMaps = nullptr;
                    worker->mStatus = LotFilesWorker256::Status::Idle;
                    continue;
                }
                qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents); // handle any pending signal-to-slot before moving threads
                worker->mCombinedCellMaps->moveToThread(worker->mCombinedCellMaps->mMapComposite, mWorkerThreads[i]);
                worker->mStatus = LotFilesWorker256::Status::Working;
                QMetaObject::invokeMethod(worker, "addJob", Qt::QueuedConnection);
            }
        }
        if (worker->mStatus == LotFilesWorker256::Status::Error) {
            mFailures += GenerateCellFailure(worker->mCell, worker->mError);
            mProgressDialog->setCellStatus(worker->mCombinedCellMaps->mCell256X - mCellBounds256.left(),
                                           worker->mCombinedCellMaps->mCell256Y - mCellBounds256.top(),
                                           ExportLotsProgressDialog::CellStatus::Failed);
            delete worker->mCombinedCellMaps;
            worker->mCombinedCellMaps = nullptr;
            worker->mStatus = LotFilesWorker256::Status::Idle;
        }
        if (worker->mStatus == LotFilesWorker256::Status::Finished) {
            mProgressDialog->setCellStatus(worker->mCombinedCellMaps->mCell256X - mCellBounds256.left(),
                                           worker->mCombinedCellMaps->mCell256Y - mCellBounds256.top(),
                                           ExportLotsProgressDialog::CellStatus::Exported);
            delete worker->mCombinedCellMaps;
            worker->mCombinedCellMaps = nullptr;
            worker->mStatus = LotFilesWorker256::Status::Idle;
            mStats.combine(worker->mStats);
        }
    }

    if (mCell256Queue.isEmpty() == false) {
        return;
    }

    if (getBusyWorker() != nullptr) {
        return;
    }

    if (mThreadsStopped == false) {
        qDebug() << "LotFilesManager256: Stopping threads";
        stopThreads();
        mThreadsStopped = true;
        return;
    }

    bool bThreadsRunning = false;
    for (int i = 0; i < mWorkers.size(); i++) {
        LotFilesWorker256 *worker = mWorkers[i];
        if (worker != nullptr) {
            bThreadsRunning = true;
            break;
        }
    }
    if (bThreadsRunning) {
        return;
    }

    mWorkers.clear();
    mWorkerThreads.clear();

    mTimer.stop();
    mTimer.disconnect(this);

    qDebug() << "LotFilesManager256: Stopped threads";
    qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
    mDialog->setVisible(false);

    if (!mFailures.isEmpty()) {
        QStringList errorList;
        for (GenerateCellFailure failure : mFailures) {
            errorList += QString(QStringLiteral("Cell %1,%2: %3")).arg(failure.cell->x()).arg(failure.cell->y()).arg(failure.error);
        }
        GenerateLotsFailureDialog dialog(errorList, MainWindow::instance());
        dialog.exec();
    }

    QString stats = tr("Finished!\n\nBuildings: %1\nRooms: %2\nRoom rects: %3\nRoom objects: %4")
            .arg(mStats.numBuildings)
            .arg(mStats.numRooms)
            .arg(mStats.numRoomRects)
            .arg(mStats.numRoomObjects);
    QMessageBox::information(MainWindow::instance(),
                             tr("Generate Lot Files"), stats);
}

LotFilesWorker256 *LotFilesManager256::getFirstWorkerWithStatus(LotFilesWorker256::Status status)
{
    for (int i = 0; i < mWorkers.size(); i++) {
        LotFilesWorker256 *worker = mWorkers[i];
        if (worker == nullptr) {
            continue; // thread exited
        }
        if (worker->mStatus == status) {
            return worker;
        }
    }
    return nullptr;
}

LotFilesWorker256 *LotFilesManager256::getIdleWorker()
{
    return getFirstWorkerWithStatus(LotFilesWorker256::Status::Idle);
}

LotFilesWorker256 *LotFilesManager256::getBusyWorker()
{
    for (int i = 0; i < mWorkers.size(); i++) {
        LotFilesWorker256 *worker = mWorkers[i];
        if (worker == nullptr) {
            continue; // thread exited
        }
        if (worker->mStatus != LotFilesWorker256::Status::Idle) {
            return worker;
        }
    }
    return nullptr;
}

bool LotFilesManager256::generateCell(LotFilesWorker256 *worker, WorldCell *cell, int cell256X, int cell256Y)
{
#if 1
#else
    // Busy-wait until a worker is available.
    LotFilesWorker256 *worker = nullptr;
    while ((worker = getIdleWorker()) == nullptr) {
        updateWorkers();
        qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
        Sleep::msleep(1000 / 30);
    }
    if (mCancel) {
        mError.clear();
        return false;
    }
#endif
    mProgressDialog->setPrompt(tr("Loading maps (%1,%2)").arg(cell256X).arg(cell256Y));
    CombinedCellMaps *combinedMaps = new CombinedCellMaps();
    bool ok = combinedMaps->startLoading(mWorldDoc, cell256X, cell256Y);
    qApp->processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
    if ((ok == false) || (combinedMaps->mError.isEmpty() == false)) {
        mError = combinedMaps->mError;
        delete combinedMaps;
        return false;
    }
    worker->mCombinedCellMaps = combinedMaps;
    worker->mCell = cell;
    worker->mStatus = LotFilesWorker256::Status::LoadingMaps;
    return true;
}

void LotFilesManager256::workTimerTimeout()
{
    updateWorkers();
}

void LotFilesManager256::workerThreadFinished(int i)
{
    qDebug() << "LotFilesManager256: worker thread finished #" << i;
    mWorkers[i]->deleteLater();
    mWorkerThreads[i]->deleteLater();
    mWorkers[i] = nullptr;
    mWorkerThreads[i] = nullptr;
}

void LotFilesManager256::cancel()
{
    mCancel = true;
    mProgressDialog->setPrompt(tr("Stopping..."));
}

bool LotFilesWorker256::generateCell()
{
    mStats.reset();

    CombinedCellMaps& combinedMaps = *mCombinedCellMaps;
    mWorldDoc = mManager->mWorldDoc;

    int cell256X = combinedMaps.mCell256X;
    int cell256Y = combinedMaps.mCell256Y;

    MapComposite* mapComposite = combinedMaps.mMapComposite;
    MapInfo* mapInfo = mapComposite->mapInfo();

//    PROGRESS progress(tr("Generating .lot files (%1,%2)").arg(cell256X).arg(cell256Y));

    // Check for missing tilesets.
    for (MapComposite *mc : mapComposite->maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            mError = tr("Some tilesets are missing in a map in cell %1,%2:\n%3").arg(cell256X).arg(cell256Y).arg(mc->mapInfo()->path());
            mStatus = Status::Error;
//            qApp->processEvents(QEventLoop::ProcessEventsFlag::ExcludeUserInputEvents); // handle any pending signal-to-slot before moving threads
            mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
            return false;
        }
    }

    if (generateHeader(combinedMaps, mapComposite) == false) {
        mStatus = Status::Error;
        mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
        return false;
    }

    bool chunkDataOnly = false;
    if (chunkDataOnly) {
        for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
            lg->prepareDrawing2();
        }
        generateChunkData();
        clearRemovedBuildingsList();
        mStatus = Status::Finished;
        mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
        return true;
    }

    int mapWidth = combinedMaps.mCellsWidth * CELL_WIDTH;
    int mapHeight = combinedMaps.mCellsHeight * CELL_HEIGHT;

    // Resize the grid and cleanup data from the previous cell.
    mGridData.resize(mapWidth);
    for (int x = 0; x < mapWidth; x++) {
        mGridData[x].resize(mapHeight);
        for (int y = 0; y < mapHeight; y++) {
            mGridData[x][y].fill(LotFile::Square(), MAX_WORLD_LEVELS);
        }
    }

    // FIXME: This is for all the 300x300 cells, not just the single 256x256 cell.
    mMinLevel = 10000;
    mMaxLevel = -10000;

    Tile *missingTile = Tiled::Internal::TilesetManager::instance()->missingTile();
    QRect cellBounds256(cell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH,
                        cell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_WIDTH,
                        CELL_SIZE_256, CELL_SIZE_256);
    QVector<const Tiled::Cell *> cells(40);
    for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
        lg->prepareDrawing2();
        int d = (mapInfo->orientation() == Map::Isometric) ? -3 : 0;
        d *= lg->level();
        for (int y = d; y < mapHeight; y++) {
            for (int x = d; x < mapWidth; x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                for (const Tiled::Cell *cell : qAsConst(cells)) {
                    if (cell->tile == missingTile) continue;
                    int lx = x, ly = y;
                    if (mapInfo->orientation() == Map::Isometric) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    if (lx >= mapWidth) continue;
                    if (ly >= mapHeight) continue;
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    mGridData[lx][ly][lg->level() - MIN_WORLD_LEVEL].Entries.append(e);
                    if (cellBounds256.contains(lx, ly)) {
                        TileMap[e->gid]->used = true;
                        mMinLevel = std::min(mMinLevel, lg->level());
                        mMaxLevel = std::max(mMaxLevel, lg->level());
                    }
                }
            }
        }
    }

    if (mMinLevel == 10000) {
        mMinLevel = mMaxLevel = 0;
    }

    generateBuildingObjects(mapWidth, mapHeight);

    generateJumboTrees(combinedMaps);

    generateHeaderAux(cell256X, cell256Y);

    /////

    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    QString fileName = tr("world_%1_%2.lotpack")
            .arg(cell256X)
            .arg(cell256Y);

    QString lotsDirectory = lotSettings.exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        mStatus = Status::Error;
        mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('L') << quint8('O') << quint8('T') << quint8('P');

    out << qint32(VERSION_LATEST);

    // C# 'long' is signed 64-bit integer
    out << qint32(CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256);
    for (int m = 0; m < CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256; m++) {
        out << qint64(m);
    }

    QList<qint64> PositionMap;

    for (int x = 0; x < CHUNKS_PER_CELL_256; x++) {
        for (int y = 0; y < CHUNKS_PER_CELL_256; y++) {
            PositionMap += file.pos();
            int chunkX = cell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH + x * CHUNK_SIZE_256;
            int chunkY = cell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT + y * CHUNK_SIZE_256;
            if (generateChunk(out, chunkX, chunkY) == false) {
                mStatus = Status::Error;
                mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
                return false;
            }
        }
    }

    file.seek(4 + 4 + 4); // 'LOTS' + version + #chunks
    for (int m = 0; m < CHUNKS_PER_CELL_256 * CHUNKS_PER_CELL_256; m++) {
        out << qint64(PositionMap[m]);
    }

    file.close();

    generateChunkData();

    clearRemovedBuildingsList();

    mStatus = Status::Finished;
    mCombinedCellMaps->moveToThread(mCombinedCellMaps->mMapComposite, qApp->thread());
    return true;
}

LotFilesWorker256::LotFilesWorker256(LotFilesManager256 *manager, InterruptibleThread *thread) :
    BaseWorker(thread),
    mManager(manager),
    mWorldDoc(manager->mWorldDoc),
    mRoomRectLookup(),
    mRoomLookup()
{

}

void LotFilesWorker256::work()
{
    IN_WORKER_THREAD
    generateCell();
}

bool LotFilesWorker256::generateHeader(CombinedCellMaps& combinedMaps, MapComposite *mapComposite)
{
    qDeleteAll(mRoomRects);
    qDeleteAll(roomList);
    qDeleteAll(buildingList);

    mRoomRects.clear();
    mRoomRectByLevel.clear();
    roomList.clear();
    buildingList.clear();

    // Create the set of all tilesets used by the map and its sub-maps.
    QList<Tileset*> tilesets;
    for (MapComposite *mc : mapComposite->maps())
        tilesets += mc->map()->tilesets();

    mJumboTreeTileset = nullptr;
    if (mJumboTreeTileset == nullptr) {
        mJumboTreeTileset = new Tiled::Tileset(QLatin1String("jumbo_tree_01"), 64, 128);
        mJumboTreeTileset->loadFromNothing(QSize(64, 128), QLatin1String("jumbo_tree_01"));
    }
    tilesets += mJumboTreeTileset;
    QScopedPointer<Tiled::Tileset> scoped(mJumboTreeTileset);

    qDeleteAll(TileMap.values());
    TileMap.clear();
    TileMap[0] = new LotFile::Tile;

    mTilesetToFirstGid.clear();
    mTilesetNameToFirstGid.clear();
    uint firstGid = 1;
    for (Tileset *tileset : tilesets) {
        if (!handleTileset(tileset, firstGid))
            return false;
    }

    const GenerateLotsSettings &lotSettings = combinedMaps.mCells[0]->world()->getGenerateLotsSettings();

    for (WorldCell *cell : combinedMaps.mCells) {
        for (MapComposite *subMap : mapComposite->subMaps()) {
            if (subMap->origin() != (cell->pos() + lotSettings.worldOrigin - QPoint(combinedMaps.mMinCell300X,combinedMaps.mMinCell300Y)) * 300)
                continue;
            if (processObjectGroups(combinedMaps, cell, subMap) == false) {
                return false;
            }
        }
    }

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    QPoint relativeToCell256(-(combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH),
                            -(combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT));
    for (int level : mRoomRectByLevel.keys()) {
        QList<LotFile::RoomRect*> rrList = mRoomRectByLevel[level];
        // Use spatial partitioning to speed up the code below.
        mRoomRectLookup.clear(relativeToCell256.x(), relativeToCell256.y(), combinedMaps.mCellsWidth * CHUNKS_PER_CELL, combinedMaps.mCellsHeight * CHUNKS_PER_CELL, CHUNK_WIDTH);
        for (LotFile::RoomRect *rr : rrList) {
            mRoomRectLookup.add(rr, rr->bounds());
        }
        for (LotFile::RoomRect *rr : rrList) {
            if (rr->room == nullptr) {
                rr->room = new LotFile::Room(rr->nameWithoutSuffix(), rr->floor);
                rr->room->rects += rr;
                rr->room->mCell = rr->mCell;
                roomList += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#')))
                continue;
            QList<LotFile::RoomRect*> rrList2;
            mRoomRectLookup.overlapping(QRect(rr->bounds().adjusted(-1, -1, 1, 1)), rrList2);
            for (LotFile::RoomRect *comp : rrList2) {
                if (comp == rr)
                    continue;
                // Don't merge rects across 300x300 cell boundaries, like the south wall in the Studio map.
                if (rr->mCell != comp->mCell)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != nullptr) {
                        LotFile::Room *room = comp->room;
                        for (LotFile::RoomRect *rr2 : room->rects) {
                            Q_ASSERT(rr2->room == room);
                            Q_ASSERT(!rr->room->rects.contains(rr2));
                            rr2->room = rr->room;
                        }
                        rr->room->rects += room->rects;
                        roomList.removeOne(room);
                        delete room;
                    } else {
                        comp->room = rr->room;
                        rr->room->rects += comp;
                        Q_ASSERT(rr->room->rects.count(comp) == 1);
                    }
                }
            }
        }
    }

    mRoomLookup.clear(relativeToCell256.x(), relativeToCell256.y(), combinedMaps.mCellsWidth * CHUNKS_PER_CELL, combinedMaps.mCellsHeight * CHUNKS_PER_CELL, CHUNK_WIDTH);
    for (LotFile::Room *r : roomList) {
        r->mBounds = r->calculateBounds();
        mRoomLookup.add(r, r->bounds());
    }

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    for (LotFile::Room *r : roomList) {
        if (r->building == nullptr) {
            r->building = new LotFile::Building();
            buildingList += r->building;
            r->building->RoomList += r;
        }
        QList<LotFile::Room*> roomList2;
        mRoomLookup.overlapping(r->bounds().adjusted(-1, -1, 1, 1), roomList2);
        for (LotFile::Room *comp : roomList2) {
            if (comp == r)
                continue;
            // Don't merge rooms across 300x300 cell boundaries, like the south wall in the Studio map.
            if (r->mCell != comp->mCell)
                continue;
            if (r->building == comp->building)
                continue;
            if (r->inSameBuilding(comp)) {
                if (comp->building != nullptr) {
                    LotFile::Building *b = comp->building;
                    for (LotFile::Room *r2 : b->RoomList) {
                        Q_ASSERT(r2->building == b);
                        Q_ASSERT(!r->building->RoomList.contains(r2));
                        r2->building = r->building;
                    }
                    r->building->RoomList += b->RoomList;
                    buildingList.removeOne(b);
                    delete b;
                } else {
                    comp->building = r->building;
                    r->building->RoomList += comp;
                    Q_ASSERT(r->building->RoomList.count(comp) == 1);
                }
            }
        }
    }

    // Remove buildings with their north-west corner not in the cell.
    // Buildings may extend past the east and south edges of the 256x256 cell.
    QRect cellBounds256(0, 0, CELL_SIZE_256, CELL_SIZE_256);
    for (int i = buildingList.size() - 1; i >= 0; i--) {
        LotFile::Building* building = buildingList[i];
        QRect bounds = building->calculateBounds();
        if (bounds.isEmpty()) {
            continue;
        }
        if (cellBounds256.contains(bounds.topLeft())) {
            continue;
        }
        for (LotFile::Room *room : building->RoomList) {
            for (LotFile::RoomRect *roomRect : room->rects) {
                mRoomRects.removeOne(roomRect);
                mRoomRectByLevel[roomRect->floor].removeOne(roomRect);
//                delete roomRect;
            }
            roomList.removeOne(room);
//            delete room;
        }
        buildingList.removeAt(i);
//        delete building;
        mRemovedBuildingList += building;
    }

    for (int i = 0; i < roomList.size(); i++) {
        roomList[i]->ID = i;
    }
    mStats.numRoomRects += mRoomRects.size();
    mStats.numRooms += roomList.size();

    mStats.numBuildings += buildingList.size();

    return true;
}

bool LotFilesWorker256::generateHeaderAux(int cell256X, int cell256Y)
{
    QString fileName = tr("%1_%2.lotheader")
            .arg(cell256X)
            .arg(cell256Y);

    QString lotsDirectory = mWorldDoc->world()->getGenerateLotsSettings().exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('L') << quint8('O') << quint8('T') << quint8('H');

    out << qint32(VERSION_LATEST);

    QList<LotFile::Tile*> usedTiles;
    for (LotFile::Tile *tile : TileMap) {
        if (tile->used) {
            usedTiles += tile;
            if (tile->name.startsWith(QLatin1String("jumbo_tree_01"))) {
                int nnn = 0;
            }
        }
    }
    out << qint32(usedTiles.size());
    std::sort(usedTiles.begin(), usedTiles.end(), [](const LotFile::Tile *a, const LotFile::Tile *b) {
        return QString::compare(a->name, b->name, Qt::CaseSensitive) < 0;
    });
    for (int i = 0; i < usedTiles.size(); i++) {
        LotFile::Tile *tile = usedTiles[i];
        SaveString(out, tile->name);
        tile->id = i;
    }

    out << qint32(CHUNK_SIZE_256);
    out << qint32(CHUNK_SIZE_256);
    out << qint32(mMinLevel);
    out << qint32(mMaxLevel);

    out << qint32(roomList.count());
    for (LotFile::Room *room : roomList) {
        SaveString(out, room->name);
        out << qint32(room->floor);

        out << qint32(room->rects.size());
        for (LotFile::RoomRect *rr : room->rects) {
            out << qint32(rr->x);
            out << qint32(rr->y);
            out << qint32(rr->w);
            out << qint32(rr->h);
        }

        out << qint32(room->objects.size());
        for (const LotFile::RoomObject &object : room->objects) {
            out << qint32(object.metaEnum);
            out << qint32(object.x);
            out << qint32(object.y);
        }
    }

    out << qint32(buildingList.count());
    for (LotFile::Building *building : buildingList) {
        out << qint32(building->RoomList.count());
        for (LotFile::Room *room : building->RoomList) {
            out << qint32(room->ID);
        }
    }

    // Set the zombie intensity on each square using the spawn image.
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
    const int MAX_300x300_CELLS = 3;
    quint8 ZombieIntensity[MAX_300x300_CELLS * CELL_WIDTH][MAX_300x300_CELLS * CELL_HEIGHT];
    const QImage& ZombieSpawnMap = mManager->ZombieSpawnMap;
    QRect zombieSpawnMapBounds(lotSettings.worldOrigin.x() * CHUNKS_PER_CELL, lotSettings.worldOrigin.y() * CHUNKS_PER_CELL, ZombieSpawnMap.width(), ZombieSpawnMap.height());
    QRect combinedMapBounds(mCombinedCellMaps->mMinCell300X * CHUNKS_PER_CELL, mCombinedCellMaps->mMinCell300Y * CHUNKS_PER_CELL, mCombinedCellMaps->mCellsWidth * CHUNKS_PER_CELL, mCombinedCellMaps->mCellsHeight * CHUNKS_PER_CELL);
    QRect bounds = zombieSpawnMapBounds & combinedMapBounds;
    for (int chunkY = bounds.top(); chunkY <= bounds.bottom(); chunkY++) {
        for (int chunkX = bounds.left(); chunkX <= bounds.right(); chunkX++) {
            QRgb pixel = ZombieSpawnMap.pixel(chunkX - zombieSpawnMapBounds.left(), chunkY - zombieSpawnMapBounds.top());
            quint8 chunkIntensity = qRed(pixel);
            for (int squareY = 0; squareY < CHUNK_HEIGHT; squareY++) {
                for (int squareX = 0; squareX < CHUNK_WIDTH; squareX++) {
                    int gx = (chunkX - combinedMapBounds.left()) * CHUNK_WIDTH + squareX;
                    int gy = (chunkY - combinedMapBounds.top()) * CHUNK_HEIGHT + squareY;
                    ZombieIntensity[gx][gy] = chunkIntensity;
                }
            }
        }
    }

    zombieSpawnMapBounds = QRect(lotSettings.worldOrigin.x() * CELL_WIDTH, lotSettings.worldOrigin.y() * CELL_HEIGHT, ZombieSpawnMap.width() * CHUNK_WIDTH, ZombieSpawnMap.height() * CHUNK_HEIGHT);
    combinedMapBounds = QRect(mCombinedCellMaps->mMinCell300X * CELL_WIDTH, mCombinedCellMaps->mMinCell300Y * CELL_HEIGHT, mCombinedCellMaps->mCellsWidth * CELL_WIDTH, mCombinedCellMaps->mCellsHeight * CELL_HEIGHT);
    QRect combinedMapBounds256(cell256X * CELL_SIZE_256, cell256Y * CELL_SIZE_256, CELL_SIZE_256, CELL_SIZE_256);
    QRect validSquares = zombieSpawnMapBounds & combinedMapBounds256;
    QPoint p1 = combinedMapBounds256.topLeft();
    for (int x = 0; x < CHUNKS_PER_CELL_256; x++) {
        for (int y = 0; y < CHUNKS_PER_CELL_256; y++) {
#if 1
            QRect chunkRect(p1.x() + x * CHUNK_SIZE_256, p1.y() + y * CHUNK_SIZE_256, CHUNK_SIZE_256, CHUNK_SIZE_256);
            chunkRect &= validSquares;
            int chunkIntensity = 0;
            for (int y3 = chunkRect.top(); y3 <= chunkRect.bottom(); y3++) {
                for (int x3 = chunkRect.left(); x3 <= chunkRect.right(); x3++) {
                    chunkIntensity += ZombieIntensity[x3 - combinedMapBounds.left()][y3 - combinedMapBounds.top()];
                }
            }
            float alpha = chunkIntensity / float(CHUNK_SIZE_256 * CHUNK_SIZE_256 * 255);
            out << qint8(alpha * 255);
#else
            qint8 density = calculateZombieDensity(x1 + x * CHUNK_SIZE_256, y1 + y * CHUNK_SIZE_256);
            out << density;
#endif
        }
    }

    file.close();

    return true;
}

bool LotFilesWorker256::generateChunk(QDataStream &out, int chunkX, int chunkY)
{
    int notdonecount = 0;
    for (int z = mMinLevel; z <= mMaxLevel; z++)  {
        for (int x = 0; x < CHUNK_SIZE_256; x++) {
            for (int y = 0; y < CHUNK_SIZE_256; y++) {
                int gx = chunkX + x;
                int gy = chunkY + y;
                const QList<LotFile::Entry*> &entries = mGridData[gx][gy][z - MIN_WORLD_LEVEL].Entries;
                if (entries.count() == 0) {
                    notdonecount++;
                    continue;
                }
                if (notdonecount > 0) {
                    out << qint32(-1);
                    out << qint32(notdonecount);
                }
                notdonecount = 0;
                out << qint32(entries.count() + 1);
                out << qint32(getRoomID(gx, gy, z));
                for (const LotFile::Entry *entry : entries) {
                    Q_ASSERT(TileMap[entry->gid]);
                    Q_ASSERT(TileMap[entry->gid]->id != -1);
                    out << qint32(TileMap[entry->gid]->id);
                }
            }
        }
    }
    if (notdonecount > 0) {
        out << qint32(-1);
        out << qint32(notdonecount);
    }

    return true;
}

void LotFilesWorker256::generateBuildingObjects(int mapWidth, int mapHeight)
{
    for (LotFile::Room *room : roomList) {
        for (LotFile::RoomRect *rr : room->rects) {
            generateBuildingObjects(mapWidth, mapHeight, room, rr);
        }
    }
}

void LotFilesWorker256::generateBuildingObjects(int mapWidth, int mapHeight, LotFile::Room *room, LotFile::RoomRect *rr)
{
    for (int x = rr->x; x < rr->x + rr->w; x++) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];

            // Remember the room at each position in the map.
            // TODO: Remove this, it isn't used by Java code now.
            square.roomID = room->ID;

            /* Examine every tile inside the room.  If the tile's metaEnum >= 0
               then create a new RoomObject for it. */
            for (LotFile::Entry *entry : square.Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y;
                    object.metaEnum = metaEnum;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check south of the room for doors.
    int y = rr->y + rr->h;
    if (y < mapHeight) {
        for (int x = rr->x; x < rr->x + rr->w; x++) {
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];
            for (LotFile::Entry *entry : square.Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumNorth(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y - 1;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check east of the room for doors.
    int x = rr->x + rr->w;
    if (x < mapWidth) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {
            LotFile::Square& square = mGridData[x][y][room->floor - MIN_WORLD_LEVEL];
            for (LotFile::Entry *entry : square.Entries) {
                int metaEnum = TileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumWest(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x - 1;
                    object.y = y;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }
}

void LotFilesWorker256::generateJumboTrees(CombinedCellMaps& combinedMaps)
{
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();

    const quint8 JUMBO_ZONE = 1;
    const quint8 PREVENT_JUMBO = 2;
    const quint8 REMOVE_TREE = 3;
    const quint8 JUMBO_TREE = 4;

    QSet<QString> treeTiles;
    QSet<QString> floorVegTiles;
    for (TileDefFile *tdf : Navigate::IsoGridSquare256::mTileDefFiles) {
        for (TileDefTileset *tdts : tdf->tilesets()) {
            for (TileDefTile *tdt : tdts->mTiles) {
                // Get the set of all tree tiles.
                if (tdt->mProperties.contains(QLatin1String("tree")) || (tdts->mName.startsWith(QLatin1String("vegetation_trees")))) {
                    treeTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
                // Get the set of all floor + vegetation tiles.
                if (tdt->mProperties.contains(QLatin1String("solidfloor")) ||
                        tdt->mProperties.contains(QLatin1String("FloorOverlay")) ||
                        tdt->mProperties.contains(QLatin1String("vegitation"))) {
                    floorVegTiles += QString::fromLatin1("%1_%2").arg(tdts->mName).arg(tdt->id());
                }
            }
        }
    }

    quint8 grid[CELL_SIZE_256][CELL_SIZE_256];
    quint8 densityGrid[CELL_SIZE_256][CELL_SIZE_256];
    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            grid[x][y] = PREVENT_JUMBO;
            densityGrid[x][y] = 0;
        }
    }

    QHash<ObjectType*,const JumboZone*> objectTypeMap;
    for (const JumboZone* jumboZone : qAsConst(mManager->mJumboZoneList)) {
        if (ObjectType *objectType = mWorldDoc->world()->objectType(jumboZone->zoneName)) {
            objectTypeMap[objectType] = jumboZone;
        }
    }

    PropertyDef *JumboDensity = mWorldDoc->world()->propertyDefinition(QStringLiteral("JumboDensity"));

    QRect cellBounds256(combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH,
                        combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT,
                        CELL_SIZE_256, CELL_SIZE_256);

    ClipperLib::Path zonePath;
    for (WorldCell* cell : combinedMaps.mCells) {
        QPoint cellPos300((cell->x() + lotSettings.worldOrigin.x() - combinedMaps.mMinCell300X) * CELL_WIDTH,
                          (cell->y() + lotSettings.worldOrigin.y() - combinedMaps.mMinCell300Y) * CELL_HEIGHT);
        for (WorldCellObject *obj : cell->objects()) {
            if ((obj->level() != 0) || !objectTypeMap.contains(obj->type())) {
                continue;
            }
            if (obj->isPoint() || obj->isPolyline()) {
                continue;
            }
            zonePath.clear();
            if (obj->isPolygon()) {
                for (const auto &pt : obj->points()) {
                    zonePath << ClipperLib::IntPoint(cellPos300.x() + pt.x * 100, cellPos300.y() + pt.y * 100);
                }
            }
            quint8 density = objectTypeMap[obj->type()]->density;
            Property* property = JumboDensity ? obj->properties().find(JumboDensity) : nullptr;
            if (property != nullptr) {
                bool ok = false;
                int value = property->mValue.toInt(&ok);
                if (ok && (value >= 0) && (value <= 100)) {
                    density = value;
                }
            }
            int ox = cellPos300.x() + obj->x();
            int oy = cellPos300.y() + obj->y();
            int ow = obj->width();
            int oh = obj->height();
            for (int y = oy; y < oy + oh; y++) {
                for (int x = ox; x < ox + ow; x++) {
                    if ((zonePath.empty() == false)) {
                        ClipperLib::IntPoint pt(x * 100 + 50, y * 100 + 50); // center of the square
                        if (ClipperLib::PointInPolygon(pt, zonePath) == 0) {
                            continue;
                        }
                    }
                    int gx = x - cellBounds256.x();
                    int gy = y - cellBounds256.y();
                    if ((gx >= 0) && (gx < CELL_SIZE_256) && (gy >= 0) && (gy < CELL_SIZE_256)) {
                        grid[gx][gy] = JUMBO_ZONE;
                        densityGrid[gx][gy] = std::max(densityGrid[gx][gy], density);
                    }
                }
            }
        }
    }

    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            // Prevent jumbo trees near any second-story tiles
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            if (!mGridData[wx][wy][1 - MIN_WORLD_LEVEL].Entries.isEmpty()) {
                for (int yy = y; yy <= y + 4; yy++) {
                    for (int xx = x; xx <= x + 4; xx++) {
                        if (xx >= 0 && xx < CELL_SIZE_256 && yy >= 0 && yy < CELL_SIZE_256)
                            grid[xx][yy] = PREVENT_JUMBO;
                    }
                }
            }

            // Prevent jumbo trees near non-floor, non-vegetation (fences, etc)
            const auto& entries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
            for (LotFile::Entry *e : entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (!floorVegTiles.contains(tile->name)) {
                    for (int yy = y - 1; yy <= y + 1; yy++) {
                        for (int xx = x - 1; xx <= x + 1; xx++) {
                            if (xx >= 0 && xx < CELL_SIZE_256 && yy >= 0 && yy < CELL_SIZE_256)
                                grid[xx][yy] = PREVENT_JUMBO;
                        }
                    }
                    break;
                }
            }
        }
    }

    // Prevent jumbo trees near north/west edges of cells
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < CELL_SIZE_256; y++) {
            grid[x][y] = PREVENT_JUMBO;
        }
    }

    // Get a list of all tree positions in the cell.
    QList<QPoint> allTreePos;
    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            const auto& entries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
            for (LotFile::Entry *e : entries) {
                LotFile::Tile *tile = TileMap[e->gid];
                if (treeTiles.contains(tile->name) == false) {
                    continue;
                }
                allTreePos += QPoint(x, y);
                break;
            }
        }
    }

    QRandomGenerator qrand;

    while (!allTreePos.isEmpty()) {
        int r = qrand() % allTreePos.size();
        QPoint treePos = allTreePos.takeAt(r);
        quint8 g = grid[treePos.x()][treePos.y()];
        quint8 density = densityGrid[treePos.x()][treePos.y()];
        if ((g == JUMBO_ZONE) && (qrand() % 100 < density)) {
            grid[treePos.x()][treePos.y()] = JUMBO_TREE;
            // Remove all trees surrounding a jumbo tree.
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0)
                        continue;
                    int x = treePos.x() + dx;
                    int y = treePos.y() + dy;
                    if (x >= 0 && x < CELL_SIZE_256 && y >= 0 && y < CELL_SIZE_256)
                        grid[x][y] = REMOVE_TREE;
                }
            }
        }
    }

    for (int y = 0; y < CELL_SIZE_256; y++) {
        for (int x = 0; x < CELL_SIZE_256; x++) {
            int wx = x + cellBounds256.x();
            int wy = y + cellBounds256.y();
            if (grid[x][y] == JUMBO_TREE) {
                QList<LotFile::Entry*>& squareEntries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
                for (LotFile::Entry *e : squareEntries) {
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        e->gid = mTilesetToFirstGid[mJumboTreeTileset];
                        TileMap[e->gid]->used = true;
                        break;
                    }
                }
            }
            if (grid[x][y] == REMOVE_TREE) {
                QList<LotFile::Entry*>& squareEntries = mGridData[wx][wy][0 - MIN_WORLD_LEVEL].Entries;
                for (int i = 0; i < squareEntries.size(); i++) {
                    LotFile::Entry *e = squareEntries[i];
                    LotFile::Tile *tile = TileMap[e->gid];
                    if (treeTiles.contains(tile->name)) {
                        squareEntries.removeAt(i);
                        break;
                    }
                }
            }
        }
    }
}

void LotFilesWorker256::generateChunkData()
{
    mRoomRectLookup.clear(0, 0, CHUNKS_PER_CELL_256, CHUNKS_PER_CELL_256, CHUNK_SIZE_256);
    for (LotFile::RoomRect *rr : mRoomRectByLevel[0]) {
        mRoomRectLookup.add(rr, rr->bounds());
    }
    for (LotFile::Building *building : mRemovedBuildingList) {
        for (LotFile::Room *room : building->RoomList) {
            for (LotFile::RoomRect *rr : room->rects) {
                if (rr->floor == 0) {
                    mRoomRectLookup.add(rr, rr->bounds());
                }
            }
        }
    }
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
    Navigate::ChunkDataFile256 cdf;
    cdf.fromMap(*mCombinedCellMaps, mCombinedCellMaps->mMapComposite, mRoomRectLookup, lotSettings);
}

void LotFilesWorker256::clearRemovedBuildingsList()
{
    for (LotFile::Building *building : mRemovedBuildingList) {
        for (LotFile::Room *room : building->RoomList) {
            for (LotFile::RoomRect *rr : room->rects) {
                delete rr;
            }
            delete room;
        }
        delete building;
    }
    mRemovedBuildingList.clear();
}

bool LotFilesWorker256::handleTileset(const Tiled::Tileset *tileset, uint &firstGid)
{
    if (!tileset->fileName().isEmpty()) {
        mError = tr("Only tileset image files supported, not external tilesets");
        return false;
    }

    QString name = nameOfTileset(tileset);

    // TODO: Verify that two tilesets sharing the same name are identical
    // between maps.
#if 1
    auto it = mTilesetNameToFirstGid.find(name);
    if (it != mTilesetNameToFirstGid.end()) {
        mTilesetToFirstGid.insert(tileset, it.value());
        return true;
    }
#else
    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end) {
        QString name2 = nameOfTileset(i.key());
        if (name == name2) {
            mTilesetToFirstGid.insert(tileset, i.value());
            return true;
        }
        ++i;
    }
#endif

    for (int i = 0; i < tileset->tileCount(); ++i) {
        int localID = i;
        int ID = firstGid + localID;
        LotFile::Tile *tile = new LotFile::Tile(QStringLiteral("%1_%2").arg(name).arg(localID));
        tile->metaEnum = TileMetaInfoMgr::instance()->tileEnumValue(tileset->tileAt(i));
        TileMap[ID] = tile;
    }

    mTilesetToFirstGid.insert(tileset, firstGid);
    mTilesetNameToFirstGid.insert(name, firstGid);
    firstGid += tileset->tileCount();

    return true;
}

int LotFilesWorker256::getRoomID(int x, int y, int z)
{
    return mGridData[x][y][z - MIN_WORLD_LEVEL].roomID;
}

uint LotFilesWorker256::cellToGid(const Cell *cell)
{
    Tileset *tileset = cell->tile->tileset();

#if 1
    auto v = mTilesetToFirstGid.find(tileset);
    if (v == mTilesetToFirstGid.end()) {
        // tileset not found
        return 0;
    }
    return v.value() + cell->tile->id();
#else
    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end && i.key() != tileset)
        ++i;
    if (i == i_end) // tileset not found
        return 0;

    return i.value() + cell->tile->id();
#endif
}

bool LotFilesWorker256::processObjectGroups(CombinedCellMaps &combinedMaps, WorldCell *cell, MapComposite *mapComposite)
{
    for (Layer *layer : mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (!processObjectGroup(combinedMaps, cell, og, mapComposite->levelRecursive(), mapComposite->originRecursive()))
                return false;
        }
    }

    for (MapComposite *subMap : mapComposite->subMaps())
        if (!processObjectGroups(combinedMaps, cell, subMap))
            return false;

    return true;
}

bool LotFilesWorker256::processObjectGroup(CombinedCellMaps &combinedMaps, WorldCell *cell, ObjectGroup *objectGroup, int levelOffset, const QPoint &offset)
{
    int level = objectGroup->level();
    level += levelOffset;

    // Align with the 256x256 cell.
    QPoint offset1 = offset;
    offset1.rx() -= combinedMaps.mCell256X * CELL_SIZE_256 - combinedMaps.mMinCell300X * CELL_WIDTH;
    offset1.ry() -= combinedMaps.mCell256Y * CELL_SIZE_256 - combinedMaps.mMinCell300Y * CELL_HEIGHT;

    for (const MapObject *mapObject : objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (!mapObject->width() || !mapObject->height())
            continue;

        int x = std::floor(mapObject->x());
        int y = std::floor(mapObject->y());
        int w = std::ceil(mapObject->x() + mapObject->width()) - x;
        int h = std::ceil(mapObject->y() + mapObject->height()) - y;

        QString name = mapObject->name();
        if (name.isEmpty())
            name = QLatin1String("unnamed");

        if (objectGroup->map()->orientation() == Map::Isometric) {
            x += 3 * level;
            y += 3 * level;
        }

        if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
            if (x < 0 || y < 0 || x + w > CELL_WIDTH || y + h > CELL_HEIGHT) {
                x = qBound(0, x, CELL_WIDTH);
                y = qBound(0, y, CELL_HEIGHT);
                mError = tr("A RoomDef in cell %1,%2 overlaps cell boundaries.\nNear x,y=%3,%4")
                        .arg(cell->x()).arg(cell->y()).arg(x).arg(y);
                return false;
            }
            // Apply the MapComposite offset in the top-level map.
            x += offset1.x();
            y += offset1.y();
            LotFile::RoomRect *rr = new LotFile::RoomRect(name, x, y, level, w, h);
            rr->mCell = cell;
            mRoomRects += rr;
            mRoomRectByLevel[level] += rr;
        }
    }
    return true;
}

void LotFilesWorker256::resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    for (PropertyTemplate *pt : ph->templates())
        resolveProperties(pt, result);
    for (Property *p : ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

qint8 LotFilesWorker256::calculateZombieDensity(int x, int y)
{
    // TODO: Get the total depth of 8x8 squares, then divide by 64.
    const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
    int chunk300X = std::floor(x / float(CHUNK_WIDTH)) - lotSettings.worldOrigin.x() * CHUNKS_PER_CELL;
    int chunk300Y = std::floor(y / float(CHUNK_HEIGHT)) - lotSettings.worldOrigin.y() * CHUNKS_PER_CELL;
    const QImage& ZombieSpawnMap = mManager->ZombieSpawnMap;
    if (chunk300X < 0 || chunk300Y < 0 || chunk300X >= ZombieSpawnMap.size().width() || chunk300Y >= ZombieSpawnMap.size().height()) {
        return 0;
    }
    QRgb pixel = ZombieSpawnMap.pixel(chunk300X, chunk300Y);
    int intensity = qRed(pixel);
    return quint8(intensity);
}

void LotFilesWorker256::addJob()
{
    scheduleWork();
}

//const QString LotFilesWorker256::tr(const char *str) const
//{
//    return mManager->tr(str);
//}

/////

CombinedCellMaps::CombinedCellMaps()
{

}

CombinedCellMaps::~CombinedCellMaps()
{
    if (mMapComposite == nullptr) {
        return;
    }
    MapInfo* mapInfo = mMapComposite->mapInfo(); // 256x256
    delete mMapComposite;
    delete mapInfo->map();
    delete mapInfo;
}

bool CombinedCellMaps::startLoading(WorldDocument *worldDoc, int cell256X, int cell256Y)
{
    const GenerateLotsSettings &lotSettings = worldDoc->world()->getGenerateLotsSettings();
    mCell256X = cell256X;
    mCell256Y = cell256Y;
    QRect cellBounds300 = toCellRect300(QRect(cell256X, cell256Y, 1, 1));
    int minCell300X = cellBounds300.x();
    int minCell300Y = cellBounds300.y();
    int maxCell300X = cellBounds300.right() + 1;
    int maxCell300Y = cellBounds300.bottom() + 1;
    mMinCell300X = minCell300X;
    mMinCell300Y = minCell300Y;
    mCellsWidth = maxCell300X - minCell300X;
    mCellsHeight = maxCell300Y - minCell300Y;
    mCells.clear();
    for (int cell300Y = minCell300Y; cell300Y < maxCell300Y; cell300Y++) {
        for (int cell300X = minCell300X; cell300X < maxCell300X; cell300X++) {
            WorldCell* cell = worldDoc->world()->cellAt(cell300X - lotSettings.worldOrigin.x(), cell300Y - lotSettings.worldOrigin.y());
            if (cell == nullptr) {
                continue;
            }
            if (cell->mapFilePath().isEmpty()) {
                continue;
            }
            MapInfo *mapInfo = MapManager::instance()->loadMap(cell->mapFilePath(), QString(), true);
            if (mapInfo == nullptr) {
                mError = MapManager::instance()->errorString();
                return false;
            }
            mLoader.addMap(mapInfo);
            for (WorldCellLot *lot : cell->lots()) {
                if (MapInfo *info = MapManager::instance()->loadMap(lot->mapName(), QString(), true, MapManager::PriorityMedium)) {
                    mLoader.addMap(info);
                } else {
                    mError = MapManager::instance()->errorString();
                    return false;
                }
            }
            mCells += cell;
        }
    }
    return true;
}

int CombinedCellMaps::checkLoading(WorldDocument *worldDoc)
{
    if (mLoader.isLoading()) {
        return 0;
    }
    if (mLoader.errorString().isEmpty() == false) {
        mError = mLoader.errorString();
        return -1;
    }
    const GenerateLotsSettings &lotSettings = worldDoc->world()->getGenerateLotsSettings();
    MapInfo* mapInfo = getCombinedMap();
    mMapComposite = new MapComposite(mapInfo);
    for (WorldCell* cell : mCells) {
        MapInfo *info = MapManager::instance()->mapInfo(cell->mapFilePath());
        QPoint cellPos((cell->x() + lotSettings.worldOrigin.x() - mMinCell300X) * CELL_WIDTH, (cell->y() + lotSettings.worldOrigin.y() - mMinCell300Y) * CELL_HEIGHT);
        MapComposite* subMap = mMapComposite->addMap(info, cellPos, 0);
        subMap->setLotFilesManagerMap(true);
        for (WorldCellLot *lot : cell->lots()) {
            MapInfo *info = MapManager::instance()->mapInfo(lot->mapName());
            subMap->addMap(info, lot->pos(), lot->level());
        }
    }
    mMapComposite->synch(); //
    return 1;
}

MapInfo *CombinedCellMaps::getCombinedMap()
{
    QString mapFilePath(QLatin1String("<LotFilesManagerMap>"));
    Map *map = new Map(Map::LevelIsometric, mCellsWidth * CELL_WIDTH, mCellsHeight * CELL_HEIGHT, 64, 32);
    MapInfo *mapInfo = new MapInfo(map);
    mapInfo->setFilePath(mapFilePath);
    return mapInfo;
}

void CombinedCellMaps::moveToThread(MapComposite *mapComposite, QThread *thread)
{
    mapComposite->moveToThread(thread);
    for (MapComposite *subMap : mapComposite->subMaps()) {
        moveToThread(subMap, thread);
    }
}

QRect CombinedCellMaps::toCellRect256(const QRect &cellRect300)
{
    int minCell256X = std::floor(cellRect300.x() * CELL_WIDTH / float(CELL_SIZE_256));
    int minCell256Y = std::floor(cellRect300.y() * CELL_HEIGHT / float(CELL_SIZE_256));
    int maxCell256X = std::ceil(((cellRect300.right() + 1) * CELL_WIDTH - 1) / float(CELL_SIZE_256));
    int maxCell256Y = std::ceil(((cellRect300.bottom() + 1) * CELL_HEIGHT - 1) / float(CELL_SIZE_256));
    return QRect(minCell256X, minCell256Y, maxCell256X - minCell256X, maxCell256Y - minCell256Y);
}

QRect CombinedCellMaps::toCellRect300(const QRect &cellRect256)
{
    int minCell300X = std::floor(cellRect256.x() * CELL_SIZE_256 / float(CELL_WIDTH));
    int minCell300Y = std::floor(cellRect256.y() * CELL_SIZE_256 / float(CELL_HEIGHT));
    int maxCell300X = std::ceil(((cellRect256.right() + 1) * CELL_SIZE_256 - 1) / float(CELL_WIDTH));
    int maxCell300Y = std::ceil(((cellRect256.bottom() + 1) * CELL_SIZE_256 - 1) / float(CELL_HEIGHT));
    return QRect(minCell300X, minCell300Y, maxCell300X - minCell300X, maxCell300Y - minCell300Y);
}
