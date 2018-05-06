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

#include "searchdock.h"
#include "ui_searchdock.h"

#include "celldocument.h"
#include "documentmanager.h"
#include "mainwindow.h"
#include "world.h"
#include "worldcell.h"
#include "worlddocument.h"
#include "worldscene.h"
#include "worldview.h"

#include <QListWidget>
#include <QListWidgetItem>

SearchDock::SearchDock(QWidget* parent)
    : QDockWidget(parent)
    , ui(new Ui::SearchDock)
    , mSynching(false)
{
    ui->setupUi(this);

    setObjectName(QLatin1String("SearchDock"));

    connect(ui->combo1, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated1);
    connect(ui->combo2, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated2);
    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &SearchDock::listSelectionChanged);
    connect(ui->listWidget, &QListWidget::activated, this, &SearchDock::listActivated);

    connect(DocumentManager::instance(), &DocumentManager::documentAboutToClose, this, &SearchDock::documentAboutToClose);
}

void SearchDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void SearchDock::retranslateUi()
{
    setWindowTitle(tr("Search"));
}

void SearchDock::setDocument(Document *doc)
{
    mWorldDoc = doc->asWorldDocument();
    mCellDoc = doc->asCellDocument();
    setEnabled(true);

    comboActivated1(0);

    WorldDocument* worldDoc = worldDocument();
    SearchResults* results = mResults.contains(worldDoc) ? mResults[worldDoc] : nullptr;
    setList(results);
    if (results && ui->combo1->currentIndex() == 0) {
        int index = ui->combo2->findText(results->objectType);
        ui->combo2->setCurrentIndex(index);
    }
}

void SearchDock::clearDocument()
{
    mWorldDoc = nullptr;
    mCellDoc = nullptr;
    ui->listWidget->clear();
    ui->combo2->clear();
    setEnabled(false);
}

WorldDocument *SearchDock::worldDocument()
{
    if (mWorldDoc)
        return mWorldDoc;
    if (mCellDoc)
        return mCellDoc->worldDocument();
    return nullptr;
}

void SearchDock::comboActivated1(int index)
{
    ui->combo2->clear();

    if (worldDocument() == nullptr)
        return;
    World* world = worldDocument()->world();

    if (index == 0) {
        for (ObjectType* objType : world->objectTypes()) {
            if (objType->isNull())
                ui->combo2->addItem(QLatin1String("<None>"), QVariant::fromValue(objType));
            else
                ui->combo2->addItem(objType->name(), QVariant::fromValue(objType));
        }
        ui->combo2->setCurrentIndex(0);
    }
}

void SearchDock::comboActivated2(int index)
{
    Q_UNUSED(index);
    search();
}

void SearchDock::search()
{
    World* world = mWorldDoc ? mWorldDoc->world() : mCellDoc->world();

    ObjectType* objType = ui->combo2->currentData().value<ObjectType*>();

    SearchResults* results = mResults.contains(mWorldDoc) ? mResults[mWorldDoc] : new SearchResults();
    results->reset();
    results->objectType = objType->name();

    for (int y = 0; y < world->height(); y++) {
        for (int x = 0; x < world->width(); x++) {
            WorldCell* cell = world->cellAt(x, y);
            if (cell == nullptr)
                continue;
            for (WorldCellObject* obj : cell->objects()) {
                if (obj->type() == objType) {
                    // FIXME: If the world is resized, these cells may be destroyed.
                    results->cells += cell;
                    break;
                }
            }
        }
    }

    mResults[mWorldDoc] = results;

    setList(results);
}

void SearchDock::setList(SearchResults *results)
{
    mSynching = true;
//    WorldCell* selected = results ? results->selectedCell : nullptr;
    ui->listWidget->clear();
    mSynching = false;

    if (results == nullptr)
        return;

//    results->selectedCell = selected;
    QListWidgetItem* selectedItem = nullptr;

    for (WorldCell* cell : results->cells) {
        QListWidgetItem* item = new QListWidgetItem(tr("Cell %1,%2").arg(cell->x()).arg(cell->y()));
        item->setData(Qt::UserRole, QVariant::fromValue(cell));
        ui->listWidget->addItem(item);

        if (cell == results->selectedCell)
            selectedItem = item;
    }

    if (selectedItem != nullptr) {
        mSynching = true;
        selectedItem->setSelected(true);
        mSynching = false;
        ui->listWidget->scrollToItem(selectedItem);
    }
}

void SearchDock::listSelectionChanged()
{
    if (mSynching)
        return;

    WorldDocument* worldDoc = worldDocument();
    if (worldDoc == nullptr)
        return;

    if (!mResults.contains(worldDoc))
        return;
    SearchResults* results = mResults[worldDoc];

    QList<QListWidgetItem*> selected = ui->listWidget->selectedItems();
    if (selected.size() != 1) {
        results->selectedCell = nullptr;
        return;
    }

    QListWidgetItem* item = selected.first();
    WorldCell* cell = item->data(Qt::UserRole).value<WorldCell*>();
    results->selectedCell = cell;

    WorldScene* scene = worldDoc->view()->scene()->asWorldScene();
    worldDoc->view()->centerOn(scene->cellToPixelCoords(cell->x() + 0.5f, cell->y() + 0.5f));
}

void SearchDock::listActivated(const QModelIndex &index)
{
    QListWidgetItem* item = ui->listWidget->item(index.row());
    WorldCell* cell = item->data(Qt::UserRole).value<WorldCell*>();
    WorldDocument* worldDoc = mWorldDoc ? mWorldDoc : mCellDoc->worldDocument();
    worldDoc->setSelectedCells(QList<WorldCell*>() << cell);
    worldDoc->editCell(cell);
}

void SearchDock::documentAboutToClose(int index, Document *doc)
{
    WorldDocument* worldDoc = doc->asWorldDocument() ? doc->asWorldDocument() : doc->asCellDocument()->worldDocument();
    if (mResults.contains(worldDoc)) {
        SearchResults* results = mResults[worldDoc];
        mResults.remove(worldDoc);
        delete results;
    }
}
