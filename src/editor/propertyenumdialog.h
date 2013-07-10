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

#ifndef PROPERTYENUMDIALOG_H
#define PROPERTYENUMDIALOG_H

#include <QDialog>

class PropertyEnum;
class UndoRedoButtons;
class World;
class WorldDocument;

class QListWidgetItem;

namespace Ui {
class PropertyEnumDialog;
}

class PropertyEnumDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit PropertyEnumDialog(WorldDocument *worldDoc, QWidget *parent = 0);
    ~PropertyEnumDialog();
    
    World *world();

private slots:
    void addEnum();
    void removeEnum();
    void currentEnumChanged(int row);
    void propertyEnumAboutToBeRemoved(int index);
    void propertyEnumChanged(PropertyEnum *pe);

    void addChoice();
    void removeChoice();
    void currentChoiceChanged(int row);

    void multiToggled(bool multi);

    void updateActions();

    void setEnumsList();
    void setChoicesList();

    void enumItemChanged(QListWidgetItem *item);
    void choiceItemChanged(QListWidgetItem *item);

private:
    QString makeNameUnique(const QString &name, PropertyEnum *ignore);
    QString makeChoiceUnique(const QString &name, int ignore);

private:
    Ui::PropertyEnumDialog *ui;
    UndoRedoButtons *mUndoRedoButtons;
    WorldDocument *mDocument;
    PropertyEnum *mCurrentEnum;
    int mSynching;
};

#endif // PROPERTYENUMDIALOG_H
