/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "lotpacksearch.h"
#include "ui_lotpacksearch.h"

#include "chunkmap.h"
#include "celldocument.h"
#include "cellscene.h"
#include "choosetiledialog.h"
#include "documentmanager.h"
#include "lotpackwindow.h"
#include "preferences.h"
#include "world.h"
#include "worlddocument.h"
#include "zoomable.h"

#include "maprenderer.h"
#include "tileset.h"

#include <QBuffer>

#include <cmath>

LotPackSearch::LotPackSearch(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LotPackSearch)
{
    ui->setupUi(this);

    connect(ui->buttonAddTile, &QPushButton::clicked, this, &LotPackSearch::addTile);
    connect(ui->buttonRemoveTile, &QPushButton::clicked, this, &LotPackSearch::removeTile);
    connect(ui->buttonClearTiles, &QPushButton::clicked, this, &LotPackSearch::clearTiles);
    connect(ui->buttonSearch, &QPushButton::clicked, this, &LotPackSearch::search);
    connect(ui->buttonOpenCell, &QPushButton::clicked, this, &LotPackSearch::openCell);
    connect(ui->buttonClose, &QPushButton::clicked, this, &QMainWindow::close);

    connect(ui->tileList, &QListWidget::currentItemChanged, this, &LotPackSearch::synchUI);
    connect(ui->resultList, &QListWidget::currentRowChanged, this, &LotPackSearch::currentResultChanged);

    synchUI();
}

LotPackSearch::~LotPackSearch()
{
    delete ui;
}

bool LotPackSearch::isTileAddedAlready(const QString& tileName) const
{
    for (int i = 0; i < ui->tileList->count(); i++) {
        QListWidgetItem *item = ui->tileList->item(i);
        if (item->text() == tileName) {
            return true;
        }
    }
    return false;
}

bool LotPackSearch::containsAny(const QStringList &haystack, const QStringList &needles, QSet<QString> &contains)
{
    contains.clear();
    const QSet<QString> haystackSet(haystack.constBegin(), haystack.constEnd());
    for (const QString &string1 : needles) {
        if (haystackSet.contains(string1)) {
            contains += string1;
        }
    }
    return !contains.isEmpty();
}

void LotPackSearch::searchCell(int cellX, int cellY, LotHeader *lotHeader, const QSet<QString> &tilesToFind)
{
    const QString directory = mLotPackWindow->world()->Directory;
    QString filenamepack = QString::fromLatin1("%1/world_%2_%3.lotpack").arg(directory).arg(cellX).arg(cellY);
    QBuffer *fo = CellLoader::instance()->openLotPackFile(filenamepack);
    if (!fo) {
        return;
    }

    fo->seek(0);
    QDataStream in(fo);
    in.setByteOrder(QDataStream::LittleEndian);

    int version = IsoLot::VERSION0;
    char magic[4] = { 0 };
    in.readRawData(magic, 4);
    if (magic[0] == 'L' && magic[1] == 'O' && magic[2] == 'T' && magic[3] == 'P') {
        version = IsoLot::readInt(in);
        if (version < IsoLot::VERSION0 || version > IsoLot::VERSION_LATEST) {
            return;
        }
    } else {
        fo->seek(0);
    }

    QString tileToFind = QStringLiteral("furniture_seating_indoor_01_8");

    const IsoConstants &isoConstants = mLotPackWindow->world()->isoConstants;
    for (int chunkY = 0; chunkY < isoConstants.CHUNKS_PER_CELL; chunkY++) {
        for (int chunkX = 0; chunkX < isoConstants.CHUNKS_PER_CELL; chunkX++) {
            int index = chunkX * isoConstants.CHUNKS_PER_CELL + chunkY;
            fo->seek((version >= IsoLot::VERSION1 ? 8 : 0) + 4 + index * 8);
            qint64 pos;
            in >> pos;
            fo->seek(pos);
            int skip = 0;
            for (int z = lotHeader->minLevel; z <= lotHeader->maxLevel; ++z) {
                for (int x = 0; x < isoConstants.SQUARES_PER_CHUNK; ++x) {
                    for (int y = 0; y < isoConstants.SQUARES_PER_CHUNK; ++y) {
                        if (skip > 0) {
                            --skip;
                        } else {
                            int count = IsoLot::readInt(in);
                            if (count == -1) {
                                skip = IsoLot::readInt(in);
                                if (skip > 0) {
                                    --skip;
                                }
                            }  else {
                                int room = IsoLot::readInt(in);
                                Q_UNUSED(room)
                                //roomIDs[x][y][z - info->minLevel] = room;
                                Q_ASSERT(count > 1 && count < 30);
                                for (int n = 1; n < count; ++n) {
                                    int usedTileIndex = IsoLot::readInt(in);
                                    //this->data[x][y][z - info->minLevel] += d;
                                    QString usedTileName = lotHeader->tilesUsed[usedTileIndex];
                                    if (tilesToFind.contains(usedTileName)) {
                                        addResult(usedTileName, cellX, cellY, chunkX * isoConstants.SQUARES_PER_CHUNK + x, chunkY * isoConstants.SQUARES_PER_CHUNK + y, z);
                                    }
                                }
                            }
                        }
                    }
                }
            }

        }
    }
}

void LotPackSearch::addResult(const QString &tileName, int cellX, int cellY, int x, int y, int z)
{
    const IsoConstants &isoConstants = mLotPackWindow->world()->isoConstants;
    int squareX = cellX * isoConstants.SQUARES_PER_CELL + x;
    int squareY = cellY * isoConstants.SQUARES_PER_CELL + y;
    mResults += { tileName, squareX, squareY, z };
    int cell300X = std::floor(squareX / 300.0);
    int cell300Y = std::floor(squareY / 300.0);
    const QString s = QStringLiteral("%1 in cell %2,%3 at %4,%5,%6").arg(tileName).arg(cell300X).arg(cell300Y).arg(squareX - cell300X * 300).arg(squareY - cell300Y * 300).arg(z);
    ui->resultList->addItem(s);
}

void LotPackSearch::synchUI()
{
    ui->buttonRemoveTile->setEnabled(ui->tileList->currentItem() != nullptr);
    ui->buttonClearTiles->setEnabled(ui->tileList->count() > 0);
    ui->buttonSearch->setEnabled(ui->tileList->count() > 0);
    ui->buttonOpenCell->setEnabled(ui->resultList->currentItem() != nullptr);
}

void LotPackSearch::addTile()
{
    ChooseTileDialog dialog;
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QList<Tiled::Tile*> tiles = dialog.chosenTiles();
    if (tiles.isEmpty()) {
        return;
    }
    for (Tiled::Tile *tile : tiles) {
        const QString tileName = QStringLiteral("%1_%2").arg(tile->tileset()->name()).arg(tile->id());
        if (isTileAddedAlready(tileName)) {
            continue;
        }
        ui->tileList->addItem(tileName);
    }
    synchUI();
}

void LotPackSearch::removeTile()
{
    int row = ui->tileList->currentRow();
    if (row == -1) {
        return;
    }
    delete ui->tileList->takeItem(row);
    synchUI();
}

void LotPackSearch::clearTiles()
{
    ui->tileList->clear();
    synchUI();
}

void LotPackSearch::search()
{
    ui->resultList->clear();
    mResults.clear();

    QStringList tilesToFind;
    for (int i = 0; i < ui->tileList->count(); i++) {
        tilesToFind += ui->tileList->item(i)->text();
    }

    IsoMetaGrid *metaGrid = mLotPackWindow->world()->MetaGrid;
    for (int y = metaGrid->miny; y <= metaGrid->maxy; y++) {
        for (int x = metaGrid->minx; x <= metaGrid->maxx; x++) {
            auto it = IsoLot::CellCoordToLotHeader.find(IsoLot::CellCoord(x, y));
            if (it == IsoLot::CellCoordToLotHeader.end()) {
                continue;
            }
            LotHeader* lotHeader = it.value();
            QSet<QString> contains;
            if (containsAny(lotHeader->tilesUsed, tilesToFind, contains)) {
                searchCell(x, y, lotHeader, contains);
                qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    }
}

void LotPackSearch::openCell()
{
    int row = ui->resultList->currentRow();
    const SearchResult &result = mResults.at(row);
    Document *doc = DocumentManager::instance()->currentDocument();
    if (doc == nullptr) {
        return;
    }
    WorldDocument *worldDoc = doc->isWorldDocument() ? doc->asWorldDocument() : doc->asCellDocument()->worldDocument();
    int cell300X = std::floor(result.x / 300.0);
    int cell300Y = std::floor(result.y / 300.0);
    WorldCell *cell = worldDoc->world()->cellAt(cell300X, cell300Y);
    if (cell == nullptr) {
        return;
    }
    worldDoc->editCell(cell);
    CellDocument *cellDoc = DocumentManager::instance()->findDocument(cell);
    if (cellDoc == nullptr) {
        return;
    }
    DocumentManager::instance()->setCurrentDocument(cellDoc);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    QPointF tilePos(result.x - cell300X * 300, result.y - cell300Y * 300);
    cellDoc->view()->centerOn(cellDoc->scene()->renderer()->tileToPixelCoords(tilePos, result.z));
    Preferences::instance()->setHighlightCurrentLevel(true);
    cellDoc->setCurrentLevel(result.z);
    cellDoc->scene()->setHighlightRoomPosition({result.x - cell300X * 300, result.y - cell300Y * 300});
    cellDoc->view()->zoomable()->setScale(1.0);
}

void LotPackSearch::currentResultChanged(int row)
{
    if (row == -1) {
        return;
    }
    const SearchResult &result = mResults.at(row);
    mLotPackWindow->focusOn(result.x, result.y, result.z);
    synchUI();
}
