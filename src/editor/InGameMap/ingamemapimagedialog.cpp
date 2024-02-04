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

#include "ingamemapimagedialog.h"
#include "ui_ingamemapimagedialog.h"

#include "celldocument.h"
#include "chunkmap.h"
#include "documentmanager.h"
#include "world.h"
#include "worlddocument.h"

#include "BuildingEditor/buildingtiles.h"

#include <QBuffer>
#include <QDebug>
#include <QFileDialog>
#include <QImage>

InGameMapImageDialog::InGameMapImageDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::InGameMapImageDialog)
{
    ui->setupUi(this);

    Document *doc = DocumentManager::instance()->currentDocument();
    World *world = doc->asWorldDocument() ? doc->asWorldDocument()->world() : doc->asCellDocument()->world();
    const GenerateLotsSettings &settings = world->getGenerateLotsSettings();
    ui->inputMapPath->setText(settings.exportDir);

    ui->buttonGroup->setId(ui->radioButton300, 1);
    ui->buttonGroup->setId(ui->radioButton256, 2);
    ui->radioButton256->setChecked(true);

    connect(ui->buttonBrowseMap, &QToolButton::clicked, this, &InGameMapImageDialog::chooseMapDirectory);
    connect(ui->buttonBrowseImage, &QToolButton::clicked, this, &InGameMapImageDialog::chooseOutputFile);
    connect(ui->buttonCreateImage, &QPushButton::clicked, this, &InGameMapImageDialog::clickedTheButton);

    ui->statusLabel->setText(QStringLiteral("Click the button below to begin."));
}

InGameMapImageDialog::~InGameMapImageDialog()
{
    delete ui;
}

void InGameMapImageDialog::chooseMapDirectory()
{
    QString recent = ui->inputMapPath->text();
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose map directory"), recent);
    if (f.isEmpty()) {
        return;
    }
    ui->inputMapPath->setText(QDir::toNativeSeparators(f));
}

void InGameMapImageDialog::chooseOutputFile()
{
    QString f = QFileDialog::getSaveFileName(this, tr("Save PNG As..."),
                                             ui->outputImagePath->text(), QLatin1String("PNG Files (*.png)"));
    if (f.isEmpty()) {
        return;
    }
    ui->outputImagePath->setText(QDir::toNativeSeparators(f));
}

void InGameMapImageDialog::clickedTheButton()
{
    if (mRunning) {
        mStop = true;
        return;
    }

    QString inputPath = ui->inputMapPath->text().trimmed();
    QString outputPath = ui->outputImagePath->text().trimmed();
    if (inputPath.isEmpty() || !QDir(inputPath).exists()) {
        return;
    }
    if (outputPath.isEmpty() || !outputPath.toLower().endsWith(QStringLiteral(".png"))) {
        return;
    }

    mRunning = true;
    ui->buttonCreateImage->setText(QStringLiteral("STOP"));
    createImage();
    ui->buttonCreateImage->setText(QStringLiteral("Create Image"));
    ui->statusLabel->setText(QStringLiteral("Click the button below to begin."));
    mRunning = false;
    mStop = false;
}

void InGameMapImageDialog::createImage()
{
    ui->statusLabel->setText(QStringLiteral("Loading .lotheader files"));
    qApp->processEvents();

    QString inputPath = ui->inputMapPath->text();
    QString outputPath = ui->outputImagePath->text();

    qDeleteAll(IsoLot::InfoHeaders);
    IsoLot::InfoHeaders.clear();
    IsoLot::CellCoordToLotHeader.clear();

    bool b256 = ui->radioButton256->isChecked();
    IsoMetaGrid metaGrid((IsoConstants(b256)));
    metaGrid.Create(inputPath);
    QSize worldSize(metaGrid.maxx - metaGrid.minx + 1, metaGrid.maxy - metaGrid.miny + 1);

    QImage image(worldSize * 300, QImage::Format_RGBA8888);
    image.fill(Qt::gray);

    for (int cy = metaGrid.miny; cy <= metaGrid.maxy; cy++) {
        for (int cx = metaGrid.minx; cx <= metaGrid.maxx; cx++) {
            ui->statusLabel->setText(QStringLiteral("Creating Image (cell %1,%2)").arg(cx).arg(cy));
            qApp->processEvents();
            if (mStop) {
                qDeleteAll(IsoLot::InfoHeaders);
                IsoLot::InfoHeaders.clear();
                IsoLot::CellCoordToLotHeader.clear();
                mStop = false;
                return;
            }
            cellToImage(image, inputPath, metaGrid, cx, cy);
        }
    }

    ui->statusLabel->setText(QStringLiteral("Writing PNG"));
    qApp->processEvents();
    image.save(outputPath);
}

void InGameMapImageDialog::cellToImage(QImage &image, const QString &mapDirectory, IsoMetaGrid& metaGrid, int cellX, int cellY)
{
    QString filenameheader = QStringLiteral("%1/%2_%3.lotheader").arg(mapDirectory).arg(cellX).arg(cellY);
    if (!IsoLot::InfoHeaders.contains(filenameheader)) {
        return;
    }
    LotHeader* header = IsoLot::InfoHeaders[filenameheader];
    QString filenamepack = QStringLiteral("%1/world_%2_%3.lotpack").arg(mapDirectory).arg(cellX).arg(cellY);
    QFile file(filenamepack);
    if (!file.open(QFile::ReadOnly)) {
        return;
    }

    QBuffer buffer;
    buffer.open(QBuffer::ReadWrite);
    buffer.write(file.readAll());
    file.close();
    buffer.seek(0);

    QDataStream in(&buffer);
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
        buffer.seek(0);
    }
    bool b256 = ui->radioButton256->isChecked();
    int CHUNKS_PER_CELL = b256 ? 32 : 30;
    int SQUARES_PER_CHUNK = b256 ? 8 : 10;
    int SQUARES_PER_CELL = CHUNKS_PER_CELL * SQUARES_PER_CHUNK;

    for (int chunkY = 0; chunkY < CHUNKS_PER_CELL; chunkY++) {
        for (int chunkX = 0; chunkX < CHUNKS_PER_CELL; chunkX++) {
            int index = chunkX * CHUNKS_PER_CELL + chunkY;
            // Skip 'LOTP' + version + #chunks
            buffer.seek((version >= IsoLot::VERSION1 ? 8 : 0) + 4 + index * 8);
            qint64 pos;
            in >> pos;
            buffer.seek(pos);
            int skip = 0;
            for (int z = header->minLevel; z <= header->maxLevel; ++z) {
                for (int x = 0; x < SQUARES_PER_CHUNK; ++x) {
                    for (int y = 0; y < SQUARES_PER_CHUNK; ++y) {
                        if (skip > 0) {
                            --skip;
                            continue;
                        }
                        int count = IsoLot::readInt(in);
                        if (count == -1) {
                            skip = IsoLot::readInt(in);
                            if (skip > 0) {
                                --skip;
                            }
                            continue;
                        }
                        int roomID = IsoLot::readInt(in);
//                        roomIDs[x][y][z] = roomID;
                        Q_ASSERT(count > 1 && count < 30);
                        for (int n = 1; n < count; ++n) {
                            int tileNameIndex = IsoLot::readInt(in);
//                            this->data[x][y][z] += d;
                            QString tileName = header->tilesUsed[tileNameIndex];
                            BuildingEditor::BuildingTile buildingTile = header->buildingTiles[tileNameIndex];
                            int pixelX = (cellX - metaGrid.minx) * SQUARES_PER_CELL + chunkX * SQUARES_PER_CHUNK + x;
                            int pixelY = (cellY - metaGrid.miny) * SQUARES_PER_CELL + chunkY * SQUARES_PER_CHUNK + y;
                            tileToImage(image, buildingTile, pixelX, pixelY);
                        }
                    }
                }
                break; // z=0 only
            }
        }
    }
}

void InGameMapImageDialog::tileToImage(QImage &image, const BuildingEditor::BuildingTile &buildingTile, int pixelX, int pixelY)
{
    if (buildingTile.mIndex == -1) {
        return; // failed to parse the tile name
    }
    int tileIndex = buildingTile.mIndex;
    if (buildingTile.mTilesetName.contains(QStringLiteral("_trees"))) {
        image.setPixel(pixelX, pixelY, qRgb(38, 53, 22)); // normaltree
    }
    if (buildingTile.mTilesetName.contains(QStringLiteral("jumbo"))) {
        image.setPixel(pixelX, pixelY, qRgb(38, 53, 22)); // jumbotree
    }
    if (buildingTile.mTilesetName.contains(QStringLiteral("_railroad"))) {
        image.setPixel(pixelX, pixelY, qRgb(73, 58, 43)); // rails
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("vegetation"))) {
        image.setPixel(pixelX, pixelY, qRgb(48, 73, 32)); // vegetation
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("blends_natural_01"))) {
        if (tileIndex >= 0 && tileIndex <= 15) {
            image.setPixel(pixelX, pixelY, qRgb(217, 207, 183)); // sand
        }
        if (tileIndex >= 16 && tileIndex <= 31) {
            image.setPixel(pixelX, pixelY, qRgb(75, 88, 27)); // darkgrass
        }
        if (tileIndex >= 32 && tileIndex <= 47) {
            image.setPixel(pixelX, pixelY, qRgb(97, 103, 36)); // medgrass
        }
        if (tileIndex >= 48 && tileIndex <= 63) {
            image.setPixel(pixelX, pixelY, qRgb(127, 120, 45)); // litegrass
        }
        if (tileIndex >= 64 && tileIndex <= 79) {
            image.setPixel(pixelX, pixelY, qRgb(91, 63, 21)); // dirt
        }
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("blends_natural_02"))) {
        image.setPixel(pixelX, pixelY, qRgb(108, 127, 131)); // water
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("blends_street_01"))) {
        image.setPixel(pixelX, pixelY, qRgb(128, 128, 128)); // street
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("floors_exterior_tilesandstone"))) {
        image.setPixel(pixelX, pixelY, qRgb(132, 81, 76)); // tilesand
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("floors_exterior_tilesandwood"))) {
        image.setPixel(pixelX, pixelY, qRgb(132, 81, 76)); // tilesand
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("location_"))) {
        image.setPixel(pixelX, pixelY, qRgb(132, 81, 76)); // tilesand
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("vegetation_farm"))) {
        image.setPixel(pixelX, pixelY, qRgb(218, 165, 32)); // Corn
    }
    if (buildingTile.mTilesetName.startsWith(QStringLiteral("walls_"))) {
        image.setPixel(pixelX, pixelY, qRgb(93, 44, 39)); // walls
    }
}
