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

#include <qmath.h>

// Modulo with a possibly-negative number
#define NMOD(a,b) ((a) - qFloor((a) / (b)) * (b))

GoToDialog::GoToDialog(World *world, const QPoint &initial, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GoToDialog),
    mWorld(world),
    mSynching(0)
{
    ui->setupUi(this);

    QPoint worldOrigin = world->getGenerateLotsSettings().worldOrigin;

    ui->worldX->setRange(worldOrigin.x() * 300, (worldOrigin.x() + world->width()) * 300);
    ui->worldY->setRange(worldOrigin.y() * 300, (worldOrigin.y() + world->height()) * 300);

    ui->rangeX->setText(tr("Min: %1    Max: %2").arg(ui->worldX->minimum()).arg(ui->worldX->maximum()));
    ui->rangeY->setText(tr("Min: %1    Max: %2").arg(ui->worldY->minimum()).arg(ui->worldY->maximum()));

    ui->cellX->setRange(worldOrigin.x(), worldOrigin.x() + world->width() - 1);
    ui->cellY->setRange(worldOrigin.y(), worldOrigin.y() + world->height() - 1);

    ui->posX->setRange(0, 299);
    ui->posY->setRange(0, 299);

    mSynching++;
    QPoint p = worldOrigin * 300 + initial;
    ui->worldX->setValue(p.x());
    ui->worldY->setValue(p.y());
    ui->cellX->setValue(qFloor(p.x() / 300));
    ui->cellY->setValue(qFloor(p.y() / 300));
    ui->posX->setValue(NMOD(p.x(), 300.0));
    ui->posY->setValue(NMOD(p.y(), 300.0));
    mSynching--;

    connect(ui->worldX, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::worldXChanged);
    connect(ui->worldY, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::worldYChanged);
    connect(ui->cellX, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::cellXChanged);
    connect(ui->cellY, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::cellYChanged);
    connect(ui->posX, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::posXChanged);
    connect(ui->posY, qOverload<int>(&QSpinBox::valueChanged), this, &GoToDialog::posYChanged);
}

GoToDialog::~GoToDialog()
{
    delete ui;
}

int GoToDialog::worldX() const
{
    return ui->worldX->value() - mWorld->getGenerateLotsSettings().worldOrigin.x() * 300;
}

int GoToDialog::worldY() const
{
    return ui->worldY->value() - mWorld->getGenerateLotsSettings().worldOrigin.y() * 300;
}

void GoToDialog::worldXChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->cellX->setValue(qFloor(val / 300.0));
    ui->posX->setValue(NMOD(val, 300.0));
    mSynching--;
}

void GoToDialog::worldYChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->cellY->setValue(qFloor(val / 300.0));
    ui->posY->setValue(NMOD(val, 300.0));
    mSynching--;
}

void GoToDialog::cellXChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->worldX->setValue(val * 300 + ui->posX->value());
    mSynching--;
}

void GoToDialog::cellYChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->worldY->setValue(val * 300 + ui->posY->value());
    mSynching--;
}

void GoToDialog::posXChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->worldX->setValue(ui->cellX->value() * 300 + val);
    mSynching--;
}

void GoToDialog::posYChanged(int val)
{
    if (mSynching) return;
    mSynching++;
    ui->worldY->setValue(ui->cellY->value() * 300 + val);
    mSynching--;
}
