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

#ifndef MAPBOXPROPERTIESFORM_H
#define MAPBOXPROPERTIESFORM_H

#include <QWidget>

class CellDocument;
class Document;
class WorldCell;
class WorldDocument;

class MapBoxFeature;
class MapBoxProperties;

namespace Ui {
class MapboxPropertiesForm;
}

class MapboxPropertiesForm : public QWidget
{
    Q_OBJECT

public:
    explicit MapboxPropertiesForm(QWidget *parent = nullptr);
    ~MapboxPropertiesForm();

    void setDocument(Document *doc);
    void clearDocument();

    void setFeature(MapBoxFeature* feature);

private slots:
    void onAddButton();
    void onEditButton();
    void onDeleteButton();
    void onCopy();
    void onPaste();
    void onSelectionChanged();
    void selectedFeaturesChanged();
    void propertiesChanged(WorldCell* cell, int featureIndex);

private:
    int selectedRow();
    void syncUi();

private:
    Ui::MapboxPropertiesForm *ui;
    WorldDocument* mWorldDoc;
    CellDocument* mCellDoc;
    MapBoxFeature* mFeature;
    MapBoxProperties* mProperties;
};

#endif // MAPBOXPROPERTIESFORM_H
