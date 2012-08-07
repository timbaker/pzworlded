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

#include "copypastedialog.h"
#include "ui_copypastedialog.h"

#include "celldocument.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QFileInfo>

CopyPasteDialog::CopyPasteDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CopyPasteDialog)
    , mWorldDoc(worldDoc)
    , mCellDoc(0)
    , mWorld(worldDoc->world())
{
    ui->setupUi(this);

    setup();
}

CopyPasteDialog::CopyPasteDialog(CellDocument *cellDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CopyPasteDialog)
    , mWorldDoc(cellDoc->worldDocument())
    , mCellDoc(cellDoc)
    , mWorld(mWorldDoc->world())
{
    ui->setupUi(this);

    setup();
}

void CopyPasteDialog::setup()
{
    ///// WORLD /////

    connect(ui->worldCat, SIGNAL(currentRowChanged(int)),
            SLOT(worldSelectionChanged(int)));
    connect(ui->worldTree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            SLOT(worldItemChanged(QTreeWidgetItem*,int)));
    connect(ui->worldCheckAll, SIGNAL(clicked()), SLOT(worldCheckAll()));
    connect(ui->worldCheckNone, SIGNAL(clicked()), SLOT(worldCheckNone()));

    foreach (PropertyDef *pd, mWorld->propertyDefinitions())
        mCheckedPropertyDefs[pd] = true;

    ///// CELLS /////

    connect(ui->cellCat, SIGNAL(currentRowChanged(int)),
            SLOT(cellCategoryChanged(int)));

    mCells = mWorldDoc->selectedCells();
    if (mCells.isEmpty()) {
        foreach (WorldCell *cell, mWorld->cells()) {
            if (!cell->isEmpty())
                mCells += cell;
        }
    }
}

///// WORLD /////

void CopyPasteDialog::showPropertyDefs()
{
    ui->worldTree->clear();
    foreach (PropertyDef *pd, mWorld->propertyDefinitions().sorted()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << pd->mName);
        item->setCheckState(0, mCheckedPropertyDefs[pd]
                            ? Qt::Checked : Qt::Unchecked);
        mItemToPropertyDef[item] = pd;
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showTemplates()
{
    ui->worldTree->clear();
    foreach (PropertyTemplate *pt, mWorld->propertyTemplates().sorted()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << pt->mName);
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showObjectTypes()
{
    ui->worldTree->clear();
    foreach (ObjectType *ot, mWorld->objectTypes()) {
        if (ot->isNull())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << ot->name());
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->addTopLevelItem(item);
    }
}

void CopyPasteDialog::showObjectGroups()
{
    ui->worldTree->clear();
    foreach (WorldObjectGroup *og, mWorld->objectGroups()) {
        if (og->isNull())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << og->name());
        item->setCheckState(0, Qt::Checked);
        ui->worldTree->insertTopLevelItem(0, item);
    }
}

void CopyPasteDialog::worldSelectionChanged(int index)
{
    mWorldCat = static_cast<enum WorldCat>(index);
    switch (mWorldCat) {
    case PropertyDefs:
        showPropertyDefs();
        break;
    case Templates:
        showTemplates();
        break;
    case ObjectTypes:
        showObjectTypes();
        break;
    case ObjectGroups:
        showObjectGroups();
        break;
    }
}

void CopyPasteDialog::worldItemChanged(QTreeWidgetItem *item, int column)
{
    switch (mWorldCat) {
    case PropertyDefs:
        mCheckedPropertyDefs[mItemToPropertyDef[item]] = item->checkState(column);
        break;
    case Templates:
        break;
    case ObjectTypes:
        break;
    case ObjectGroups:
        break;
    }
}

void CopyPasteDialog::worldCheckAll()
{
    QTreeWidget *view = ui->worldTree;
    switch (mWorldCat) {
    case PropertyDefs:
    case Templates:
    case ObjectTypes:
    case ObjectGroups:
        for (int i = 0; i < view->topLevelItemCount(); i++)
            view->topLevelItem(i)->setCheckState(0, Qt::Checked);
        break;
    }
}

void CopyPasteDialog::worldCheckNone()
{
    QTreeWidget *view = ui->worldTree;
    switch (mWorldCat) {
    case PropertyDefs:
    case Templates:
    case ObjectTypes:
    case ObjectGroups:
        for (int i = 0; i < view->topLevelItemCount(); i++)
            view->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
        break;
    }
}

///// CELLS /////

void CopyPasteDialog::showCellProperties()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    foreach (WorldCell *cell, mCells) {
        if (cell->properties().isEmpty())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(
                    QStringList() << tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
        item->setCheckState(0, Qt::Checked);
        v->insertTopLevelItem(0, item);
    }
    v->expandAll();
}

void CopyPasteDialog::showCellLots()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    foreach (WorldCell *cell, mCells) {
        if (cell->lots().isEmpty())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(
                    QStringList() << tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
        item->setCheckState(0, Qt::Checked);
        v->insertTopLevelItem(0, item);
        foreach (WorldCellLot *lot, cell->lots()) {
            QTreeWidgetItem *item2 = new QTreeWidgetItem(
                        item, QStringList() << QFileInfo(lot->mapName()).fileName());
            item2->setCheckState(0, Qt::Checked);
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellObjects()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    foreach (WorldCell *cell, mCells) {
        if (cell->objects().isEmpty())
            continue;
        QTreeWidgetItem *item = new QTreeWidgetItem(
                    QStringList() << tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
        item->setCheckState(0, Qt::Checked);

        v->insertTopLevelItem(0, item);
        foreach (WorldCellObject *obj, cell->objects()) {
            QTreeWidgetItem *item2 = new QTreeWidgetItem(
                        item, QStringList() << tr("%1 (%2)").arg(obj->name()).arg(obj->type()->name()));
            item2->setCheckState(0, Qt::Checked);
        }
    }
    v->expandAll();
}

void CopyPasteDialog::showCellMap()
{
    QTreeWidget *v = ui->cellTree;
    v->clear();
    foreach (WorldCell *cell, mCells) {
        if (cell->mapFilePath().isEmpty())
            continue;

        QTreeWidgetItem *item = new QTreeWidgetItem(
                    QStringList() << tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
        item->setCheckState(0, Qt::Checked);
        v->insertTopLevelItem(0, item);

        QTreeWidgetItem *item2 = new QTreeWidgetItem(
                    item, QStringList() << QFileInfo(cell->mapFilePath()).fileName());
        item2->setCheckState(0, Qt::Checked);
    }
    v->expandAll();
}

void CopyPasteDialog::cellCategoryChanged(int index)
{
    mCellCat = static_cast<enum CellCat>(index);
    switch (mCellCat) {
    case Properties:
        showCellProperties();
        break;
    case CellTemplates:
        break;
    case Lots:
        showCellLots();
        break;
    case Objects:
        showCellObjects();
        break;
    case Map:
        showCellMap();
        break;
    }
}

void CopyPasteDialog::cellItemChanged(QTreeWidgetItem *item, int column)
{
}

void CopyPasteDialog::cellCheckAll()
{
}

void CopyPasteDialog::cellCheckNone()
{
}
