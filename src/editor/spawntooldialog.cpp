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

#include "spawntooldialog.h"
#include "ui_spawntooldialog.h"

#include "celldocument.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"

#include <QSettings>

SINGLETON_IMPL(SpawnToolDialog)

SpawnToolDialog::SpawnToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpawnToolDialog),
    mDocument(0)
{
    ui->setupUi(this);

    ui->list->setSortingEnabled(false);

    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->add, SIGNAL(clicked()), SLOT(addItem()));
    connect(ui->remove, SIGNAL(clicked()), SLOT(removeItem()));
    connect(ui->list, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(itemChanged(QListWidgetItem*)));
}

SpawnToolDialog::~SpawnToolDialog()
{
    delete ui;
}

void SpawnToolDialog::setVisible(bool visible)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("SpawnToolDialog"));
    if (visible) {
        QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
        if (!geom.isEmpty())
            restoreGeometry(geom);
    }

    QDialog::setVisible(visible);

    if (!visible) {
        settings.setValue(QLatin1String("geometry"), saveGeometry());
    }
    settings.endGroup();
}

void SpawnToolDialog::setDocument(CellDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    if (mDocument) {
        connect(mDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(selectedObjectsChanged()));
        connect(mDocument->worldDocument(), SIGNAL(professionsChanged()),
                SLOT(professionsChanged()));
        setList();
        selectedObjectsChanged();
    }
}

void SpawnToolDialog::selectedObjectsChanged()
{
    int count = selectedSpawnPoints().size();
    ui->label->setText(tr("%1 objects selected").arg(count));
}

void SpawnToolDialog::professionsChanged()
{
    setList();
}

void SpawnToolDialog::addItem()
{
    mDocument->worldDocument()->setProfessions(mDocument->world()->professions()
                                               << QLatin1String("newprofession"));
}

void SpawnToolDialog::removeItem()
{
}

void SpawnToolDialog::itemChanged(QListWidgetItem *item)
{
    int row = ui->list->row(item);
    QStringList professions = mDocument->world()->professions();
    if (item->text() != professions.at(row)) {
        professions[row] = item->text();
        mDocument->worldDocument()->setProfessions(professions);
    }

    if (selectedSpawnPoints().size())
        toObjects();
}

void SpawnToolDialog::setList()
{
    ui->list->clear();

    foreach (QString profession, mDocument->world()->professions()) {
        QListWidgetItem *item = new QListWidgetItem(profession);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setCheckState(Qt::Checked);
        ui->list->addItem(item);
    }
}

void SpawnToolDialog::toObjects()
{
    PropertyDef *pd = mDocument->world()->propertyDefinition(QLatin1String("Professions"));
    if (!pd) return;

    QStringList professions;
    for (int row = 0; row < ui->list->count(); row++) {
        if (ui->list->item(row)->checkState() == Qt::Checked)
            professions += ui->list->item(row)->text();
    }
    QString value = professions.join(QLatin1String(","));

    foreach (WorldCellObject *obj, selectedSpawnPoints()) {
        if (Property *p = obj->properties().find(pd))
            mDocument->worldDocument()->setPropertyValue(obj, p, value);
        else
            mDocument->worldDocument()->addProperty(obj, pd->mName, value);
    }
}

QList<WorldCellObject *> SpawnToolDialog::selectedSpawnPoints()
{
    QList<WorldCellObject *> objects;
    foreach (WorldCellObject *obj, mDocument->selectedObjects())
        if (obj->isSpawnPoint())
            objects += obj;
    return objects;
}
