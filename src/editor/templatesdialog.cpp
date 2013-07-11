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

#include "templatesdialog.h"
#include "ui_templatesdialog.h"

#include "undoredo.h"
#include "world.h"
#include "worlddocument.h"

#include <QToolButton>

TemplatesDialog::TemplatesDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TemplatesDialog)
    , mUndoRedoButtons(new UndoRedoButtons(worldDoc, this))
    , mWorldDoc(worldDoc)
    , mTemplate(0)
{
    ui->setupUi(this);

    ui->buttonsLayout->insertWidget(0, mUndoRedoButtons->undoButton());
    ui->buttonsLayout->insertWidget(1, mUndoRedoButtons->redoButton());

    setList();

    QListWidget *view = ui->templatesView;
    connect(view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));

    connect(ui->propertiesView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(displayDescription()));
    connect(ui->propertiesView, SIGNAL(closeItem(QModelIndex)),
            SLOT(closeItem(QModelIndex)));

    connect(ui->clearTemplate, SIGNAL(clicked()),
            SLOT(clearTemplate()));
    connect(ui->addTemplate, SIGNAL(clicked()),
            SLOT(addTemplate()));
    connect(ui->removeTemplate, SIGNAL(clicked()),
            SLOT(removeSelectedTemplate()));
    connect(ui->updateTemplate, SIGNAL(clicked()),
            SLOT(updateSelectedTemplate()));

    connect(ui->templateName, SIGNAL(textChanged(QString)), SLOT(synchButtons()));
    connect(ui->templateDesc, SIGNAL(textChanged()), SLOT(synchButtons()));

    connect(mWorldDoc, SIGNAL(templateAboutToBeRemoved(int)),
            SLOT(templateAboutToBeRemoved(int)));
    connect(mWorldDoc, SIGNAL(templateAdded(int)),
            SLOT(templateAdded(int)));
    connect(mWorldDoc, SIGNAL(templateChanged(PropertyTemplate*)),
            SLOT(templateChanged(PropertyTemplate*)));

    synchButtons();
}

void TemplatesDialog::setList()
{
    QListWidget *view = ui->templatesView;
    view->clear();
    foreach (PropertyTemplate *pt, mWorldDoc->world()->propertyTemplates().sorted())
        view->addItem(pt->mName);
}

int TemplatesDialog::rowOf(PropertyTemplate *pt)
{
    return mWorldDoc->world()->propertyTemplates().sorted().indexOf(pt);
}

void TemplatesDialog::selectionChanged()
{
    QModelIndexList selection = ui->templatesView->selectionModel()->selectedIndexes();
    if (selection.size() == 1) {
        int row = selection.first().row();
        mTemplate = mWorldDoc->world()->propertyTemplates().sorted().at(row);
        ui->propertiesView->setPropertyHolder(mWorldDoc, mTemplate);
    } else {
        mTemplate = 0;
        ui->propertiesView->clearPropertyHolder();
    }

    if (mTemplate) {
        ui->templateName->setText(mTemplate->mName);
        ui->templateDesc->setPlainText(mTemplate->mDescription);
        ui->templateDesc->document()->clearUndoRedoStacks();
    } else {
        ui->templateName->clear();
        ui->templateDesc->clear();
        ui->templateDesc->document()->clearUndoRedoStacks();
    }

    synchButtons();
}

void TemplatesDialog::closeItem(const QModelIndex &index)
{
    if (PropertyTemplate *pt = ui->propertiesView->model()->toTemplate(index)) {
        int i = mTemplate->templates().indexOf(pt);
        mWorldDoc->removeTemplate(mTemplate, i);
    }  else if (Property *p = ui->propertiesView->model()->toProperty(index)) {
        int i = mTemplate->properties().indexOf(p);
        mWorldDoc->removeProperty(mTemplate, i);
    }
}

void TemplatesDialog::clearTemplate()
{
    ui->templatesView->setCurrentIndex(QModelIndex());
    ui->templateName->clear();
    ui->templateDesc->clear();
}

void TemplatesDialog::updateSelectedTemplate()
{
    mWorldDoc->changeTemplate(mTemplate, ui->templateName->text(),
                              ui->templateDesc->toPlainText());
#if 0
    PropertyTemplate *pt = mTemplate;
    setList();
    int row = mWorldDoc->world()->propertyTemplates().sorted().indexOf(pt);
    ui->templatesView->setCurrentRow(row, QItemSelectionModel::Select);
    synchButtons();
#endif
}

void TemplatesDialog::removeSelectedTemplate()
{
    int row = ui->templatesView->currentRow();
    int index = mWorldDoc->world()->propertyTemplates().indexOf(mTemplate);
    mWorldDoc->removeTemplate(index);
#if 0
    clearTemplate();
    delete ui->templatesView->takeItem(row);
#endif
}

void TemplatesDialog::addTemplate()
{
    mWorldDoc->addTemplate(ui->templateName->text(),
                           ui->templateDesc->toPlainText());
#if 0
    setList();
//    ui->templatesView->clearSelection();
    clearTemplate();
#endif
}

void TemplatesDialog::displayDescription()
{
    ui->propertyDesc->clear();
    QModelIndexList selection = ui->propertiesView->selectionModel()->selectedIndexes();
    if (selection.size() == 2) {
        QString desc;
        if (PropertyTemplate *pt = ui->propertiesView->model()->toTemplate(selection.first()))
            desc = pt->mDescription;
        if (Property *p = ui->propertiesView->model()->toProperty(selection.first()))
            desc = p->mDefinition->mDescription;
        ui->propertyDesc->insertPlainText(desc);
    } else {
    }
}

void TemplatesDialog::synchButtons()
{
    bool enableUpdate = false;
    bool enableAdd = false;
    bool enableRemove = false;

    QString name = ui->templateName->text();
    if (mTemplate) {
        if (name != mTemplate->mName ||
                ui->templateDesc->toPlainText() != mTemplate->mDescription) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    if (!name.isEmpty())
        enableAdd = true;
    else
        enableUpdate = false;

    if (PropertyTemplate *pt = mWorldDoc->world()->propertyTemplates().find(name)) {
        enableAdd = false;
        if (pt != mTemplate)
            enableUpdate = false;
    }

    ui->addTemplate->setEnabled(enableAdd);
    ui->updateTemplate->setEnabled(enableUpdate);
    ui->removeTemplate->setEnabled(enableRemove);

    ui->addTemplate->setDefault(enableAdd && !enableUpdate);
    ui->updateTemplate->setDefault(enableUpdate);
    ui->buttonBox->button(QDialogButtonBox::Close)->setDefault(!enableAdd &&
                                                               !enableUpdate);
}

void TemplatesDialog::templateAdded(int index)
{
    Q_UNUSED(index)
    PropertyTemplate *pt = mTemplate;
    setList();
    if (pt)
        ui->templatesView->setCurrentRow(rowOf(pt));
    else
        clearTemplate();
}

void TemplatesDialog::templateAboutToBeRemoved(int index)
{
    PropertyTemplate *pt = mWorldDoc->world()->propertyTemplates().at(index);
    int row = rowOf(pt);
    if (row == ui->templatesView->currentRow()) {
        clearTemplate();
    }
    delete ui->templatesView->takeItem(row);
}

void TemplatesDialog::templateChanged(PropertyTemplate *pt)
{
    PropertyTemplate *current = mTemplate;
    setList();
    if (current)
        ui->templatesView->setCurrentRow(rowOf(pt));
}
