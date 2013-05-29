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

#include "newworlddialog.h"
#include "ui_newworlddialog.h"

#include <QFileDialog>

NewWorldDialog::NewWorldDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewWorldDialog)
{
    ui->setupUi(this);

    ui->pathWorldGB->setChecked(true);

    connect(ui->osmBrowse, SIGNAL(clicked()), SLOT(osmBrowse()));
}

NewWorldDialog::~NewWorldDialog()
{
    delete ui;
}

QSize NewWorldDialog::worldSize() const
{
    return QSize(ui->widthSpinBox->value(), ui->heightSpinBox->value());
}

bool NewWorldDialog::pathWorld() const
{
    return ui->pathWorldGB->isChecked();
}

QString NewWorldDialog::osmFile() const
{
    return ui->osmEdit->text();
}

void NewWorldDialog::osmBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Import OpenStreetMap"),
                                             ui->osmEdit->text());
    if (!f.isEmpty())
        ui->osmEdit->setText(QDir::toNativeSeparators(f));
}
