/*
 * tilesetmodel.cpp
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 *
 * This file is part of Tiled.
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

#include "tilesetmodel.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"
#ifdef ZOMBOID
#ifndef WORLDED
#include "mapdocument.h"
#endif
#include "tilesetmanager.h"
#endif

using namespace Tiled;
using namespace Tiled::Internal;

TilesetModel::TilesetModel(Tileset *tileset, QObject *parent):
    QAbstractListModel(parent),
#ifdef ZOMBOID
    mMapDocument(0),
#endif
    mTileset(tileset)
{
}

#ifdef ZOMBOID
TilesetModel::~TilesetModel()
{
}

#ifndef WORLDED
void TilesetModel::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;

    if (mMapDocument)
        connect(mMapDocument, &MapDocument::tileLayerNameChanged,
                this, &TilesetModel::tileLayerNameChanged);
}
#endif

void TilesetModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}
#endif

int TilesetModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

#ifdef ZOMBOID
    if (!mTileset)
        return 0;
#endif

    const int tiles = mTileset->tileCount();
    const int columns = mTileset->columnCount();

    int rows = 1;
    if (columns > 0) {
        rows = tiles / columns;
        if (tiles % columns > 0)
            ++rows;
    }

    return rows;
}

int TilesetModel::columnCount(const QModelIndex &parent) const
{
#ifdef ZOMBOID
    if (!mTileset)
        return 0;
#endif
    return parent.isValid() ? 0 : mTileset->columnCount();
}

QVariant TilesetModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (Tile *tile = tileAt(index))
            return tile->image();
    }
#ifdef ZOMBOID_TILE_LAYER_NAMES
    if (role == Qt::DecorationRole) {
        if (Tile *tile = tileAt(index))
            return TilesetManager::instance()->layerName(tile);
    }
    if (role == Qt::ToolTipRole) {
        if (Tile *tile = tileAt(index))
            return TilesetManager::instance()->layerName(tile);
    }
#endif

    return QVariant();
}

QVariant TilesetModel::headerData(int /* section */,
                                  Qt::Orientation /* orientation */,
                                  int role) const
{
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

Tile *TilesetModel::tileAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    const int i = index.column() + index.row() * mTileset->columnCount();
    return mTileset->tileAt(i);
}

void TilesetModel::setTileset(Tileset *tileset)
{
    if (mTileset == tileset)
        return;
    beginResetModel();
    mTileset = tileset;
    endResetModel();
}

#ifdef ZOMBOID
void TilesetModel::tileLayerNameChanged(Tile *tile)
{
    if (tile->tileset() == mTileset) {
        int column = tile->id() % mTileset->columnCount();
        int row = tile->id() / mTileset->columnCount();
        QModelIndex index = createIndex(row, column, (void*)0);
        emit dataChanged(index, index);
    }
}
#endif
