/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "writespawnpointsdialog.h"
#include "ui_writespawnpointsdialog.h"

#include "world.h"
#include "worlddocument.h"

#include <QDir>
#include <QFileDialog>

WriteSpawnPointsDialog::WriteSpawnPointsDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteSpawnPointsDialog),
    mDocument(worldDoc)
{
    ui->setupUi(this);

    mSpawnPointsFileName = mDocument->world()->getLuaSettings().spawnPointsFile;
    ui->fileName->setText(QDir::toNativeSeparators(mSpawnPointsFileName));
    connect(ui->browse, SIGNAL(clicked()), SLOT(browse()));
}

WriteSpawnPointsDialog::~WriteSpawnPointsDialog()
{
    delete ui;
}

void WriteSpawnPointsDialog::browse()
{
    QString fileName = mSpawnPointsFileName;
    if (fileName.isEmpty() && !mDocument->fileName().isEmpty()) {
        QFileInfo info(mDocument->fileName());
        fileName = info.absolutePath() + QLatin1String("/spawnpoints.lua");
    }
    QString f = QFileDialog::getSaveFileName(this, tr("Save Spawn Points As"),
                                             fileName, tr("LUA files (*.lua)"));
    if (f.isEmpty())
        return;

    mSpawnPointsFileName = f;
    ui->fileName->setText(QDir::toNativeSeparators(f));
}

void WriteSpawnPointsDialog::accept()
{
    LuaSettings settings = mDocument->world()->getLuaSettings();
    settings.spawnPointsFile = mSpawnPointsFileName;
    if (settings != mDocument->world()->getLuaSettings())
        mDocument->changeLuaSettings(settings);

    QDialog::accept();
}
