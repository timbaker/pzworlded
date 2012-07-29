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

#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include "preferences.h"

#include <QFileDialog>
#include <QStringList>

PreferencesDialog::PreferencesDialog(WorldDocument *worldDoc, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PreferencesDialog)
    , mWorldDoc(worldDoc)
{
    ui->setupUi(this);

    connect(ui->pathsList, SIGNAL(itemSelectionChanged()), SLOT(pathSelectionChanged()));
    connect(ui->addPathButton, SIGNAL(clicked()), SLOT(addPath()));
    connect(ui->removePathButton, SIGNAL(clicked()), SLOT(removePath()));
    connect(ui->pathUpButton, SIGNAL(clicked()), SLOT(movePathUp()));
    connect(ui->pathDownButton, SIGNAL(clicked()), SLOT(movePathDown()));

    connect(this, SIGNAL(accepted()), SLOT(dialogAccepted()));

    Preferences *prefs = Preferences::instance();
    mSearchPaths = prefs->searchPaths();

    setPathsList();
    ui->openGL->setChecked(prefs->useOpenGL());
}

void PreferencesDialog::setPathsList()
{
    ui->pathsList->clear();
    ui->pathsList->addItems(mSearchPaths);
    synchPathsButtons();
}

void PreferencesDialog::synchPathsButtons()
{
    if (int row = selectedPath()) {
        ui->removePathButton->setEnabled(true);
        ui->pathUpButton->setEnabled(row > 1);
        ui->pathDownButton->setEnabled(row < mSearchPaths.size());
    } else {
        ui->removePathButton->setEnabled(false);
        ui->pathUpButton->setEnabled(false);
        ui->pathDownButton->setEnabled(false);
    }
}

int PreferencesDialog::selectedPath()
{
    QListWidget *view = ui->pathsList;
    QList<QListWidgetItem*> selection = view->selectedItems();
    if (selection.size() == 1)
        return view->row(selection.first()) + 1; // view->currentRow()
    return 0;
}

void PreferencesDialog::addPath()
{
    QString f = QFileDialog::getExistingDirectory(this);
    if (!f.isEmpty()) {
        mSearchPaths += f;
        setPathsList();
    }
}

void PreferencesDialog::removePath()
{
    if (int index = selectedPath()) {
        --index;
        mSearchPaths.removeAt(index);
        setPathsList();
    }
}

void PreferencesDialog::movePathUp()
{
    if (int index = selectedPath()) {
        --index;
        QString path = mSearchPaths.takeAt(index);
        mSearchPaths.insert(index - 1, path);
        setPathsList();
    }
}

void PreferencesDialog::movePathDown()
{
    if (int index = selectedPath()) {
        --index;
        QString path = mSearchPaths.takeAt(index);
        mSearchPaths.insert(index + 1, path);
        setPathsList();
    }
}

void PreferencesDialog::pathSelectionChanged()
{
    synchPathsButtons();
}

void PreferencesDialog::dialogAccepted()
{
    Preferences *prefs = Preferences::instance();
    prefs->setSearchPaths(mSearchPaths);
    prefs->setUseOpenGL(ui->openGL->isChecked());
}
