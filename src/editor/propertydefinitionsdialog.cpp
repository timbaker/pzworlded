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

#include "propertyenumdialog.h"
#include "undoredo.h"
#include "worlddocument.h"
#include "world.h"

#include <QMessageBox>

PropertyDefinitionsDialog::PropertyDefinitionsDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PropertyDefinitionsDialog)
    , mUndoRedoButtons(new UndoRedoButtons(worldDoc, this))
    , mWorldDoc(worldDoc)
    , mDef(0)
    , mEnum(0)
    , mSynching(0)
{
    ui->setupUi(this);

    ui->buttonsLayout->insertWidget(0, mUndoRedoButtons->undoButton());
    ui->buttonsLayout->insertWidget(1, mUndoRedoButtons->redoButton());

    connect(ui->definitionsList, &QTreeWidget::itemSelectionChanged, this, &PropertyDefinitionsDialog::definitionSelected);
    connect(ui->addDefButton, &QAbstractButton::clicked, this, &PropertyDefinitionsDialog::addDefinition);
    connect(ui->updateDefButton, &QAbstractButton::clicked, this, &PropertyDefinitionsDialog::updateDefinition);
    connect(ui->removeDefButton, &QAbstractButton::clicked, this, &PropertyDefinitionsDialog::removeDefinition);
    connect(ui->clearDefButton, &QAbstractButton::clicked, this, &PropertyDefinitionsDialog::clearUI);

    connect(ui->defNameEdit, &QLineEdit::textChanged, this, &PropertyDefinitionsDialog::synchButtons);
    connect(ui->defDefaultEdit, &QLineEdit::textChanged, this, &PropertyDefinitionsDialog::synchButtons);
    connect(ui->defDescEdit, &QPlainTextEdit::textChanged, this, &PropertyDefinitionsDialog::synchButtons);

    connect(ui->enumCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &PropertyDefinitionsDialog::currentEnumChanged);
    connect(ui->enumEdit, &QAbstractButton::clicked, this, &PropertyDefinitionsDialog::editEnums);

    connect(mWorldDoc, &WorldDocument::propertyDefinitionAdded,
            this, &PropertyDefinitionsDialog::propertyDefinitionAdded);
    connect(mWorldDoc, &WorldDocument::propertyDefinitionAboutToBeRemoved,
            this, &PropertyDefinitionsDialog::propertyDefinitionAboutToBeRemoved);
    connect(mWorldDoc, &WorldDocument::propertyDefinitionChanged,
            this, &PropertyDefinitionsDialog::propertyDefinitionChanged);

    connect(mWorldDoc, &WorldDocument::propertyEnumAdded,
            this, &PropertyDefinitionsDialog::propertyEnumAdded);
    connect(mWorldDoc, &WorldDocument::propertyEnumChanged,
            this, &PropertyDefinitionsDialog::propertyEnumChanged);
    connect(mWorldDoc, &WorldDocument::propertyEnumAboutToBeRemoved,
            this, &PropertyDefinitionsDialog::propertyEnumAboutToBeRemoved);

    setEnums();
    setList();

    synchButtons();
}

void PropertyDefinitionsDialog::propertyDefinitionAdded(PropertyDef *, int)
{
    setList();
    clearUI();
}

void PropertyDefinitionsDialog::propertyDefinitionAboutToBeRemoved(int index)
{
    mSynching++;
    PropertyDef *pd = mWorldDoc->world()->propertyDefinitions().at(index);
    int row = mWorldDoc->world()->propertyDefinitions().sorted().indexOf(pd);
    delete ui->definitionsList->topLevelItem(row);
    mSynching--;

    // Another row may have been selected during deletion; deselect it
    clearUI();
}

void PropertyDefinitionsDialog::propertyDefinitionChanged(PropertyDef *pd)
{
#if 1
    // Changing the name might change the sort order
    PropertyDef *current = mDef;
    setList();
    if (current != 0) {
        int row = mWorldDoc->world()->propertyDefinitions().sorted().indexOf(pd);
        QTreeWidgetItem *item = ui->definitionsList->topLevelItem(row);
        ui->definitionsList->setCurrentItem(item);
    }
#else
    int row = mWorldDoc->world()->propertyDefinitions().sorted().indexOf(pd);
    QTreeWidgetItem *item = ui->definitionsList->topLevelItem(row);
    item->setData(0, Qt::DisplayRole, pd->mName);
    item->setData(1, Qt::DisplayRole, pd->mDefaultValue);

    if (pd == mDef)
        definitionSelected();
#endif
}

void PropertyDefinitionsDialog::propertyEnumAdded(int index)
{
    Q_UNUSED(index)
    setEnums();
}

void PropertyDefinitionsDialog::propertyEnumAboutToBeRemoved(int index)
{
    ui->enumCombo->removeItem(index + 1);
}

void PropertyDefinitionsDialog::propertyEnumChanged(PropertyEnum *pe)
{
    int index = mWorldDoc->world()->propertyEnums().indexOf(pe) + 1;
    ui->enumCombo->setItemText(index, pe->name());
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

    ui->enumCombo->setCurrentIndex((mDef && mDef->mEnum)
                                   ? mWorldDoc->world()->propertyEnums().indexOf(mDef->mEnum) + 1
                                   : 0);

    ui->defDescEdit->document()->clearUndoRedoStacks();
    synchButtons();
}

void PropertyDefinitionsDialog::addDefinition()
{
    QString name = ui->defNameEdit->text();
    QString defaultValue = ui->defDefaultEdit->text();
    QString description = ui->defDescEdit->toPlainText();
    PropertyEnum *pe = mEnum;
    PropertyDef *pd = new PropertyDef(name, defaultValue, description, pe);

    mWorldDoc->addPropertyDefinition(pd);
#if 0
    setList();

    // If nothing was selected...
    ui->defNameEdit->clear();
    ui->defDefaultEdit->clear();
    ui->defDescEdit->clear();

    synchButtons();
#endif
}

void PropertyDefinitionsDialog::updateDefinition()
{
    Q_ASSERT(mDef && mItem);

    if (mEnum != mDef->mEnum) {
        int ret = QMessageBox::warning(this, tr("Change Enum"),
                                       tr("You are about to change the enum assigned to this property definition.\nDoing so might change the property value of all cells and objects in the world!\nReally apply these changes?"),
                                       QMessageBox::Yes, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    mWorldDoc->changePropertyDefinition(mDef,
                                        ui->defNameEdit->text(),
                                        ui->defDefaultEdit->text(),
                                        ui->defDescEdit->toPlainText(),
                                        mEnum);
#if 0
    PropertyDef *pd = mDef;
    setList();
    int row = mWorldDoc->world()->propertyDefinitions().sorted().indexOf(pd);
    QTreeWidgetItem *item = ui->definitionsList->topLevelItem(row);
    ui->definitionsList->setCurrentItem(item);

    synchButtons();
#endif
}

void PropertyDefinitionsDialog::removeDefinition()
{
    Q_ASSERT(mDef && mItem);
    if (mWorldDoc->removePropertyDefinition(mDef)) {
#if 0
        mSynching++;
        delete mItem; // removes it from view
        mSynching--;

        // Another row may have been selected during deletion; deselect it
        clearUI();
#endif
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

    QString name = ui->defNameEdit->text();
    if (mDef) {
        if (name != mDef->mName ||
                ui->defDefaultEdit->text() != mDef->mDefaultValue ||
                ui->defDescEdit->toPlainText() != mDef->mDescription ||
                mEnum != mDef->mEnum) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    if (!name.isEmpty())
        enableAdd = true;
    else
        enableUpdate = false;

    if (PropertyDef *pd = mWorldDoc->world()->propertyDefinition(name)) {
        enableAdd = false;
        if (pd != mDef)
            enableUpdate = false;
    }

    ui->addDefButton->setEnabled(enableAdd);
    ui->updateDefButton->setEnabled(enableUpdate);
    ui->removeDefButton->setEnabled(enableRemove);

    ui->addDefButton->setDefault(enableAdd && !enableUpdate);
    ui->updateDefButton->setDefault(enableUpdate);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(!enableAdd &&
                                                               !enableUpdate);

//    ui->enumCombo->setEnabled(mDef);
}

void PropertyDefinitionsDialog::editEnums()
{
    PropertyEnumDialog d(mWorldDoc, this);
    d.exec();
    setEnums();

    ui->enumCombo->setCurrentIndex((mDef && mDef->mEnum)
                                   ? mWorldDoc->world()->propertyEnums().indexOf(mDef->mEnum) + 1
                                   : 0);

    synchButtons();
}

void PropertyDefinitionsDialog::currentEnumChanged()
{
//    if (mSynching || !mDef) return;
    int index = ui->enumCombo->currentIndex();
    mEnum = (index > 0) ? mWorldDoc->world()->propertyEnums().at(index - 1) : 0;
#if 1
    synchButtons();
#else
    if (mEnum != mDef->mEnum)
        mWorldDoc->changePropertyDefinition(mDef, mDef->mName, mDef->mDefaultValue,
                                            mDef->mDescription, mEnum);
#endif
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

void PropertyDefinitionsDialog::setEnums()
{
    mSynching++;
    ui->enumCombo->clear();
    ui->enumCombo->addItem(tr("<none>"));
    foreach (PropertyEnum *pe, mWorldDoc->world()->propertyEnums())
        ui->enumCombo->addItem(pe->name());
    mSynching--;
}
