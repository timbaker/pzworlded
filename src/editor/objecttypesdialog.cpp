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

#include "objecttypesdialog.h"
#include "ui_objecttypesdialog.h"

#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

ObjectTypesDialog::ObjectTypesDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectTypesDialog)
    , mWorldDoc(worldDoc)
    , mObjType(0)
    , mSynching(false)
{
    ui->setupUi(this);

    connect(ui->typesView, SIGNAL(itemSelectionChanged()), SLOT(typeSelected()));
    connect(ui->addButton, SIGNAL(clicked()), SLOT(add()));
    connect(ui->updateButton, SIGNAL(clicked()), SLOT(update()));
    connect(ui->removeButton, SIGNAL(clicked()), SLOT(remove()));

    connect(ui->nameEdit, SIGNAL(textChanged(QString)), SLOT(synchButtons()));

    setList();

    synchButtons();
}

void ObjectTypesDialog::typeSelected()
{
    // This is to avoid a race condition when deleting an item.
    // row() returns the row *before* the item is deleted.
    if (mSynching)
        return;

    QListWidget *view = ui->typesView;
    QList<QListWidgetItem*> selection = view->selectedItems();
    if (selection.size() == 1) {
        mItem = selection.first();
        int row = view->row(mItem);
        ObjectType *ot = mWorldDoc->world()->objectTypes().at(row + 1);
        ui->nameEdit->setText(ot->name());
        mObjType = ot;
    } else {
        ui->nameEdit->clear();
        mObjType = 0;
        mItem = 0;
    }

    synchButtons();
}

void ObjectTypesDialog::add()
{
    QString name = ui->nameEdit->text();
    ObjectType *ot = new ObjectType(name);

    mWorldDoc->addObjectType(ot);

    setList();

    // If nothing was selected...
    ui->nameEdit->clear();

    synchButtons();
}

void ObjectTypesDialog::update()
{
    Q_ASSERT(mObjType && mItem);

    mWorldDoc->changeObjectTypeName(mObjType,
                                 ui->nameEdit->text());

    mItem->setData(Qt::DisplayRole, mObjType->name());

    synchButtons();
}

void ObjectTypesDialog::remove()
{
    Q_ASSERT(mObjType && mItem);
    if (mWorldDoc->removeObjectType(mObjType)) {
        mSynching = true;
        delete mItem; // removes it from view
        mSynching = false;

        // Another row may have been selected during deletion; deselect it
        clearUI();
    }
}

void ObjectTypesDialog::clearUI()
{
    ui->typesView->clearSelection();

    ui->nameEdit->clear();
    mObjType = 0;
    mItem = 0;

    synchButtons();
}

void ObjectTypesDialog::synchButtons()
{
    bool enableUpdate = false;
    bool enableAdd = false;
    bool enableRemove = false;

    if (mObjType) {
        if (ui->nameEdit->text() != mObjType->name()) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    QString name = ui->nameEdit->text();
    if (!name.isEmpty()) {
        enableAdd = !mWorldDoc->world()->objectTypes().contains(name);
    }

    ui->addButton->setEnabled(enableAdd);
    ui->updateButton->setEnabled(enableUpdate);
    ui->removeButton->setEnabled(enableRemove);

    ui->addButton->setDefault(enableAdd);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(!enableAdd);
}

void ObjectTypesDialog::setList()
{
    QListWidget *view = ui->typesView;
    view->clear();
    // Skip the null object type at the start of the list
    view->insertItems(0, mWorldDoc->world()->objectTypes().names().mid(1));
}
