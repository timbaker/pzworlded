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

#include "gotodialog.h"
#include "ui_gotodialog.h"

#include "world.h"

GoToDialog::GoToDialog(World *world, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GoToDialog)
{
    ui->setupUi(this);

    ui->spinX->setMaximum(world->width() * 300);
    ui->spinY->setMaximum(world->height() * 300);

    ui->rangeX->setText(tr("Min: %1    Max %2").arg(ui->spinX->minimum()).arg(ui->spinX->maximum()));
    ui->rangeY->setText(tr("Min: %1    Max %2").arg(ui->spinY->minimum()).arg(ui->spinY->maximum()));

    xChanged(ui->spinX->value());
    yChanged(ui->spinY->value());

    connect(ui->spinX, SIGNAL(valueChanged(int)), SLOT(xChanged(int)));
    connect(ui->spinY, SIGNAL(valueChanged(int)), SLOT(yChanged(int)));
}

GoToDialog::~GoToDialog()
{
    delete ui;
}

int GoToDialog::worldX() const
{
    return ui->spinX->value();
}

int GoToDialog::worldY() const
{
    return ui->spinY->value();
}

void GoToDialog::xChanged(int val)
{
    ui->cellX->setText(tr("Cell X: %1").arg(val / 300));
}

void GoToDialog::yChanged(int val)
{
    ui->cellY->setText(tr("Cell Y: %1").arg(val / 300));
}
