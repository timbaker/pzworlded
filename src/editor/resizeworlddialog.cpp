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

#include "resizeworlddialog.h"
#include "ui_resizeworlddialog.h"

#include "undoredo.h"
#include "world.h"
#include "worlddocument.h"

ResizeWorldDialog::ResizeWorldDialog(WorldDocument *doc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResizeWorldDialog),
    mDocument(doc)
{
    ui->setupUi(this);

    ui->oldWidth->setText(QString::number(doc->world()->width()));
    ui->oldHeight->setText(QString::number(doc->world()->height()));

    ui->widthSpinBox->setValue(doc->world()->width());
    ui->heightSpinBox->setValue(doc->world()->height());
}

ResizeWorldDialog::~ResizeWorldDialog()
{
    delete ui;
}

void ResizeWorldDialog::accept()
{
    QSize newSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
    mDocument->resizeWorld(newSize);
    QDialog::accept();
}
