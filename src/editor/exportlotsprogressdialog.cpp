/*
 * Copyright 2024, Tim Baker <treectrl@users.sf.net>
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

#include "exportlotsprogressdialog.h"
#include "ui_exportlotsprogressdialog.h"

#include <QGraphicsRectItem>

ExportLotsProgressDialog::ExportLotsProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ExportLotsProgressDialog)
{
    ui->setupUi(this);
    ui->graphicsView->setScene(new QGraphicsScene(ui->graphicsView));
    ui->graphicsView->scene()->setBackgroundBrush(Qt::lightGray);
    ui->prompt->clear();
    connect(ui->buttonCancel, &QPushButton::clicked, this, &ExportLotsProgressDialog::cancel);
}

ExportLotsProgressDialog::~ExportLotsProgressDialog()
{
    delete ui;
}

void ExportLotsProgressDialog::setWorldSize(int widthInCells, int heightInCells)
{
    mWidthInCells = widthInCells;
    mHeightInCells = heightInCells;
    QGraphicsScene *scene = ui->graphicsView->scene();
    int cellSize = 8;
    for (int y = 0; y < mHeightInCells; y++) {
        for (int x = 0; x < mWidthInCells; x++) {
            QRect cellRect(x * cellSize, y * cellSize, cellSize, cellSize);
            QGraphicsRectItem *item = scene->addRect(cellRect, Qt::NoPen, Qt::gray);
            mCellItems += item;
        }
    }
    QRect bounds(0, 0, mWidthInCells * cellSize, mHeightInCells * cellSize);
    scene->setSceneRect(bounds.adjusted(-10, -10, 10, 10));
    ui->graphicsView->fitInView(bounds.adjusted(-10, -10, 10, 10), Qt::KeepAspectRatio);
}

void ExportLotsProgressDialog::setCellStatus(int cellX, int cellY, CellStatus status)
{
    QGraphicsRectItem *item = mCellItems[cellX + cellY * mWidthInCells];
    switch (status) {
    case CellStatus::Missing:
        item->setBrush(Qt::gray);
        break;
    case CellStatus::Pending:
        item->setBrush(Qt::white);
        break;
    case CellStatus::Exported:
        item->setBrush(Qt::green);
        break;
    case CellStatus::Failed:
        item->setBrush(Qt::red);
        break;
    }
}

void ExportLotsProgressDialog::setPrompt(const QString &str)
{
    ui->prompt->setText(str);
}

void ExportLotsProgressDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    int cellSize = 8;
    QRect bounds(0, 0, mWidthInCells * cellSize, mHeightInCells * cellSize);
    ui->graphicsView->fitInView(bounds.adjusted(-10, -10, 10, 10), Qt::KeepAspectRatio);
}

void ExportLotsProgressDialog::cancel()
{
    emit cancelled();
}
