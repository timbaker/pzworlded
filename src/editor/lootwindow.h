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

#ifndef LOOTWINDOW_H
#define LOOTWINDOW_H

#include "singleton.h"
#include "mapbuildings.h"
#include "tiledeffile.h"

#include <QMainWindow>
#include <QTimer>

class CellDocument;
class Document;
class MapComposite;

namespace Tiled
{
class Tile;
}

namespace Ui {
class LootWindow;
}


class LootWindow : public QMainWindow, public Singleton<LootWindow>
{
    Q_OBJECT
public:
    explicit LootWindow(QWidget *parent = 0);
    ~LootWindow();

    class Container
    {
    public:
        Container(MapBuildingsNS::Room *r, int x, int y, int z, const QString &type) :
            mRoom(r),
            mX(x),
            mY(y),
            mZ(z),
            mType(type)
        {

        }

        MapBuildingsNS::Room *mRoom;
        QString mType;
        int mX;
        int mY;
        int mZ;
    };

    class Building
    {
    public:
        ~Building()
        {
            qDeleteAll(mContainers);
        }

        QString mFileName;
        QList<Container*> mContainers;
    };

public slots:
    void setDocument(Document *doc);
    void chooseGameDirectory();
    void selectionChanged();
    void mapContentsChanged();
    void updateNow();

private:
    void showEvent(QShowEvent *e);
    void hideEvent(QHideEvent *e);

    bool readTileProperties(const QString &fileName);
    bool readLuaDistributions(const QString &fileName);
    void examineMapComposite(MapComposite *mc);
    void examineTile(int x, int y, int z, Tiled::Tile *tile);
    void addContainer(int x, int y, int z, const QString &container);
    void setList();

private:
    Ui::LootWindow *ui;
    CellDocument *mDocument;
    bool mShowing;
    TileDefFile mTileDefFile;
    MapBuildings mMapBuildings;

    QMap<MapBuildingsNS::Building*,Building*> mBuildingMap;
    QList<Building*> mBuildings;
    QList<Container*> mLooseContainers;

    QMap<QString,QStringList> mDistributions;

    QTimer mTimer;
};

#endif // LOOTWINDOW_H
