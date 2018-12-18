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

#include "mapboxpropertiesform.h"
#include "ui_mapboxpropertiesform.h"

#include "celldocument.h"
#include "worldcell.h"
#include "worlddocument.h"

#include "mapboxpropertydialog.h"
#include "worldcellmapbox.h"

MapboxPropertiesForm::MapboxPropertiesForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MapboxPropertiesForm)
    , mWorldDoc(nullptr)
    , mCellDoc(nullptr)
    , mFeature(nullptr)
{
    ui->setupUi(this);

    syncUi();
}

MapboxPropertiesForm::~MapboxPropertiesForm()
{
    delete ui;
}

void MapboxPropertiesForm::setDocument(Document *doc)
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
        connect(mWorldDoc, &WorldDocument::mapboxPropertiesChanged,
                this, &MapboxPropertiesForm::propertiesChanged);
    }
    if (mCellDoc) {
        connect(mCellDoc->worldDocument(), &WorldDocument::mapboxPropertiesChanged,
                this, &MapboxPropertiesForm::propertiesChanged);
        connect(mCellDoc, &CellDocument::selectedMapboxFeaturesChanged,
                this, &MapboxPropertiesForm::selectedFeaturesChanged);
    }

    setFeature(nullptr); // FIXME: use single selected feature if any
}

void MapboxPropertiesForm::clearDocument()
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

void MapboxPropertiesForm::setFeature(MapBoxFeature *feature)
{
    mFeature = feature;

    ui->listWidget->clear();

    if (mFeature != nullptr) {
        for (auto& property : mFeature->properties()) {
            QListWidgetItem* item = new QListWidgetItem(QStringLiteral("%1 = %2").arg(property.mKey).arg(property.mValue), ui->listWidget);
        }
    }

    syncUi();
}

void MapboxPropertiesForm::onAddButton()
{
    MapboxPropertyDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
        MapBoxProperty property;
        property.mKey = dialog.getKey();
        property.mValue = dialog.getValue();
        worldDoc->addMapboxProperty(mFeature->cell(), mFeature->index(), mFeature->properties().size(), property);
    }
}

void MapboxPropertiesForm::onEditButton()
{
    int index = selectedRow();
    if (index == -1)
        return;

    MapboxPropertyDialog dialog(this);
    auto& property = mFeature->properties().at(index);
    dialog.setProperty(property.mKey, property.mValue);
    if (dialog.exec() == QDialog::Accepted) {
        WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
        MapBoxProperty property;
        property.mKey = dialog.getKey();
        property.mValue = dialog.getValue();
        worldDoc->setMapboxProperty(mFeature->cell(), mFeature->index(), index, property);
    }
}

void MapboxPropertiesForm::onDeleteButton()
{
    int index = selectedRow();
    if (index == -1)
        return;

    WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
    worldDoc->removeMapboxProperty(mFeature->cell(), mFeature->index(), index);
}

void MapboxPropertiesForm::onSelectionChanged()
{
    syncUi();
}

void MapboxPropertiesForm::selectedFeaturesChanged()
{
    auto& selected = mCellDoc->selectedMapboxFeatures();
    auto feature = selected.size() == 1 ? selected.first() : nullptr;
    setFeature(feature);
}

void MapboxPropertiesForm::propertiesChanged(WorldCell *cell, int featureIndex)
{
    if (!mFeature || mFeature->cell() != cell || mFeature->index() != featureIndex)
        return;

    setFeature(mFeature);
}

int MapboxPropertiesForm::selectedRow()
{
    auto selected = ui->listWidget->selectedItems();
    if (selected.isEmpty())
        return -1;
    auto* item = selected.first();
    return ui->listWidget->row(item);
}

void MapboxPropertiesForm::syncUi()
{
    ui->addButton->setEnabled(mFeature != nullptr);

    auto selected = ui->listWidget->selectedItems();
    ui->deleteButton->setEnabled(!selected.isEmpty());
    ui->editButton->setEnabled(!selected.isEmpty());

    setEnabled(mFeature != nullptr);
}
