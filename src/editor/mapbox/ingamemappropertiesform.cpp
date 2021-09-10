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

#include "ingamemappropertiesform.h"
#include "ui_ingamemappropertiesform.h"

#include "celldocument.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "ingamemappropertydialog.h"
#include "worldcellingamemap.h"

#include <QUndoStack>

InGameMapPropertiesForm::InGameMapPropertiesForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::InGameMapPropertiesForm)
    , mWorldDoc(nullptr)
    , mCellDoc(nullptr)
    , mFeature(nullptr)
    , mProperties(nullptr)
{
    ui->setupUi(this);

    connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &InGameMapPropertiesForm::onItemDoubleClicked);

    syncUi();
}

InGameMapPropertiesForm::~InGameMapPropertiesForm()
{
    delete ui;
}

void InGameMapPropertiesForm::setDocument(Document *doc)
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc) {
        mCellDoc->worldDocument()->disconnect(this);
        mCellDoc->disconnect(this);
    }

    mWorldDoc = doc ? doc->asWorldDocument() : nullptr;
    mCellDoc = doc ? doc->asCellDocument() : nullptr;

    if (mWorldDoc) {
        connect(mWorldDoc, &WorldDocument::inGameMapPropertiesChanged,
                this, &InGameMapPropertiesForm::propertiesChanged);
        connect(mWorldDoc, &WorldDocument::selectedInGameMapFeaturesChanged,
                this, &InGameMapPropertiesForm::selectedFeaturesChanged);
    }
    if (mCellDoc) {
        connect(mCellDoc->worldDocument(), &WorldDocument::inGameMapPropertiesChanged,
                this, &InGameMapPropertiesForm::propertiesChanged);
        connect(mCellDoc, &CellDocument::selectedInGameMapFeaturesChanged,
                this, &InGameMapPropertiesForm::selectedFeaturesChanged);
    }

    setFeature(nullptr); // FIXME: use single selected feature if any
}

void InGameMapPropertiesForm::clearDocument()
{
    if (mWorldDoc)
        mWorldDoc->disconnect(this);
    if (mCellDoc) {
        mCellDoc->worldDocument()->disconnect(this);
        mCellDoc->disconnect(this);
    }

    mWorldDoc = nullptr;
    mCellDoc = nullptr;

    setFeature(nullptr);
}

void InGameMapPropertiesForm::setFeature(InGameMapFeature *feature)
{
    mFeature = feature;

    ui->listWidget->clear();

    if (mFeature != nullptr) {
        for (auto& property : mFeature->properties()) {
            QListWidgetItem* item = new QListWidgetItem(
                        QStringLiteral("%1 = %2").arg(property.mKey).arg(property.mValue),
                        ui->listWidget);
        }
    }

    syncUi();
}

void InGameMapPropertiesForm::onAddButton()
{
    InGameMapPropertyDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
        InGameMapProperty property;
        property.mKey = dialog.getKey();
        property.mValue = dialog.getValue();
        worldDoc->addInGameMapProperty(mFeature->cell(), mFeature->index(), mFeature->properties().size(), property);
    }
}

void InGameMapPropertiesForm::onEditButton()
{
    int index = selectedRow();
    if (index == -1)
        return;

    InGameMapPropertyDialog dialog(this);
    auto& property = mFeature->properties().at(index);
    dialog.setProperty(property.mKey, property.mValue);
    if (dialog.exec() == QDialog::Accepted) {
        WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
        InGameMapProperty property;
        property.mKey = dialog.getKey();
        property.mValue = dialog.getValue();
        worldDoc->setInGameMapProperty(mFeature->cell(), mFeature->index(), index, property);
    }
}

void InGameMapPropertiesForm::onDeleteButton()
{
    int index = selectedRow();
    if (index == -1)
        return;

    WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
    worldDoc->removeInGameMapProperty(mFeature->cell(), mFeature->index(), index);
}

void InGameMapPropertiesForm::onCopy()
{
    if (!mFeature)
        return;
    if (!mProperties)
        mProperties = new InGameMapProperties();
    *mProperties = mFeature->mProperties;
    syncUi();
}

void InGameMapPropertiesForm::onPaste()
{
    auto& selected = mWorldDoc ? mWorldDoc->selectedInGameMapFeatures() : mCellDoc->selectedInGameMapFeatures();
    WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
    worldDoc->undoStack()->beginMacro(QStringLiteral("Paste InGameMap Properties"));
    for (auto* feature : selected) {
        worldDoc->setInGameMapProperties(feature->cell(), feature->index(), *mProperties);
    }
    worldDoc->undoStack()->endMacro();
}

void InGameMapPropertiesForm::onSelectionChanged()
{
    syncUi();
}

void InGameMapPropertiesForm::onItemDoubleClicked(QListWidgetItem* item)
{
    onEditButton();
}

void InGameMapPropertiesForm::selectedFeaturesChanged()
{
    auto& selected = mWorldDoc ? mWorldDoc->selectedInGameMapFeatures() : mCellDoc->selectedInGameMapFeatures();
    auto feature = selected.size() == 1 ? selected.first() : nullptr;
    setFeature(feature);
}

void InGameMapPropertiesForm::propertiesChanged(WorldCell *cell, int featureIndex)
{
    if (!mFeature || mFeature->cell() != cell || mFeature->index() != featureIndex)
        return;

    setFeature(mFeature);
}

int InGameMapPropertiesForm::selectedRow()
{
    auto selected = ui->listWidget->selectedItems();
    if (selected.isEmpty())
        return -1;
    auto* item = selected.first();
    return ui->listWidget->row(item);
}

void InGameMapPropertiesForm::syncUi()
{
    if (!mWorldDoc && !mCellDoc) {
        setEnabled(false);
        return;
    }

    auto& selectedFeatures = mWorldDoc ? mWorldDoc->selectedInGameMapFeatures() : mCellDoc->selectedInGameMapFeatures();

    ui->addButton->setEnabled(mFeature != nullptr);

    auto selected = ui->listWidget->selectedItems();
    ui->deleteButton->setEnabled(!selected.isEmpty());
    ui->editButton->setEnabled(!selected.isEmpty());

    ui->copyButton->setEnabled(selectedFeatures.size() == 1);
    ui->pasteButton->setEnabled(!selectedFeatures.isEmpty() && mProperties != nullptr);

    setEnabled(!selectedFeatures.isEmpty());
}
