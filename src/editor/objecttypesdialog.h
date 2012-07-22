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

#ifndef OBJECTTYPESDIALOG_H
#define OBJECTTYPESDIALOG_H

#include <QDialog>

class ObjectType;
class WorldDocument;

class QListWidgetItem;

namespace Ui {
class ObjectTypesDialog;
}

class ObjectTypesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ObjectTypesDialog(WorldDocument *worldDoc, QWidget *parent = 0);

private slots:
    void typeSelected();
    void add();
    void update();
    void remove();
    void synchButtons();

private:
    void setList();
    void clearUI();

private:
    Ui::ObjectTypesDialog *ui;
    WorldDocument *mWorldDoc;
    ObjectType *mObjType;
    QListWidgetItem *mItem;
    bool mSynching;
};

#endif // OBJECTTYPESDIALOG_H
