/*
 * Copyright 2018, Tim Baker <treectrl@users.sf.net>
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

#include "searchdock.h"
#include "ui_searchdock.h"

#include "celldocument.h"
#include "mainwindow.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QListWidget>
#include <QListWidgetItem>

SearchDock::SearchDock(QWidget* parent)
    : QDockWidget(parent)
    , ui(new Ui::SearchDock)
{
    ui->setupUi(this);

    setObjectName(QLatin1String("SearchDock"));

    connect(ui->combo1, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated1);
    connect(ui->combo2, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated2);
    connect(ui->listWidget, &QListWidget::activated, this, &SearchDock::listActivated);
}

void SearchDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void SearchDock::retranslateUi()
{
    setWindowTitle(tr("Search"));
}

void SearchDock::setDocument(Document *doc)
{
    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    setEnabled(true);
}

void SearchDock::clearDocument()
{
    mWorldDoc = 0;
    mCellDoc = 0;
    ui->listWidget->clear();
    setEnabled(false);
}

void SearchDock::comboActivated1(int index)
{
    World* world = mWorldDoc ? mWorldDoc->world() : mCellDoc->world();

    ui->combo2->clear();

    if (index == 0) {
        for (ObjectType* objType : world->objectTypes()) {
            if (objType->isNull())
                ui->combo2->addItem(QLatin1String("<None>"), QVariant::fromValue(objType));
            else
                ui->combo2->addItem(objType->name(), QVariant::fromValue(objType));
        }
        ui->combo2->setCurrentIndex(0);
    }
}

void SearchDock::comboActivated2(int index)
{
    search();
}

void SearchDock::search()
{
    World* world = mWorldDoc ? mWorldDoc->world() : mCellDoc->world();

    ObjectType* objType = ui->combo2->currentData().value<ObjectType*>();

    ui->listWidget->clear();

    for (int y = 0; y < world->height(); y++) {
        for (int x = 0; x < world->width(); x++) {
            WorldCell* cell = world->cellAt(x, y);
            if (cell == nullptr)
                continue;
            for (WorldCellObject* obj : cell->objects()) {
                if (obj->type() == objType) {
                    QListWidgetItem* item = new QListWidgetItem(
                                tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
                    item->setData(Qt::UserRole, QVariant::fromValue(cell));
                    ui->listWidget->addItem(item);
                    break;
                }
            }
        }
    }
}

void SearchDock::listActivated(const QModelIndex &index)
{
    QListWidgetItem* item = ui->listWidget->item(index.row());
    WorldCell* cell = item->data(Qt::UserRole).value<WorldCell*>();
    WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
    worldDoc->setSelectedCells(QList<WorldCell*>() << cell);
    // TODO: scroll into view
    worldDoc->editCell(cell);
}
