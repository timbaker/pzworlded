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

#include "bmptotmx.h"
#include "mainwindow.h"
#include "world.h"
#include "worlddocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>

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
    ui->exportEdit->setText(QDir::toNativeSeparators(mExportDir));
    connect(ui->exportBrowse, SIGNAL(clicked()), SLOT(exportBrowse()));

    // Rules.txt
    mRulesFile = settings.rulesFile;
    if (mRulesFile.isEmpty()) {
        mRulesFile = BMPToTMX::instance()->defaultRulesFile();
        QFileInfo info(mRulesFile);
        if (info.exists())
            mRulesFile = info.canonicalFilePath();
    }
    ui->rulesEdit->setText(QDir::toNativeSeparators(mRulesFile));
    connect(ui->rulesBrowse, SIGNAL(clicked()), SLOT(rulesBrowse()));

    // Blends.txt
    mBlendsFile = settings.blendsFile;
    if (mBlendsFile.isEmpty()) {
        mBlendsFile = BMPToTMX::instance()->defaultBlendsFile();
        QFileInfo info(mBlendsFile);
        if (info.exists())
            mBlendsFile = info.canonicalFilePath();
    }
    ui->blendsEdit->setText(QDir::toNativeSeparators(mBlendsFile));
    connect(ui->blendsBrowse, SIGNAL(clicked()), SLOT(blendsBrowse()));

    // MapBaseXML.txt
    mMapBaseFile = settings.mapbaseFile;
    if (mMapBaseFile.isEmpty()) {
        mMapBaseFile = BMPToTMX::instance()->defaultMapBaseXMLFile();
        QFileInfo info(mMapBaseFile);
        if (info.exists())
            mMapBaseFile = info.canonicalFilePath();
    }
    ui->mapbaseEdit->setText(QDir::toNativeSeparators(mMapBaseFile));
    connect(ui->mapbaseBrowse, SIGNAL(clicked()), SLOT(mapbaseBrowse()));

    ui->assignMapCheckBox->setChecked(settings.assignMapsToWorld);
    ui->warnUnknownColors->setChecked(settings.warnUnknownColors);
    ui->compress->setChecked(settings.compress);
    ui->copyPixels->setChecked(settings.copyPixels);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
            SLOT(apply()));
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
        ui->exportEdit->setText(QDir::toNativeSeparators(mExportDir));
    }
}

void BMPToTMXDialog::rulesBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Rules.txt File"),
        ui->rulesEdit->text());
    if (!f.isEmpty()) {
        mRulesFile = f;
        ui->rulesEdit->setText(QDir::toNativeSeparators(mRulesFile));
    }
}

void BMPToTMXDialog::blendsBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the Blends.txt File"),
        ui->blendsEdit->text());
    if (!f.isEmpty()) {
        mBlendsFile = f;
        ui->blendsEdit->setText(QDir::toNativeSeparators(mBlendsFile));
    }
}

void BMPToTMXDialog::mapbaseBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose the MapBaseXML.txt File"),
        ui->mapbaseEdit->text());
    if (!f.isEmpty()) {
        mMapBaseFile = f;
        ui->mapbaseEdit->setText(QDir::toNativeSeparators(mMapBaseFile));
    }
}

void BMPToTMXDialog::accept()
{
    if (!validate())
        return;

    if (QFileInfo(mRulesFile) == QFileInfo(BMPToTMX::instance()->defaultRulesFile()))
        mRulesFile.clear();
    if (QFileInfo(mBlendsFile) == QFileInfo(BMPToTMX::instance()->defaultBlendsFile()))
        mBlendsFile.clear();
    if (QFileInfo(mMapBaseFile) == QFileInfo(BMPToTMX::instance()->defaultMapBaseXMLFile()))
        mMapBaseFile.clear();

    BMPToTMXSettings settings;
    settings.exportDir = mExportDir;
    settings.rulesFile = mRulesFile;
    settings.blendsFile = mBlendsFile;
    settings.mapbaseFile = mMapBaseFile;
    settings.assignMapsToWorld = ui->assignMapCheckBox->isChecked();
    settings.warnUnknownColors = ui->warnUnknownColors->isChecked();
    settings.compress = ui->compress->isChecked();
    settings.copyPixels = ui->copyPixels->isChecked();
    if (settings != mWorldDoc->world()->getBMPToTMXSettings())
        mWorldDoc->changeBMPToTMXSettings(settings);

    QDialog::accept();
}

void BMPToTMXDialog::apply()
{
    if (!validate())
        return;

    if (QFileInfo(mRulesFile) == QFileInfo(BMPToTMX::instance()->defaultRulesFile()))
        mRulesFile.clear();
    if (QFileInfo(mBlendsFile) == QFileInfo(BMPToTMX::instance()->defaultBlendsFile()))
        mBlendsFile.clear();
    if (QFileInfo(mMapBaseFile) == QFileInfo(BMPToTMX::instance()->defaultMapBaseXMLFile()))
        mMapBaseFile.clear();

    BMPToTMXSettings settings;
    settings.exportDir = mExportDir;
    settings.rulesFile = mRulesFile;
    settings.blendsFile = mBlendsFile;
    settings.mapbaseFile = mMapBaseFile;
    settings.assignMapsToWorld = ui->assignMapCheckBox->isChecked();
    settings.warnUnknownColors = ui->warnUnknownColors->isChecked();
    settings.compress = ui->compress->isChecked();
    if (settings != mWorldDoc->world()->getBMPToTMXSettings())
        mWorldDoc->changeBMPToTMXSettings(settings);

    QDialog::reject();
}

bool BMPToTMXDialog::validate()
{
    QDir dir(mExportDir);
    if (mExportDir.isEmpty() || !dir.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a valid directory to save the .tmx files in."));
        return false;
    }

    QFileInfo info(mRulesFile);
    if (!info.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a rules file."));
        return false;
    }

    info.setFile(mBlendsFile);
    if (!info.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a blends file."));
        return false;
    }

    info.setFile(mMapBaseFile);
    if (!info.exists()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a map template file."));
        return false;
    }

    return true;
}
