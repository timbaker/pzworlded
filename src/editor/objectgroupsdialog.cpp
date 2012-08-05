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

#include "objectgroupsdialog.h"
#include "ui_objectgroupsdialog.h"

#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

ObjectGroupsDialog::ObjectGroupsDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectGroupsDialog)
    , mWorldDoc(worldDoc)
    , mObjGroup(0)
    , mSelectedRow(-1)
    , mSynching(false)
{
    ui->setupUi(this);

    connect(ui->view, SIGNAL(itemSelectionChanged()), SLOT(selectionChanged()));
    connect(ui->addButton, SIGNAL(clicked()), SLOT(add()));
    connect(ui->updateButton, SIGNAL(clicked()), SLOT(update()));
    connect(ui->removeButton, SIGNAL(clicked()), SLOT(remove()));

    connect(ui->nameEdit, SIGNAL(textChanged(QString)), SLOT(synchButtons()));

    connect(ui->moveDown, SIGNAL(clicked()), SLOT(moveGroupDown()));
    connect(ui->moveUp, SIGNAL(clicked()), SLOT(moveGroupUp()));

    connect(ui->colorButton, SIGNAL(colorChanged(QColor)),
            SLOT(colorChanged(QColor)));

    setList();

    synchButtons();
}

void ObjectGroupsDialog::selectionChanged()
{
    // This is to avoid a race condition when deleting an item.
    // row() returns the row *before* the item is deleted.
    if (mSynching)
        return;

    QListWidget *view = ui->view;
    QList<QListWidgetItem*> selection = view->selectedItems();
    if (selection.size() == 1) {
        mItem = selection.first();
        mSelectedRow = view->row(mItem);
        WorldObjectGroup *og = mItemToGroup[mItem];
        ui->nameEdit->setText(og->name());
        mObjGroup = og;
    } else {
        ui->nameEdit->clear();
        mObjGroup = 0;
        mItem = 0;
        mSelectedRow = -1;
    }

    synchButtons();
}

void ObjectGroupsDialog::add()
{
    QString name = ui->nameEdit->text();
    WorldObjectGroup *og = new WorldObjectGroup(name);

    mWorldDoc->addObjectGroup(og);

    setList();

    // If nothing was selected...
    ui->nameEdit->clear();

    synchButtons();
}

void ObjectGroupsDialog::update()
{
    Q_ASSERT(mObjGroup && mItem);

    mWorldDoc->changeObjectGroupName(mObjGroup,
                                     ui->nameEdit->text());

    mItem->setData(Qt::DisplayRole, mObjGroup->name());

    synchButtons();
}

void ObjectGroupsDialog::remove()
{
    Q_ASSERT(mObjGroup && mItem);
    if (mWorldDoc->removeObjectGroup(mObjGroup)) {
        mSynching = true;
        delete mItem; // removes it from view
        mSynching = false;

        // Another row may have been selected during deletion; deselect it
        clearUI();
    }
}

void ObjectGroupsDialog::clearUI()
{
    ui->view->clearSelection();

    ui->nameEdit->clear();
    mObjGroup = 0;
    mItem = 0;
    mSelectedRow = -1;

    synchButtons();
}

void ObjectGroupsDialog::synchButtons()
{
    bool enableUpdate = false;
    bool enableAdd = false;
    bool enableRemove = false;

    if (mObjGroup) {
        if (ui->nameEdit->text() != mObjGroup->name()) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    QString name = ui->nameEdit->text();
    if (!name.isEmpty()) {
        enableAdd = !mWorldDoc->world()->objectGroups().contains(name);
    }

    ui->addButton->setEnabled(enableAdd);
    ui->updateButton->setEnabled(enableUpdate);
    ui->removeButton->setEnabled(enableRemove);

    ui->moveUp->setEnabled(mObjGroup && (mSelectedRow > 0));
    ui->moveDown->setEnabled(mObjGroup &&
                             (mSelectedRow + 1 < ui->view->count()));

    ui->colorButton->setEnabled(mObjGroup != 0);
    ui->colorButton->setColor(mObjGroup
                              ? mObjGroup->color()
                              : WorldObjectGroup::defaultColor());

    ui->addButton->setDefault(enableAdd);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(!enableAdd);
}

void ObjectGroupsDialog::moveGroupUp()
{
    int index = mWorldDoc->world()->objectGroups().indexOf(mObjGroup);
    mWorldDoc->reorderObjectGroup(mObjGroup, index + 1);

    int row = mSelectedRow;
    setList();
    ui->view->setCurrentRow(row - 1, QItemSelectionModel::Select);
    synchButtons();
}

void ObjectGroupsDialog::moveGroupDown()
{
    int index = mWorldDoc->world()->objectGroups().indexOf(mObjGroup);
    mWorldDoc->reorderObjectGroup(mObjGroup, index - 1);

    int row = mSelectedRow;
    setList();
    ui->view->setCurrentRow(row + 1, QItemSelectionModel::Select);
    synchButtons();
}

void ObjectGroupsDialog::colorChanged(const QColor &color)
{
    if (mObjGroup && mObjGroup->color() != color) {
        mWorldDoc->changeObjectGroupColor(mObjGroup, color);
        mItem->setData(Qt::DecorationRole, color);
    }
}

void ObjectGroupsDialog::setList()
{
    QListWidget *view = ui->view;
    view->clear();
    foreach (WorldObjectGroup *og, mWorldDoc->world()->objectGroups()) {
        // Skip the null object group at the start of the list
        if (og->name().isEmpty())
            continue;

        QListWidgetItem *item = new QListWidgetItem(og->name());
        item->setData(Qt::DecorationRole, og->color());
//        item->setData(Qt::UserRole, QVariant::fromValue<WorldObjectGroup*>(og));
        mItemToGroup[item] = og;

        view->insertItem(0, item);
    }
}
