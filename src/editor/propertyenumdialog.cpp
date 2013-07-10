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

#include "undoredo.h"
#include "world.h"
#include "worlddocument.h"

#include <QUndoStack>

PropertyEnumDialog::PropertyEnumDialog(WorldDocument *worldDoc, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PropertyEnumDialog),
    mUndoRedoButtons(new UndoRedoButtons(worldDoc, this)),
    mDocument(worldDoc),
    mCurrentEnum(0),
    mSynching(0)
{
    ui->setupUi(this);

    ui->buttonsLayout->insertWidget(0, mUndoRedoButtons->undoButton());
    ui->buttonsLayout->insertWidget(1, mUndoRedoButtons->redoButton());

    connect(ui->enumList, SIGNAL(currentRowChanged(int)), SLOT(currentEnumChanged(int)));
    connect(ui->enumList, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(enumItemChanged(QListWidgetItem*)));
    connect(ui->addEnum, SIGNAL(clicked()), SLOT(addEnum()));
    connect(ui->removeEnum, SIGNAL(clicked()), SLOT(removeEnum()));

    connect(ui->choiceList, SIGNAL(currentRowChanged(int)), SLOT(currentChoiceChanged(int)));
    connect(ui->choiceList, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(choiceItemChanged(QListWidgetItem*)));
    connect(ui->addChoice, SIGNAL(clicked()), SLOT(addChoice()));
    connect(ui->removeChoice, SIGNAL(clicked()), SLOT(removeChoice()));

    connect(ui->multiCheckBox, SIGNAL(toggled(bool)), SLOT(multiToggled(bool)));

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
    QString name = makeNameUnique(tr("MyNewEnum"), 0);
    mDocument->addPropertyEnum(name, QStringList(), false);
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
    QString choice = makeChoiceUnique(tr("MyNewChoice"), -1);
    mDocument->insertPropertyEnumChoice(mCurrentEnum, mCurrentEnum->values().size(), choice);
    ui->choiceList->editItem(ui->choiceList->item(ui->choiceList->count() - 1));
}

void PropertyEnumDialog::removeChoice()
{
    mDocument->removePropertyEnumChoice(mCurrentEnum, ui->choiceList->currentRow());
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
    QString name = makeNameUnique(item->text(), pe);
    if (name != pe->name())
        mDocument->changePropertyEnum(pe, name, pe->isMulti());
    else
        item->setText(name);
}

void PropertyEnumDialog::choiceItemChanged(QListWidgetItem *item)
{
    if (mSynching) return;
    int row = ui->choiceList->row(item);
    QString choice = makeChoiceUnique(item->text(), row);
    if (choice != mCurrentEnum->values().at(row))
        mDocument->renamePropertyEnumChoice(mCurrentEnum, row, choice);
    else
        item->setText(choice);
}

QString PropertyEnumDialog::makeNameUnique(const QString &name, PropertyEnum *ignore)
{
    QString unique = name;
    int n = 1;
    while (1) {
        PropertyEnum *pe = mDocument->world()->propertyEnums().find(unique);
        if (!pe || pe == ignore) break;
        unique = QString::fromLatin1("%1_%2").arg(name).arg(n++);
    }
    return unique;
}

QString PropertyEnumDialog::makeChoiceUnique(const QString &name, int ignore)
{
    QString unique = name;
    int n = 1;
    while (1) {
        int index = mCurrentEnum->values().indexOf(unique);
        if ((index < 0) || (index == ignore)) break;
        unique = QString::fromLatin1("%1_%2").arg(name).arg(n++);
    }
    return unique;
}
