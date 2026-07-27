/*
 * Copyright 2021, Tim Baker <treectrl@users.sf.net>
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

#include "ingamemapimagepyramidwindow.h"
#include "ui_ingamemapimagepyramidwindow.h"

#include "celldocument.h"
#include "documentmanager.h"
#include "worlddocument.h"
#include "world.h"

#include <quazip.h>
#include <quazipfile.h>

#include <QFileDialog>
#include <QPainter>

#include <cmath>

InGameMapImagePyramidWindow::InGameMapImagePyramidWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::InGameMapImagePyramidWindow)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);

    connect(ui->inputBrowseButton, &QToolButton::clicked, this, &InGameMapImagePyramidWindow::chooseInputFile);
    connect(ui->outputBrowseButton, &QToolButton::clicked, this, &InGameMapImagePyramidWindow::chooseOutputFile);
    connect(ui->createZipButton, &QPushButton::clicked, this, &InGameMapImagePyramidWindow::createZip);

    if (Document *doc = DocumentManager::instance()->currentDocument()) {
        World *world = nullptr;
        if (doc->asWorldDocument()) {
            world = doc->asWorldDocument()->world();
        } else {
            world = doc->asCellDocument()->world();
        }
        auto settings = world->getGenerateLotsSettings();
        ui->xMin->setValue(settings.worldOrigin.x() * 300);
        ui->yMin->setValue(settings.worldOrigin.y() * 300);
        ui->xMax->setValue((settings.worldOrigin.x() + world->width()) * 300);
        ui->yMax->setValue((settings.worldOrigin.y() + world->height()) * 300);
    }
}

InGameMapImagePyramidWindow::~InGameMapImagePyramidWindow()
{
    delete ui;
}

void InGameMapImagePyramidWindow::chooseInputFile()
{
    QString caption = QStringLiteral("Choose Input Image File");
    QString dir;
    const QString fileName = QFileDialog::getOpenFileName(this, caption, dir, tr("PNG files (*.png)"));
    if (fileName.isEmpty()) {
        return;
    }
    ui->inputNameEdit->setText(fileName);
}

void InGameMapImagePyramidWindow::chooseOutputFile()
{
    QString caption = QStringLiteral("Choose Output ZIP File");
    QString suggestedFileName;
    const QString fileName = QFileDialog::getSaveFileName(this, caption, suggestedFileName, tr("ZIP files (*.zip)"));
    if (fileName.isEmpty()) {
        return;
    }
    ui->outputNameEdit->setText(fileName);
}

void InGameMapImagePyramidWindow::createZip()
{
    ui->logText->clear();
    QString inputFileName = ui->inputNameEdit->text();
    log(QStringLiteral("Reading %1").arg(inputFileName));
    QImage image(inputFileName);
    if (image.isNull()) {
        log(QStringLiteral("Error reading %1").arg(inputFileName));
        return;
    }

    QuaZip zip(ui->outputNameEdit->text());
    if (zip.open(QuaZip::Mode::mdCreate) == false) {
        log(QStringLiteral("Error creating %1").arg(ui->outputNameEdit->text()));
        return;
    }

    QSize size(std::ceil(image.width() / 16.0f) * 16, std::ceil(image.height() / 16.0f) * 16);
    QImage image1(size, image.format());
    image1.fill(Qt::transparent);
    QPainter painter(&image1);
    painter.drawImage(QPoint(), image);
    painter.end();

    int tileSize = 256;
    int levels = 5;
    for (int level = 0; level < levels; level++) {
        float width = float(image1.width()) / (1 << level);
        float height = float(image1.height()) / (1 << level);
        QImage scaledImage = image1.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        int columns = std::ceil(width / tileSize);
        int rows = std::ceil(height / tileSize);
        log(QStringLiteral("Creating images for level %1. width x height = %2 x %3").arg(level).arg(columns).arg(rows));
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < columns; col++) {
                QImage subImage = scaledImage.copy(col * tileSize, row * tileSize, tileSize, tileSize);
                writeImageToZip(zip, subImage, col, row, level);
            }
        }
        if (width <= tileSize && height <= tileSize) {
            break;
        }
    }

    writePyramidTxt(zip, image);

    zip.close();

    log(QStringLiteral("FINISHED."));
}

void InGameMapImagePyramidWindow::writeImageToZip(QuaZip &zip, const QImage &image, int col, int row, int level)
{
    QString fileName = QStringLiteral("%1/tile%2x%3.png").arg(level).arg(col).arg(row);

    log(QStringLiteral("Adding %1 to ZIP").arg(fileName));

//    zip.setCurrentFile(fileName);

    QuaZipFile file(&zip);
    QuaZipNewInfo newInfo(fileName);
    if (file.open(QIODevice::WriteOnly, newInfo) == false) {
        log(QStringLiteral("Error opening %1 in ZIP file").arg(fileName));
        return;
    }
    if (image.save(&file, "PNG") == false) {
        log(QStringLiteral("ERROR writing %1 to ZIP file").arg(fileName));
        file.close();
        return;
    }
    file.close();
}

void InGameMapImagePyramidWindow::writePyramidTxt(QuaZip &zip, const QImage &image)
{
    QString fileName = QStringLiteral("pyramid.txt");
    log(QStringLiteral("Writing %1").arg(fileName));
    QuaZipFile file(&zip);
    QuaZipNewInfo newInfo(fileName);
    if (file.open(QIODevice::WriteOnly, newInfo) == false) {
        log(QStringLiteral("Error opening %1 in ZIP file").arg(fileName));
        return;
    }
    QTextStream ts(&file);
    ts << "VERSION=1\n";
    ts << QStringLiteral("bounds=%1 %2 %3 %4\n").arg(ui->xMin->value()).arg(ui->yMin->value()).arg(ui->xMax->value()).arg(ui->yMax->value());
    ts << QStringLiteral("imageSize=%1 %2").arg(image.size().width()).arg(image.size().height());
    ts.flush();
    file.close();
}

void InGameMapImagePyramidWindow::log(const QString &str)
{
    ui->logText->appendPlainText(str);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}
