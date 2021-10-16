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

#include <quazip.h>
#include <quazipfile.h>

#include <QFileDialog>

#if 0
namespace
{

class MipMapLevel
{
public:
    QImage mImage;
};

class ImageData
{
public:
    bool read(const QString& fileName)
    {
        mImage = QImage(fileName);
        if (mImage.isNull()) {
            return false;
        }
        return true;
    }

    int getWidth()
    {
        return mImage.width();
    }

    int getHeight()
    {
        return mImage.height();
    }

    MipMapLevel *getMipMapData(int level)
    {
        MipMapLevel *mipMapLevel = new MipMapLevel();
        return mipMapLevel;
    }

    QImage mImage;
    QList<MipMapLevel*> mMipMaps;
};

// namespace anonymous
}
#endif

InGameMapImagePyramidWindow::InGameMapImagePyramidWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::InGameMapImagePyramidWindow)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, true);
    connect(ui->inputBrowseButton, &QToolButton::clicked, this, &InGameMapImagePyramidWindow::chooseInputFile);
    connect(ui->outputBrowseButton, &QToolButton::clicked, this, &InGameMapImagePyramidWindow::chooseOutputFile);
    connect(ui->createZipButton, &QPushButton::clicked, this, &InGameMapImagePyramidWindow::createZip);
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

    QImage image(ui->inputNameEdit->text());
    if (image.isNull()) {
        log(QStringLiteral("Error reading %1").arg(ui->inputNameEdit->text()));
        return;
    }

    QuaZip zip(ui->outputNameEdit->text());
    if (zip.open(QuaZip::Mode::mdCreate) == false) {
        log(QStringLiteral("Error creating %1").arg(ui->outputNameEdit->text()));
        return;
    }

    int tileSize = 256;
    int levels = 5;
    for (int level = 0; level < levels; level++) {
        float width = float(image.width()) / (1 << level);
        float height = float(image.height()) / (1 << level);
        QImage scaledImage = image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
        return;
    }
    if (image.save(&file, "PNG") == false) {
        log(QStringLiteral("ERROR writing %1 to ZIP file").arg(fileName));
        file.close();
        return;
    }
    file.close();
}

void InGameMapImagePyramidWindow::log(const QString &str)
{
    ui->logText->appendPlainText(str);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}
