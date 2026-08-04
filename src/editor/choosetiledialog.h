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

#ifndef CHOOSETILEDIALOG_H
#define CHOOSETILEDIALOG_H

#include <QDialog>

namespace Ui {
class ChooseTileDialog;
}

namespace Tiled {
class Tile;
}

class QSettings;
class QSplitter;

class ChooseTileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChooseTileDialog(QWidget *parent = nullptr);
    ~ChooseTileDialog();

    Tiled::Tile *chosenTile() const;

private slots:
    void tilesetRowChanged(int row);
    void tilesetFilterEdited(const QString &text);
    void accept() override;
    void reject() override;

private:
    void setTilesetList();
    void setTilesetView();
    void saveSplitterSizes(QSplitter *splitter, QSettings &settings);
    void restoreSplitterSizes(QSplitter *splitter, QSettings &settings);

    void saveSettings();
    void readSettings();

private:
    Ui::ChooseTileDialog *ui;
};

#endif // CHOOSETILEDIALOG_H
