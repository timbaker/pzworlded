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

#ifndef INGAMEMAP_PROPERTIESFORM_H
#define INGAMEMAP_PROPERTIESFORM_H

#include <QWidget>

class CellDocument;
class Document;
class WorldCell;
class WorldDocument;

class InGameMapFeature;
class InGameMapProperties;

class QListWidgetItem;

namespace Ui {
class InGameMapPropertiesForm;
}

class InGameMapPropertiesForm : public QWidget
{
    Q_OBJECT

public:
    explicit InGameMapPropertiesForm(QWidget *parent = nullptr);
    ~InGameMapPropertiesForm();

    void setDocument(Document *doc);
    void clearDocument();

    void setFeature(InGameMapFeature* feature);

private slots:
    void onAddButton();
    void onEditButton();
    void onDeleteButton();
    void onCopy();
    void onPaste();
    void onSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);
    void selectedFeaturesChanged();
    void propertiesChanged(WorldCell* cell, int featureIndex);

private:
    int selectedRow();
    void syncUi();

private:
    Ui::InGameMapPropertiesForm *ui;
    WorldDocument* mWorldDoc;
    CellDocument* mCellDoc;
    InGameMapFeature* mFeature;
    InGameMapProperties* mProperties;
};

#endif // INGAMEMAP_PROPERTIESFORM_H
