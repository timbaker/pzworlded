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

#include "templatesdialog.h"
#include "ui_templatesdialog.h"

#include "world.h"
#include "worlddocument.h"

TemplatesDialog::TemplatesDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TemplatesDialog)
    , mWorldDoc(worldDoc)
    , mTemplate(0)
{
    ui->setupUi(this);

    QListWidget *view = ui->templatesView;
    foreach (PropertyTemplate *pt, mWorldDoc->world()->propertyTemplates()) {
        view->addItem(pt->mName);
    }

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

    synchButtons();
}

void TemplatesDialog::selectionChanged()
{
    QModelIndexList selection = ui->templatesView->selectionModel()->selectedIndexes();
    if (selection.size() == 1) {
        int index = selection.first().row();
        mTemplate = mWorldDoc->world()->propertyTemplates().at(index);
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
    if (ui->propertiesView->model()->toTemplate(index)) {
        mWorldDoc->removeTemplate(mTemplate, index.row());
    }  else if (ui->propertiesView->model()->toProperty(index)) {
        mWorldDoc->removeProperty(mTemplate, index.row());
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
    ui->templatesView->currentItem()->setText(mTemplate->mName);
    synchButtons();
}

void TemplatesDialog::removeSelectedTemplate()
{
    int index = ui->templatesView->currentRow();
    mWorldDoc->removeTemplate(index);
    clearTemplate();
    delete ui->templatesView->takeItem(index);
}

void TemplatesDialog::addTemplate()
{
    mWorldDoc->addTemplate(ui->templateName->text(),
                           ui->templateDesc->toPlainText());
    ui->templatesView->addItem(ui->templateName->text());
//    ui->templatesView->clearSelection();
    clearTemplate();
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

    if (mTemplate) {
        if (ui->templateName->text() != mTemplate->mName ||
                ui->templateDesc->toPlainText() != mTemplate->mDescription) {
            enableUpdate = true;
        }
        enableRemove = true;
    }

    QString name = ui->templateName->text();
    if (!name.isEmpty()) {
        enableAdd = true;
        foreach (PropertyTemplate *pt, mWorldDoc->world()->propertyTemplates()) {
            if (name == pt->mName) {
                enableAdd = false;
                break;
             }
        }
    }

    ui->addTemplate->setEnabled(enableAdd);
    ui->updateTemplate->setEnabled(enableUpdate);
    ui->removeTemplate->setEnabled(enableRemove);
}

