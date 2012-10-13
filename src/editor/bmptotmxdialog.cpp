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

#include "bmptotmxdialog.h"
#include "ui_bmptotmxdialog.h"

#include "world.h"
#include "worlddocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

BMPToTMXDialog::BMPToTMXDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BMPToTMXDialog),
    mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    const BMPToTMXSettings &settings = worldDoc->world()->getBMPToTMXSettings();

    // Export directory
    mExportDir = settings.exportDir;
    if (mExportDir.isEmpty() && !worldDoc->fileName().isEmpty()) {
        QFileInfo info(worldDoc->fileName());
        mExportDir = info.absolutePath() + QLatin1Char('/')
                + QLatin1String("tmxexport");
        info.setFile(mExportDir);
        if (info.exists())
            mExportDir = info.canonicalFilePath();
    }
    ui->exportEdit->setText(mExportDir);
    connect(ui->exportBrowse, SIGNAL(clicked()), SLOT(exportBrowse()));

    // Rules.txt
    mRulesFile = settings.rulesFile;
    if (mRulesFile.isEmpty()) {
        mRulesFile = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                + QLatin1String("Rules.txt");
        QFileInfo info(mRulesFile);
        if (info.exists())
            mRulesFile = info.canonicalFilePath();
    }
    ui->rulesEdit->setText(mRulesFile);
    connect(ui->rulesBrowse, SIGNAL(clicked()), SLOT(rulesBrowse()));

    // Blends.txt
    mBlendsFile = settings.blendsFile;
    if (mBlendsFile.isEmpty()) {
        mBlendsFile = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                + QLatin1String("Blends.txt");
        QFileInfo info(mBlendsFile);
        if (info.exists())
            mBlendsFile = info.canonicalFilePath();
    }
    ui->blendsEdit->setText(mBlendsFile);
    connect(ui->blendsBrowse, SIGNAL(clicked()), SLOT(blendsBrowse()));

    // MapBaseXML.txt
    mMapBaseFile = settings.mapbaseFile;
    if (mMapBaseFile.isEmpty()) {
        mMapBaseFile = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                + QLatin1String("MapBaseXML.txt");
        QFileInfo info(mMapBaseFile);
        if (info.exists())
            mMapBaseFile = info.canonicalFilePath();
    }
    ui->mapbaseEdit->setText(mMapBaseFile);
    connect(ui->mapbaseBrowse, SIGNAL(clicked()), SLOT(mapbaseBrowse()));

    ui->assignMapCheckBox->setChecked(settings.assignMapsToWorld);
}

BMPToTMXDialog::~BMPToTMXDialog()
{
    delete ui;
}

void BMPToTMXDialog::exportBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose the BMP to TMX Folder"),
        ui->exportEdit->text());
    if (!f.isEmpty()) {
        mExportDir = f;
        ui->exportEdit->setText(mExportDir);
    }
}

void BMPToTMXDialog::rulesBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Rules.txt File"),
        ui->rulesEdit->text());
    if (!f.isEmpty()) {
        mRulesFile = f;
        ui->rulesEdit->setText(mRulesFile);
    }
}

void BMPToTMXDialog::blendsBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Blends.txt File"),
        ui->blendsEdit->text());
    if (!f.isEmpty()) {
        mBlendsFile = f;
        ui->blendsEdit->setText(mBlendsFile);
    }
}

void BMPToTMXDialog::mapbaseBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the MapBaseXML.txt File"),
        ui->mapbaseEdit->text());
    if (!f.isEmpty()) {
        mMapBaseFile = f;
        ui->mapbaseEdit->setText(mMapBaseFile);
    }
}

void BMPToTMXDialog::accept()
{
    QDir dir(mExportDir);
    if (!dir.exists()) {
        return;
    }
    QFileInfo info(mRulesFile);
    if (!info.exists()) {
        return;
    }
    info.setFile(mBlendsFile);
    if (!info.exists()) {
        return;
    }
    info.setFile(mMapBaseFile);
    if (!info.exists()) {
        return;
    }

    BMPToTMXSettings settings;
    settings.exportDir = mExportDir;
    settings.rulesFile = mRulesFile;
    settings.blendsFile = mBlendsFile;
    settings.mapbaseFile = mMapBaseFile;
    settings.assignMapsToWorld = ui->assignMapCheckBox->isChecked();
    if (settings != mWorldDoc->world()->getBMPToTMXSettings())
        mWorldDoc->changeBMPToTMXSettings(settings);

    QDialog::accept();
}
