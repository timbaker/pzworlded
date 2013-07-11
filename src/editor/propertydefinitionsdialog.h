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

#ifndef PROPERTYDEFINITIONSDIALOG_H
#define PROPERTYDEFINITIONSDIALOG_H

#include <QDialog>

class PropertyDef;
class PropertyEnum;
class UndoRedoButtons;
class WorldDocument;

class QTreeWidgetItem;

namespace Ui {
class PropertyDefinitionsDialog;
}

class PropertyDefinitionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PropertyDefinitionsDialog(WorldDocument *worldDoc, QWidget *parent = 0);

private slots:
    void propertyDefinitionAdded(PropertyDef *pd, int index);
    void propertyDefinitionAboutToBeRemoved(int index);
    void propertyDefinitionChanged(PropertyDef *pd);

    void propertyEnumAdded(int index);
    void propertyEnumAboutToBeRemoved(int index);
    void propertyEnumChanged(PropertyEnum *pe);

    void definitionSelected();
    void addDefinition();
    void updateDefinition();
    void removeDefinition();
    void clearUI();
    void synchButtons();

    void editEnums();
    void currentEnumChanged();

private:
    void setList();
    void setEnums();

private:
    Ui::PropertyDefinitionsDialog *ui;
    UndoRedoButtons *mUndoRedoButtons;
    WorldDocument *mWorldDoc;
    PropertyDef *mDef;
    PropertyEnum *mEnum;
    QTreeWidgetItem *mItem;
    int mSynching;
};

#endif // PROPERTYDEFINITIONSDIALOG_H
