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

#include "propertydefinitionsdialog.h"
#include "ui_propertiesdialog.h"

#include "worlddocument.h"
#include "world.h"

PropertyDefinitionsDialog::PropertyDefinitionsDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PropertyDefinitionsDialog)
    , mWorldDoc(worldDoc)
    , mDef(0)
    , mSynching(false)
{
    ui->setupUi(this);

    connect(ui->definitionsList, SIGNAL(itemSelectionChanged()), SLOT(definitionSelected()));
    connect(ui->addDefButton, SIGNAL(clicked()), SLOT(addDefinition()));
    connect(ui->updateDefButton, SIGNAL(clicked()), SLOT(updateDefinition()));
    connect(ui->removeDefButton, SIGNAL(clicked()), SLOT(removeDefinition()));
    connect(ui->clearDefButton, SIGNAL(clicked()), SLOT(clearUI()));

    connect(ui->defNameEdit, SIGNAL(textChanged(QString)), SLOT(synchButtons()));
    connect(ui->defDefaultEdit, SIGNAL(textChanged(QString)), SLOT(synchButtons()));
    connect(ui->defDescEdit, SIGNAL(textChanged()), SLOT(synchButtons()));

    setList();

    synchButtons();
}

void PropertyDefinitionsDialog::definitionSelected()
{
    // This is to avoid a race condition when deleting an item.
    // indexOfTopLevelItem() returns the row *before* the item
    // is deleted.
    if (mSynching)
        return;

    QTreeWidget *view = ui->definitionsList;
    QList<QTreeWidgetItem*> selection = view->selectedItems();
    if (selection.size() == 1) {
        mItem = selection.first();
        int row = view->indexOfTopLevelItem(mItem);
        PropertyDef *pd = mWorldDoc->world()->propertyDefinitions().sorted().at(row);
        ui->defNameEdit->setText(pd->mName);
        ui->defDefaultEdit->setText(pd->mDefaultValue);
        ui->defDescEdit->clear();
        ui->defDescEdit->insertPlainText(pd->mDescription);
        mDef = pd;
    } else {
        ui->defNameEdit->clear();
        ui->defDefaultEdit->clear();
        ui->defDescEdit->clear();
        mDef = 0;
        mItem = 0;
    }

    ui->defDescEdit->document()->clearUndoRedoStacks();
    synchButtons();
}

void PropertyDefinitionsDialog::addDefinition()
{
    PropertyDef *pd = new PropertyDef();
    pd->mName = ui->defNameEdit->text();
    pd->mDefaultValue = ui->defDefaultEdit->text();
    pd->mDescription = ui->defDescEdit->toPlainText();

    mWorldDoc->addPropertyDefinition(pd);

    setList();

    // If nothing was selected...
    ui->defNameEdit->clear();
    ui->defDefaultEdit->clear();
    ui->defDescEdit->clear();

    synchButtons();
}

void PropertyDefinitionsDialog::updateDefinition()
{
    Q_ASSERT(mDef && mItem);

    mWorldDoc->changePropertyDefinition(mDef,
                                        ui->defNameEdit->text(),
                                        ui->defDefaultEdit->text(),
                                        ui->defDescEdit->toPlainText());

    PropertyDef *pd = mDef;
    setList();
    int row = mWorldDoc->world()->propertyDefinitions().sorted().indexOf(pd);
    QTreeWidgetItem *item = ui->definitionsList->topLevelItem(row);
    ui->definitionsList->setCurrentItem(item);

    synchButtons();
}

void PropertyDefinitionsDialog::removeDefinition()
{
    Q_ASSERT(mDef && mItem);
    if (mWorldDoc->removePropertyDefinition(mDef)) {
        mSynching = true;
        delete mItem; // removes it from view
        mSynching = false;

        // Another row may have been selected during deletion; deselect it
        clearUI();
    }
}

void PropertyDefinitionsDialog::clearUI()
{
    ui->definitionsList->clearSelection();

    ui->defNameEdit->clear();
    ui->defDefaultEdit->clear();
    ui->defDescEdit->clear();
    mDef = 0;
    mItem = 0;

    synchButtons();
}

void PropertyDefinitionsDialog::synchButtons()
{
    bool enableUpdate = false;
    bool enableAdd = false;
    bool enableRemove = false;

    if (mDef) {
        if (ui->defNameEdit->text() != mDef->mName ||
                ui->defDefaultEdit->text() != mDef->mDefaultValue ||
                ui->defDescEdit->toPlainText() != mDef->mDescription) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    QString name = ui->defNameEdit->text();
    if (!name.isEmpty()) {
        enableAdd = true;
        foreach (PropertyDef *pd, mWorldDoc->world()->propertyDefinitions()) {
            if (name == pd->mName) {
                enableAdd = false;
                break;
             }
        }
    }

    ui->addDefButton->setEnabled(enableAdd);
    ui->updateDefButton->setEnabled(enableUpdate);
    ui->removeDefButton->setEnabled(enableRemove);

    ui->addDefButton->setDefault(enableAdd && !enableUpdate);
    ui->updateDefButton->setDefault(enableUpdate);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(!enableAdd &&
                                                               !enableUpdate);
}

void PropertyDefinitionsDialog::setList()
{
    QTreeWidget *view = ui->definitionsList;
    view->clear();

    foreach (PropertyDef *pd, mWorldDoc->world()->propertyDefinitions().sorted()) {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setData(0, Qt::DisplayRole, pd->mName);
        item->setData(1, Qt::DisplayRole, pd->mDefaultValue);
        view->addTopLevelItem(item);
    }
}
