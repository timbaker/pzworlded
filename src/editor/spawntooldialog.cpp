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

#include <QMessageBox>
#include <QSettings>

SINGLETON_IMPL(SpawnToolDialog)

SpawnToolDialog::SpawnToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SpawnToolDialog),
    mDocument(0)
{
    ui->setupUi(this);

    ui->list->setSortingEnabled(false);
    ui->remove->setEnabled(false);

    setWindowFlags(windowFlags() | Qt::Tool);

    connect(ui->add, SIGNAL(clicked()), SLOT(addItem()));
    connect(ui->remove, SIGNAL(clicked()), SLOT(removeItem()));
    connect(ui->list, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(itemChanged(QListWidgetItem*)));
    connect(ui->list, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
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
    if (mDocument) {
        mDocument->disconnect(this);
        worldDocument()->disconnect(this);
    }

    mDocument = doc;

    if (mDocument) {
        connect(mDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(selectedObjectsChanged()));

        connect(worldDocument(), SIGNAL(propertyEnumAdded(int)),
                SLOT(checkProfessionsEnum()));
        connect(worldDocument(), SIGNAL(propertyEnumAboutToBeRemoved(int)),
                SLOT(propertyEnumAboutToBeRemoved(int)));
        connect(worldDocument(), SIGNAL(propertyEnumChoicesChanged(PropertyEnum*)),
                SLOT(propertyEnumChoicesChanged(PropertyEnum*)));
        connect(worldDocument(), SIGNAL(propertyEnumChanged(PropertyEnum*)),
                SLOT(checkProfessionsEnum()));

        connect(worldDocument(), SIGNAL(propertyAdded(PropertyHolder*,int)),
                SLOT(propertiesChanged(PropertyHolder*)));
        connect(worldDocument(), SIGNAL(propertyRemoved(PropertyHolder*,int)),
                SLOT(propertiesChanged(PropertyHolder*)));
        connect(worldDocument(), SIGNAL(propertyValueChanged(PropertyHolder*,int)),
                SLOT(propertiesChanged(PropertyHolder*)));

        connect(worldDocument(), SIGNAL(templateAdded(PropertyHolder*,int)),
                SLOT(propertiesChanged(PropertyHolder*)));
        connect(worldDocument(), SIGNAL(templateRemoved(PropertyHolder*,int)),
                SLOT(propertiesChanged(PropertyHolder*)));

        setList();
        selectedObjectsChanged();
    }
}

WorldDocument *SpawnToolDialog::worldDocument()
{
    return mDocument->worldDocument();
}

void SpawnToolDialog::selectedObjectsChanged()
{
    int count = selectedSpawnPoints().size();
    ui->label->setText(tr("%1 spawnpoints selected").arg(count));
    setList();
}

void SpawnToolDialog::propertyEnumAboutToBeRemoved(int index)
{
    if (mDocument->world()->propertyEnums().at(index) == professionsEnum())
        ui->list->clear();
}

void SpawnToolDialog::propertyEnumChoicesChanged(PropertyEnum *pe)
{
    if (pe->name() == QLatin1String("Professions"))
        setList();
}

void SpawnToolDialog::checkProfessionsEnum()
{
    if ((ui->list->count() == 0) != (professionsEnum() == 0))
        setList();
}

void SpawnToolDialog::propertiesChanged(PropertyHolder *ph)
{
//    Property *p = ph->properties().at(index);
//    if (p->mDefinition->mEnum == professionsEnum())
    foreach (WorldCellObject *obj, selectedSpawnPoints()) {
        if (obj == ph || (ph->isTemplate() && obj->usesTemplate((PropertyTemplate*)ph)))
            setList();
    }
}

void SpawnToolDialog::addItem()
{
    PropertyEnum *pe = professionsEnum();
    if (!pe) {
        worldDocument()->addPropertyEnum(QLatin1String("Professions"), QStringList(), true);
        pe = mDocument->world()->propertyEnums().find(QLatin1String("Professions"));
    }
    QString name = makeNameUnique(QLatin1String("profession"), -1);
    worldDocument()->insertPropertyEnumChoice(pe, pe->values().size(), name);
    ui->list->editItem(ui->list->item(ui->list->count() - 1));
}

void SpawnToolDialog::removeItem()
{
    int row = ui->list->currentRow();
    PropertyEnum *pe = professionsEnum();
    QString profession = pe->values().at(row);

    int ret = QMessageBox::warning(this, tr("Remove Profession"),
                                   tr("Removing this profession will affect all spawnpoints in the world!\nReally remove the '%1' profession?").arg(profession),
                                   QMessageBox::Yes, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    worldDocument()->removePropertyEnumChoice(pe, row);
}

void SpawnToolDialog::itemChanged(QListWidgetItem *item)
{
    int row = ui->list->row(item);
    if (item->text() != professions().at(row)) {
        QString name = makeNameUnique(item->text(), row);
        if (name != professions().at(row))
            worldDocument()->renamePropertyEnumChoice(professionsEnum(), row, name);
        else
            item->setText(name);
        return;
    }
    if (item->checkState() == Qt::Checked) {
        if (item->text() == QLatin1String("all")) {
            for (int i = 0; i < ui->list->count(); i++) {
                QListWidgetItem *item2 = ui->list->item(i);
                if (item2 != item)
                    item2->setCheckState(Qt::Unchecked);
            }
        } else {
            for (int i = 0; i < ui->list->count(); i++) {
                QListWidgetItem *item2 = ui->list->item(i);
                if (item2->text() == QLatin1String("all"))
                    item2->setCheckState(Qt::Unchecked);
            }
        }
    }
    if (selectedSpawnPoints().size())
        toObjects();
}

void SpawnToolDialog::currentRowChanged(int row)
{
    ui->remove->setEnabled(row != -1);
}

static void resolveProperties(PropertyHolder *ph, PropertyList &result)
{
    foreach (PropertyTemplate *pt, ph->templates())
        resolveProperties(pt, result);
    foreach (Property *p, ph->properties()) {
        result.removeAll(p->mDefinition);
        result += p;
    }
}

void SpawnToolDialog::setList()
{
    ui->list->clear();

    // Count the number of checked professions for all selected spawnpoints.
    QStringList professions = this->professions();
    QList<WorldCellObject*> spawnPoints = selectedSpawnPoints();
    QVector<int> checkCount(professions.size());
    checkCount.fill(0);
    PropertyDef *pd = mDocument->world()->propertyDefinition(QLatin1String("Professions"));
    if (spawnPoints.size() && pd) {
        foreach (WorldCellObject *obj, spawnPoints) {
            PropertyList properties;
            resolveProperties(obj, properties);
            if (Property *p = properties.find(pd)) {
                QStringList choices = p->mValue.split(QLatin1String(","), QString::SkipEmptyParts);
                foreach (QString choice, choices) {
                    int index = professions.indexOf(choice);
                    if (index >= 0) {
                        checkCount[index]++;
                    }
                }
            }
        }

    }

    for (int i = 0; i < professions.size(); i++) {
        QListWidgetItem *item = new QListWidgetItem(professions[i]);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if ((checkCount[i] == spawnPoints.size()) && checkCount[i])
            item->setCheckState(Qt::Checked);
        else if (checkCount[i] == 0)
            item->setCheckState(Qt::Unchecked);
        else
            item->setCheckState(Qt::PartiallyChecked);
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
    if (professions.contains(QLatin1String("all")))
        professions = QStringList(QLatin1String("all"));
    QString value = professions.join(QLatin1String(","));

    foreach (WorldCellObject *obj, selectedSpawnPoints()) {
        if (Property *p = obj->properties().find(pd)) {
            if (p->mValue != value)
                mDocument->worldDocument()->setPropertyValue(obj, p, value);
        } else
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

QStringList SpawnToolDialog::professions()
{
    PropertyEnum *pe = professionsEnum();
    return pe ? pe->values() : QStringList();
}

PropertyEnum *SpawnToolDialog::professionsEnum()
{
    return mDocument->world()->propertyEnums().find(QLatin1String("Professions"));
}

QString SpawnToolDialog::makeNameUnique(const QString &name, int ignore)
{
    QString unique = name;
    int n = 1;
    while (1) {
        int index = professions().indexOf(unique);
        if ((index < 0) || (index == ignore)) break;
        unique = QString::fromLatin1("%1_%2").arg(name).arg(n++);
    }
    return unique;
}
