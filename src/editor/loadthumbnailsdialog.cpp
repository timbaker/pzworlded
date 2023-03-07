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

#include "loadthumbnailsdialog.h"
#include "ui_loadthumbnailsdialog.h"

#include "worldscene.h"

LoadThumbnailsDialog::LoadThumbnailsDialog(WorldScene *scene, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoadThumbnailsDialog),
    mScene(scene)
{
    ui->setupUi(this);
    connect(ui->pushButton, &QPushButton::clicked, this, &LoadThumbnailsDialog::cancelLoading);
}

LoadThumbnailsDialog::~LoadThumbnailsDialog()
{
    delete ui;
}

void LoadThumbnailsDialog::setPrompt(const QString &text)
{
    ui->label->setText(text);
}

void LoadThumbnailsDialog::cancelLoading()
{
   mScene->cancelLoadingThumbnails();
}
