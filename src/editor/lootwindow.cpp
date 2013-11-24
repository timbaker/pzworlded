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

#include "lootwindow.h"
#include "ui_lootwindow.h"

#include "celldocument.h"
#include "cellscene.h"
#include "cellview.h"
#include "documentmanager.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "worldcell.h"

#include "maprenderer.h"
#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QFileDialog>
#include <QSettings>

using namespace Tiled;

SINGLETON_IMPL(LootWindow)

LootWindow::LootWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::LootWindow),
    mDocument(0),
    mShowing(false)
{
    ui->setupUi(this);

    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));
    connect(ui->gameDirBrowse, SIGNAL(clicked()), SLOT(chooseGameDirectory()));
    connect(ui->treeWidget->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));

    QSettings settings;
    QString d = settings.value(QLatin1String("LootWindow/GameDirectory")).toString();
    ui->gameDirectory->setText(QDir::toNativeSeparators(d));

    connect(DocumentManager::instance(), SIGNAL(currentDocumentChanged(Document*)),
            SLOT(setDocument(Document*)));

    if (QFileInfo(d).exists()) {
        readTileProperties(QDir(d).filePath(QLatin1String("media/newtiledefinitions.tiles")));
        readLuaDistributions(QDir(d).filePath(QLatin1String("media/lua/Items/SuburbsDistributions.lua")));
    }

    mTimer.setSingleShot(true);
    connect(&mTimer, SIGNAL(timeout()), SLOT(updateNow()));
}

LootWindow::~LootWindow()
{
    delete ui;
}

void LootWindow::setDocument(Document *doc)
{
    if (mDocument) {
        mDocument->scene()->disconnect(this); // FIXME: handle closing document
    }

    mDocument = doc ? doc->asCellDocument() : 0;

    if (mDocument) {
        connect(mDocument->scene(), SIGNAL(mapContentsChanged()),
                SLOT(mapContentsChanged()));
    }

    if (mDocument && mShowing)
        examineMapComposite(mDocument->scene()->mapComposite());
    else
        ui->treeWidget->clear();
}

bool LootWindow::readTileProperties(const QString &fileName)
{
    return mTileDefFile.read(fileName);
}

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

bool LootWindow::readLuaDistributions(const QString &fileName)
{
    mDistributions.clear();

    // SuburbsDistributions
    //   room
    //     container
    //     container
    //   room
    //     container
    //     container
    //     container
    // FIXME: this is totally unsafe
    if (QFileInfo(fileName).exists()) {
        if (lua_State *L = luaL_newstate()) {
            luaL_openlibs(L);
            int status = luaL_loadfile(L, fileName.toLatin1().data());
            if (status == LUA_OK) {
                status = lua_pcall(L, 0, 0, -1); // call the closure?
                if (status == LUA_OK) {
                    lua_getglobal(L, "SuburbsDistributions");
                    int tblidx = lua_gettop(L);
                    if (lua_istable(L, tblidx)) {
                        lua_pushnil(L); // push space on stack for the key
                        while (lua_next(L, tblidx) != 0) {
                            QString roomName(QLatin1String(lua_tostring(L, -2)));
                            // value at -1 is the table of items in the room
                            lua_pushnil(L); // space for key
                            while (lua_next(L, -2) != 0) {
                                QString containerName(QLatin1String(lua_tostring(L, -2)));
                                mDistributions[roomName] += containerName;
                                lua_pop(L, 1); // pop value
                            }
                            lua_pop(L, 1); // pop space for the room key
                        }
                        lua_pop(L, 1); // pop space for the key
                    }
                }
            }
            lua_close(L);
        }
    }

    return true;
}

void LootWindow::examineMapComposite(MapComposite *mc)
{
    mMapBuildings.calculate(mc);

    mBuildingMap.clear();
    qDeleteAll(mBuildings);
    mBuildings.clear();
    qDeleteAll(mLooseContainers);
    mLooseContainers.clear();

    static QVector<const Tiled::Cell*> cells(40);
    foreach (CompositeLayerGroup *lg, mc->sortedLayerGroups()) {
        lg->prepareDrawing2();
        for (int y = 0; y < mc->mapInfo()->height(); y++) {
            for (int x = 0; x < mc->mapInfo()->width(); x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                foreach (const Tiled::Cell *cell, cells) {
                    examineTile(x, y, lg->level(), cell->tile);
                }
            }
        }
    }

    setList();
}

void LootWindow::examineTile(int x, int y, int z, Tile *tile)
{
    if (TileDefTileset *ts = mTileDefFile.tileset(tile->tileset()->name())) {
        if (TileDefTile *tdt = ts->tile(tile->id() % tile->tileset()->columnCount(),
                                        tile->id() / tile->tileset()->columnCount())) {
            foreach (QString key, tdt->mProperties.keys()) {
                if (key == QLatin1String("container")) {
                    addContainer(x, y, z, tdt->mProperties[key]);
                }
            }
        }
    }
}

void LootWindow::addContainer(int x, int y, int z, const QString &container)
{
    if (MapBuildingsNS::Room *r = mMapBuildings.roomAt(QPoint(x, y), z)) {
        if (!mBuildingMap.contains(r->building)) {
            mBuildingMap[r->building] = new Building;
            mBuildings += mBuildingMap[r->building];
        }
        mBuildingMap[r->building]->mContainers += new Container(r, x, y, z, container);
    } else {
        mLooseContainers += new Container(NULL, x, y, z, container);
    }
}

Q_DECLARE_METATYPE(LootWindow::Container*)

void LootWindow::setList()
{
    QString all(QLatin1String("all"));

    ui->treeWidget->clear();
//    ui->treeWidget->setColumnCount(2);
    ui->treeWidget->setHeaderLabels(QStringList() << tr("Container type") << tr("Room name") << tr("Cell position"));
    ui->treeWidget->setColumnWidth(0, 256);
    ui->treeWidget->setColumnWidth(1, 256);
    ui->treeWidget->setHeaderHidden(false);

    if (mLooseContainers.size()) {
        QTreeWidgetItem *bItem = new QTreeWidgetItem(ui->treeWidget, QStringList() << tr("Containers not in rooms"));
        int worst = 3;
        foreach (Container *c, mLooseContainers) {
            QString posStr = tr("%1,%2,%3").arg(c->mX).arg(c->mY).arg(c->mZ);
            QTreeWidgetItem *cItem = new QTreeWidgetItem(bItem, QStringList() << c->mType << tr("<none>") << posStr);
            cItem->setData(0, Qt::UserRole, QVariant::fromValue(c));
            if (mDistributions.contains(all) && mDistributions[all].contains(c->mType)) {
                cItem->setBackgroundColor(0, Qt::yellow);
                worst = qMin(worst, 2);
            } else {
                cItem->setBackgroundColor(0, Qt::red);
                worst = qMin(worst, 1);
            }
        }
        if (worst == 3) bItem->setBackgroundColor(0, Qt::green);
        else if (worst == 2) bItem->setBackgroundColor(0, Qt::yellow);
        else if (worst == 1) bItem->setBackgroundColor(0, Qt::red);
    }

    int i = 1;
    foreach (Building *b, mBuildings) {
        QTreeWidgetItem *bItem = new QTreeWidgetItem(ui->treeWidget, QStringList() << tr("Building %1").arg(i));
        int worst = 3;
        foreach (Container *c, b->mContainers) {
            QString roomName = c->mRoom ? c->mRoom->name : tr("<outside>");
            QString posStr = tr("%1,%2,%3").arg(c->mX).arg(c->mY).arg(c->mZ);
            QTreeWidgetItem *cItem = new QTreeWidgetItem(bItem, QStringList() << c->mType << roomName << posStr);
            cItem->setData(0, Qt::UserRole, QVariant::fromValue(c));
            if (mDistributions.contains(c->mRoom->name) && mDistributions[c->mRoom->name].contains(c->mType)) {
                cItem->setBackgroundColor(0, Qt::green);
            } else if (mDistributions.contains(all) && mDistributions[all].contains(c->mType)) {
                cItem->setBackgroundColor(0, Qt::yellow);
                worst = qMin(worst, 2);
            } else {
                cItem->setBackgroundColor(0, Qt::red);
                worst = qMin(worst, 1);
            }
        }
        if (worst == 3) bItem->setBackgroundColor(0, Qt::green);
        else if (worst == 2) bItem->setBackgroundColor(0, Qt::yellow);
        else if (worst == 1) bItem->setBackgroundColor(0, Qt::red);
        i++;
    }
}

void LootWindow::chooseGameDirectory()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Choose Game Directory"),
                                                  ui->gameDirectory->text());
    if (f.isEmpty())
        return;

    QSettings settings;
    settings.setValue(QLatin1String("LootWindow/GameDirectory"), f);

    ui->gameDirectory->setText(QDir::toNativeSeparators(f));

    readTileProperties(QDir(f).filePath(QLatin1String("media/newtiledefinitions.tiles")));
    readLuaDistributions(QDir(f).filePath(QLatin1String("media/lua/Items/SuburbsDistributions.lua")));
}

void LootWindow::selectionChanged()
{
    QList<QTreeWidgetItem*> selected = ui->treeWidget->selectedItems();
    if (selected.size() == 1) {
        QTreeWidgetItem *item = selected.first();
        if (item->data(0, Qt::UserRole).canConvert<Container*>()) {
            if (Container *c = qvariant_cast<Container*>(item->data(0, Qt::UserRole))) {
                if (Preferences::instance()->highlightCurrentLevel())
                    mDocument->setCurrentLevel(c->mZ);
                if (Preferences::instance()->highlightRoomUnderPointer())
                    mDocument->scene()->setHighlightRoomPosition(QPoint(c->mX, c->mY));
                QPointF pos = mDocument->scene()->renderer()->tileToPixelCoords(c->mX, c->mY, c->mZ);
                mDocument->view()->centerOn(pos);
            }
        }
    }
}

void LootWindow::mapContentsChanged()
{
    mTimer.start(1500);
}

void LootWindow::updateNow()
{
    if (mDocument && mShowing)
        examineMapComposite(mDocument->scene()->mapComposite());
}

void LootWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    mShowing = true;
    updateNow();
}

void LootWindow::hideEvent(QHideEvent *e)
{
    QMainWindow::hideEvent(e);
    mShowing = false;
}
