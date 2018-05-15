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

    ui->lineEdit->setVisible(false);

    connect(ui->combo1, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated1);
    connect(ui->combo2, QOverload<int>::of(&QComboBox::activated), this, &SearchDock::comboActivated2);
    connect(ui->lineEdit, &QLineEdit::textChanged, this, &SearchDock::searchLotFileName);

    connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &SearchDock::listSelectionChanged);
    connect(ui->listWidget, &QListWidget::activated, this, &SearchDock::listActivated);

    connect(DocumentManager::instance(), &DocumentManager::documentAboutToClose, this, &SearchDock::documentAboutToClose);

    setEnabled(false);
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

    WorldDocument* worldDoc = worldDocument();
    SearchResults* results = searchResultsFor(worldDoc);
    ui->combo1->setCurrentIndex(static_cast<int>(results->searchBy));
    setCombo2(results->searchBy);
    setList(results);

    if (results->searchBy == SearchResults::SearchBy::ObjectType) {
        int index = ui->combo2->findText(results->searchStringObjectType);
        if (index == -1)
            index = 0; // Select <None>
        ui->combo2->setCurrentIndex(index);
        ui->combo2->setVisible(true);
        ui->lineEdit->setVisible(false);
    } else if (results->searchBy == SearchResults::SearchBy::LotFileName) {
        ui->lineEdit->setText(results->searchStringLotFileName);
        ui->combo2->setVisible(false);
        ui->lineEdit->setVisible(true);
    }
}

void SearchDock::clearDocument()
{
    mWorldDoc = nullptr;
    mCellDoc = nullptr;
    ui->listWidget->clear();
    ui->combo2->clear();
    ui->lineEdit->clear();
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

    WorldDocument *worldDoc = worldDocument();
    if (worldDoc == nullptr)
        return;

    SearchResults *results = searchResultsFor(worldDoc);

    if (index == static_cast<int>(SearchResults::SearchBy::ObjectType)) {
        setCombo2(SearchResults::SearchBy::ObjectType);
        int index = ui->combo2->findText(results->searchStringObjectType);
        if (index == -1)
            index = 0; // Select <None>
        ui->combo2->setCurrentIndex(index);
        ui->combo2->setVisible(true);
        ui->lineEdit->setVisible(false);
        searchObjectType();
    }
    if (index == static_cast<int>(SearchResults::SearchBy::LotFileName)) {
        ui->lineEdit->setText(results->searchStringLotFileName);
        ui->combo2->setVisible(false);
        ui->lineEdit->setVisible(true);
        searchLotFileName();
    }
}

void SearchDock::comboActivated2(int index)
{
    Q_UNUSED(index);
    searchObjectType();
}

void SearchDock::searchObjectType()
{
    WorldDocument *worldDoc = worldDocument();
    if (worldDoc == nullptr)
        return;
    World* world = worldDoc->world();

    ObjectType* objType = ui->combo2->currentData().value<ObjectType*>();

    SearchResults* results = searchResultsFor(worldDoc);
    results->reset();
    results->searchBy = SearchResults::SearchBy::ObjectType;
    results->searchStringObjectType = objType->name();

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

    setList(results);
}

void SearchDock::searchLotFileName()
{
    WorldDocument *worldDoc = worldDocument();
    if (worldDoc == nullptr)
        return;
    World* world = worldDoc->world();

    SearchResults* results = searchResultsFor(worldDoc);
    results->reset();
    results->searchBy = SearchResults::SearchBy::LotFileName;
    results->searchStringLotFileName = ui->lineEdit->text();

    for (int y = 0; y < world->height(); y++) {
        for (int x = 0; x < world->width(); x++) {
            WorldCell* cell = world->cellAt(x, y);
            if (cell == nullptr)
                continue;
            for (WorldCellLot* lot : cell->lots()) {
                if (lot->mapName().contains(results->searchStringLotFileName, Qt::CaseInsensitive)) {
                    // FIXME: If the world is resized, these cells may be destroyed.
                    results->cells += cell;
                    break;
                }
            }
        }
    }

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

    SearchResults* results = searchResultsFor(worldDoc, false);
    if (results == nullptr)
        return;

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
    worldDoc->setSelectedCells(QList<WorldCell*>() << cell);
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
    Q_UNUSED(index)
    WorldDocument* worldDoc = doc->asWorldDocument() ? doc->asWorldDocument() : doc->asCellDocument()->worldDocument();
    if (mResults.contains(worldDoc)) {
        SearchResults* results = mResults[worldDoc];
        mResults.remove(worldDoc);
        delete results;
    }
}

void SearchDock::setCombo2(SearchResults::SearchBy searchBy)
{
    ui->combo2->clear();

    WorldDocument *worldDoc = worldDocument();
    if (worldDoc == nullptr)
        return;
    World* world = worldDoc->world();

    if (searchBy == SearchResults::SearchBy::ObjectType) {
        for (ObjectType* objType : world->objectTypes()) {
            if (objType->isNull())
                ui->combo2->addItem(QLatin1String("<None>"), QVariant::fromValue(objType));
            else
                ui->combo2->addItem(objType->name(), QVariant::fromValue(objType));
        }
    }
}

SearchResults *SearchDock::searchResultsFor(WorldDocument *worldDoc, bool create)
{
    if (worldDoc == nullptr)
        return nullptr;
    if (mResults.contains(worldDoc))
        return mResults[worldDoc];
    if (!create)
        return nullptr;
    return mResults[worldDoc] = new SearchResults();
}
