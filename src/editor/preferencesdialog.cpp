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

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "preferences.h"
#include "tilemetainfomgr.h"

#include <QFileDialog>
#include <QStringList>

PreferencesDialog::PreferencesDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PreferencesDialog)
    , mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    Preferences *prefs = Preferences::instance();

    mTilesDirectory = prefs->tilesDirectory();
    ui->tilesDirectory->setText(QDir::toNativeSeparators(mTilesDirectory));
    connect(ui->browseTilesDirectory, SIGNAL(clicked()),
            SLOT(browseTilesDirectory()));

    QString configPath = prefs->configPath();
    ui->configDirectory->setText(QDir::toNativeSeparators(configPath));

    mGridColor = prefs->gridColor();
    ui->gridColor->setColor(mGridColor);
    connect(ui->gridColor, SIGNAL(colorChanged(QColor)),
            SLOT(gridColorChanged(QColor)));

    ui->openGL->setChecked(prefs->useOpenGL());
    ui->thumbnails->setChecked(prefs->worldThumbnails());
    ui->showAdjacent->setChecked(prefs->showAdjacentMaps());
}

void PreferencesDialog::browseTilesDirectory()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Tiles Directory"),
                                                  ui->tilesDirectory->text());
    if (!f.isEmpty()) {
        mTilesDirectory = f;
        ui->tilesDirectory->setText(QDir::toNativeSeparators(f));
    }
}

void PreferencesDialog::gridColorChanged(const QColor &gridColor)
{
    mGridColor = gridColor;
}

void PreferencesDialog::accept()
{
    Preferences *prefs = Preferences::instance();
    Tiled::TileMetaInfoMgr::instance()->changeTilesDirectory(mTilesDirectory);
    prefs->setUseOpenGL(ui->openGL->isChecked());
    prefs->setWorldThumbnails(ui->thumbnails->isChecked());
    prefs->setGridColor(mGridColor);
    prefs->setShowAdjacentMaps(ui->showAdjacent->isChecked());

    QDialog::accept();
}
