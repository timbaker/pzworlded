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

#include "propertyenumdialog.h"
#include "ui_propertyenumdialog.h"

#include "world.h"
#include "worlddocument.h"

#include <QUndoStack>

PropertyEnumDialog::PropertyEnumDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PropertyEnumDialog),
    mDocument(worldDoc),
    mCurrentEnum(0),
    mSynching(0)
{
    ui->setupUi(this);

    connect(ui->enumList, SIGNAL(currentRowChanged(int)), SLOT(currentEnumChanged(int)));
    connect(ui->enumList, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(enumItemChanged(QListWidgetItem*)));
    connect(ui->addEnum, SIGNAL(clicked()), SLOT(addEnum()));
    connect(ui->removeEnum, SIGNAL(clicked()), SLOT(removeEnum()));

    connect(ui->choiceList, SIGNAL(currentRowChanged(int)), SLOT(currentChoiceChanged(int)));
    connect(ui->choiceList, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(choiceItemChanged(QListWidgetItem*)));
    connect(ui->addChoice, SIGNAL(clicked()), SLOT(addChoice()));
    connect(ui->removeChoice, SIGNAL(clicked()), SLOT(removeChoice()));

    connect(ui->multiCheckBox, SIGNAL(toggled(bool)), SLOT(multiToggled(bool)));

    connect(ui->undo, SIGNAL(clicked()), mDocument->undoStack(), SLOT(undo()));
    connect(ui->redo, SIGNAL(clicked()), mDocument->undoStack(), SLOT(redo()));

    connect(mDocument, SIGNAL(propertyEnumAdded(int)), SLOT(setEnumsList()));
    connect(mDocument, SIGNAL(propertyEnumAboutToBeRemoved(int)),
            SLOT(propertyEnumAboutToBeRemoved(int)));
    connect(mDocument, SIGNAL(propertyEnumChanged(PropertyEnum*)),
            SLOT(propertyEnumChanged(PropertyEnum*)));
    connect(mDocument, SIGNAL(propertyEnumChoicesChanged(PropertyEnum*)),
            SLOT(setChoicesList()));
#if 0
    connect(mDocument->undoStack(), SIGNAL(canUndoChanged(bool)),
            ui->undo, SLOT(setEnabled(bool)));
    connect(mDocument->undoStack(), SIGNAL(canRedoChanged(bool)),
            ui->redo, SLOT(setEnabled(bool)));
#endif
    connect(mDocument->undoStack(), SIGNAL(indexChanged(int)),
            SLOT(updateActions()));

    mUndoIndex = mDocument->undoStack()->index();

    setEnumsList();
    updateActions();
}

PropertyEnumDialog::~PropertyEnumDialog()
{
    delete ui;
}

World *PropertyEnumDialog::world()
{
    return mDocument->world();
}

void PropertyEnumDialog::addEnum()
{
    mDocument->addPropertyEnum(tr("MyNewEnum"), false); // fixme: ensure unique
    ui->enumList->setCurrentRow(ui->enumList->count() - 1);
    ui->enumList->editItem(ui->enumList->item(ui->enumList->count() - 1));
}

void PropertyEnumDialog::removeEnum()
{
    mDocument->removePropertyEnum(mCurrentEnum);
}

void PropertyEnumDialog::currentEnumChanged(int row)
{
    mCurrentEnum = (row >= 0) ? world()->propertyEnums().at(row) : 0;
    setChoicesList();
    updateActions();
}

void PropertyEnumDialog::propertyEnumAboutToBeRemoved(int index)
{
#if 1
    ui->enumList->item(index)->setHidden(true);
    delete ui->enumList->takeItem(index);
#else
    ui->enumList->blockSignals(true);
    delete ui->enumList->takeItem(index);
    ui->enumList->blockSignals(false);
#endif
}

void PropertyEnumDialog::propertyEnumChanged(PropertyEnum *pe)
{
    int row = mDocument->world()->propertyEnums().indexOf(pe);
    mSynching++;
    ui->enumList->item(row)->setText(pe->name());
    ui->multiCheckBox->setChecked(pe->isMulti());
    mSynching--;
}

void PropertyEnumDialog::addChoice()
{
    QStringList choices = mCurrentEnum->values();
    mDocument->setPropertyEnumChoices(mCurrentEnum, choices << tr("MyNewChoice"));
    ui->choiceList->editItem(ui->choiceList->item(ui->choiceList->count() - 1));
}

void PropertyEnumDialog::removeChoice()
{
    QStringList choices = mCurrentEnum->values();
    choices.removeAt(ui->choiceList->currentRow());
    mDocument->setPropertyEnumChoices(mCurrentEnum, choices);
}

void PropertyEnumDialog::currentChoiceChanged(int row)
{
    Q_UNUSED(row)
    updateActions();
}

void PropertyEnumDialog::multiToggled(bool multi)
{
    if (mSynching) return;
    mDocument->changePropertyEnum(mCurrentEnum, mCurrentEnum->name(), multi);
}

void PropertyEnumDialog::updateActions()
{
    mSynching++;
    ui->removeEnum->setEnabled(mCurrentEnum != 0);
    ui->addChoice->setEnabled(mCurrentEnum != 0);
    ui->removeChoice->setEnabled(ui->choiceList->currentRow() != -1);
    ui->multiCheckBox->setEnabled(mCurrentEnum != 0);
    ui->multiCheckBox->setChecked(mCurrentEnum ? mCurrentEnum->isMulti() : false);
    ui->undo->setEnabled(mDocument->undoStack()->index() > mUndoIndex);
    ui->redo->setEnabled(mDocument->undoStack()->canRedo());
    mSynching--;
}

void PropertyEnumDialog::setEnumsList()
{
    ui->enumList->clear();

    foreach (PropertyEnum *pe, world()->propertyEnums()) {
        QListWidgetItem *item = new QListWidgetItem(pe->name());
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->enumList->addItem(item);
    }
}

void PropertyEnumDialog::setChoicesList()
{
    ui->choiceList->clear();

    if (!mCurrentEnum) return;

    foreach (QString name, mCurrentEnum->values()) {
        QListWidgetItem *item = new QListWidgetItem(name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->choiceList->addItem(item);
    }
}

void PropertyEnumDialog::enumItemChanged(QListWidgetItem *item)
{
    if (mSynching) return;
    int row = ui->enumList->row(item);
    PropertyEnum *pe = mDocument->world()->propertyEnums().at(row);
    mDocument->changePropertyEnum(pe, item->text(), pe->isMulti());
}

void PropertyEnumDialog::choiceItemChanged(QListWidgetItem *item)
{
    if (mSynching) return;
    QStringList choices = mCurrentEnum->values();
    choices[ui->choiceList->row(item)] = item->text();
    mDocument->setPropertyEnumChoices(mCurrentEnum, choices);
}
