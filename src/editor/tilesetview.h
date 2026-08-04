/*
 * tilesetview.h
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef TILESETVIEW_H
#define TILESETVIEW_H

#include "tilesetmodel.h"

#include <QTableView>

#ifdef WORLDED
class Zoomable;
#endif

namespace Tiled {
namespace Internal {

#ifndef WORLDED
class MapDocument;
class Zoomable;
#endif

/**
 * The tileset view. May only be used with the TilesetModel.
 */
class TilesetView : public QTableView
{
    Q_OBJECT

public:
#ifdef ZOMBOID
    TilesetView(QWidget *parent = 0);
    TilesetView(Zoomable *zoomable, QWidget *parent = 0);

    void setMapDocument(MapDocument *mapDocument);
#else
    TilesetView(MapDocument *mapDocument, Zoomable *zoomable, QWidget *parent = 0);
#endif
    QSize sizeHint() const;

    Zoomable *zoomable() const { return mZoomable; }

    bool drawGrid() const { return mDrawGrid; }
#ifdef ZOMBOID
    void setShowLayerNames(bool show) { mShowLayerNames = show; }
    bool showLayerNames() const;
#endif

    /**
     * Convenience method that returns the model as a TilesetModel.
     */
    TilesetModel *tilesetModel() const
    { return static_cast<TilesetModel *>(model()); }

protected:
    void wheelEvent(QWheelEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);

private slots:
#ifndef WORLDED
    void editTileProperties();
    void setDrawGrid(bool drawGrid);
#endif // WORLDED
    void adjustScale();

#ifdef ZOMBOID
    // Preferences signal
    void autoSwitchLayerChanged(bool enabled);
#endif

private:
    Zoomable *mZoomable;
    MapDocument *mMapDocument;
    bool mDrawGrid;
#ifdef ZOMBOID
    bool mShowLayerNames;
#endif
};

} // namespace Internal
} // namespace Tiled

Q_DECLARE_METATYPE(Tiled::Internal::TilesetView *)

#endif // TILESETVIEW_H
