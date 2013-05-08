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

#include "worldgenwindow.h"
#include "ui_worldgenwindow.h"

#include "worldgenview.h"

#include <QFileDialog>

using namespace WorldGen;

WorldGenWindow *WorldGenWindow::mInstance = 0;

WorldGenWindow *WorldGenWindow::instance()
{
    if (!mInstance)
        mInstance = new WorldGenWindow;
    return mInstance;
}

WorldGenWindow::WorldGenWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WorldGenWindow)
{
    ui->setupUi(this);

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(open()));

    connect(ui->actionIncreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthIncr()));
    connect(ui->actionDecreaseDepth, SIGNAL(triggered()),
            ui->view->scene(), SLOT(depthDecr()));

    ui->actionShowGrids->setChecked(true);
    connect(ui->actionShowGrids, SIGNAL(toggled(bool)),
            ui->view->scene(), SLOT(showGrids(bool)));
}

WorldGenWindow::~WorldGenWindow()
{
    delete ui;
}

void WorldGenWindow::open()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Open L-System"),
                                             QLatin1String("C:/Programming/WorldGen/Lse/Examples"));
    if (f.isEmpty())
        return;
    ui->view->LoadFile(f);
}
