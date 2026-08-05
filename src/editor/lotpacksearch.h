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

#ifndef LOTPACKSEARCH_H
#define LOTPACKSEARCH_H

#include <QMainWindow>

namespace Ui {
class LotPackSearch;
}

class LotHeader;
class LotPackWindow;

namespace Tiled {
class Tile;
}

class LotPackSearch : public QMainWindow
{
    Q_OBJECT

public:
    explicit LotPackSearch(QWidget *parent = nullptr);
    ~LotPackSearch();

    void setLotPackWindow(LotPackWindow *window)
    { mLotPackWindow = window; }

private:
    struct SearchResult
    {
        QString tileName;
        int x;
        int y;
        int z;
    };

    bool isTileAddedAlready(const QString &tileName) const;
    bool containsAny(const QStringList &haystack, const QStringList &needles, QSet<QString> &contains);
    void searchCell(int cellX, int cellY, LotHeader *lotHeader, const QSet<QString> &tilesToFind);
    void addResult(const QString &tileName, int cellX, int cellY, int x, int y, int z);

private slots:
    void addTile();
    void removeTile();
    void search();
    void openCell();
    void currentResultChanged(int row);
    void synchUI();

private:
    Ui::LotPackSearch *ui;
    LotPackWindow *mLotPackWindow;
    QList<SearchResult> mResults;
};

#endif // LOTPACKSEARCH_H
