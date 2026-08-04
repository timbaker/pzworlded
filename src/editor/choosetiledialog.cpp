/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "choosetiledialog.h"
#include "ui_choosetiledialog.h"

#include "tilemetainfomgr.h"

#include "tile.h"

#include <QSettings>
#include <QSplitter>

static QString SETTINGS_KEY(QStringLiteral("ChooseTileDialog"));

ChooseTileDialog::ChooseTileDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChooseTileDialog)
{
    ui->setupUi(this);

    ui->splitter->setObjectName(QStringLiteral("ChooseTileDialog.splitter"));

    ui->tilesetView->setModel(new Tiled::Internal::TilesetModel(nullptr, ui->tilesetView));

    connect(ui->tilesetFilter, &QLineEdit::textEdited, this, &ChooseTileDialog::tilesetFilterEdited);
    connect(ui->tilesetsList, &QListWidget::currentRowChanged, this, &ChooseTileDialog::tilesetRowChanged);

    readSettings();

    setTilesetList();
}

ChooseTileDialog::~ChooseTileDialog()
{
    delete ui;
}

Tiled::Tile *ChooseTileDialog::chosenTile() const
{
    QModelIndexList selection = ui->tilesetView->selectionModel()->selectedIndexes();
    if (selection.size() != 1) {
        return nullptr;
    }
    return ui->tilesetView->tilesetModel()->tileAt(selection.first());
}

void ChooseTileDialog::tilesetRowChanged(int row)
{
    setTilesetView();
}

void ChooseTileDialog::tilesetFilterEdited(const QString &text)
{
    QListWidget* listView = ui->tilesetsList;

    for (int row = 0; row < listView->count(); row++) {
        QListWidgetItem* item = listView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = listView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = listView->row(current) - 1;
        while (row >= 0 && listView->item(row)->isHidden()) {
            row--;
        }
        if (row >= 0) {
            current = listView->item(row);
            listView->setCurrentItem(current);
            listView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = listView->row(current) + 1;
        while (row < listView->count() && listView->item(row)->isHidden()) {
            row++;
        }
        if (row < listView->count()) {
            current = listView->item(row);
            listView->setCurrentItem(current);
            listView->scrollToItem(current);
            return;
        }

        // All items hidden
        listView->setCurrentItem(nullptr);
    }

    current = listView->currentItem();
    if (current != nullptr) {
        listView->scrollToItem(current);
    }
}

void ChooseTileDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void ChooseTileDialog::reject()
{
    saveSettings();
    QDialog::reject();
}

void ChooseTileDialog::setTilesetList()
{
    ui->tilesetsList->addItems(Tiled::TileMetaInfoMgr::instance()->tilesetNames());
}

void ChooseTileDialog::setTilesetView()
{
    ui->tilesetView->tilesetModel()->setTileset(nullptr);
    int row = ui->tilesetsList->currentRow();
    if (row == -1) {
        return;
    }
    Tiled::Tileset *tileset = Tiled::TileMetaInfoMgr::instance()->tileset(row);
    ui->tilesetView->tilesetModel()->setTileset(tileset);
}

void ChooseTileDialog::saveSplitterSizes(QSplitter *splitter, QSettings &settings)
{
    QVariantList v;
    for (int size : splitter->sizes()) {
        v += size;
    }
    settings.setValue(tr("%1.sizes").arg(splitter->objectName()), v);
}

void ChooseTileDialog::restoreSplitterSizes(QSplitter *splitter, QSettings &settings)
{
    const QVariant v = settings.value(tr("%1.sizes").arg(splitter->objectName()));
    if (v.canConvert(QMetaType::QVariantList)) {
        QList<int> sizes;
        for (const QVariant &v2 : v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
}

void ChooseTileDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_KEY);
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    saveSplitterSizes(ui->splitter, settings);
    settings.endGroup();
}

void ChooseTileDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(SETTINGS_KEY);
    QByteArray geom = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    restoreSplitterSizes(ui->splitter, settings);
    settings.endGroup();
}

