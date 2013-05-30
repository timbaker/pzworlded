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

#include "texturebrowsedialog.h"
#include "ui_texturebrowsedialog.h"

TextureBrowseDialog *TextureBrowseDialog::mInstance = 0;

TextureBrowseDialog *TextureBrowseDialog::instance()
{
    if (!mInstance)
        mInstance = new TextureBrowseDialog;
    return mInstance;
}

void TextureBrowseDialog::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

TextureBrowseDialog::TextureBrowseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TextureBrowseDialog)
{
    ui->setupUi(this);

    connect(ui->tableView, SIGNAL(activated(QModelIndex)), SLOT(accept()));
}

TextureBrowseDialog::~TextureBrowseDialog()
{
    delete ui;
}


void TextureBrowseDialog::setTextures(const QList<WorldTexture *> &textures)
{
    ui->tableView->setTextures(textures);
    if (textures.size())
        ui->tableView->setCurrentIndex(ui->tableView->model()->indexOfTexture(textures.first()));
}

void TextureBrowseDialog::setCurrentTexture(WorldTexture *tex)
{
    ui->tableView->setCurrentIndex(ui->tableView->model()->indexOfTexture(tex));
}

WorldTexture *TextureBrowseDialog::chosen()
{
    return ui->tableView->model()->textureAt(ui->tableView->currentIndex());
}
