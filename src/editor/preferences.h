/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QObject>
#include <QColor>
#include <QStringList>

class QSettings;

class Preferences : public QObject
{
    Q_OBJECT
public:
    static Preferences *instance();
    static void deleteInstance();

    QString configPath() const;
    QString configPath(const QString &fileName) const;

    QString appConfigPath() const;
    QString appConfigPath(const QString &fileName) const;

    QString docsPath() const;
    QString docsPath(const QString &fileName) const;

    QString luaPath() const;
    QString luaPath(const QString &fileName) const;

    bool snapToGrid() const;
    bool showCoordinates() const;
    bool showWorldGrid() const;
    bool showCellGrid() const;
    QColor gridColor() const { return mGridColor; }
    bool showMiniMap() const;
    int miniMapWidth() const;
    bool highlightCurrentLevel() const;
    bool highlightRoomUnderPointer() const
    { return mHighlightRoomUnderPointer; }

    QString mapsDirectory() const;
    void setMapsDirectory(const QString &path);

    QString tilesDirectory() const;
    void setTilesDirectory(const QString &path);

    QString texturesDirectory() const;

    bool useOpenGL() const { return mUseOpenGL; }
    void setUseOpenGL(bool useOpenGL);

    bool worldThumbnails() const { return mWorldThumbnails; }
    void setWorldThumbnails(bool thumbs);

    bool showObjectNames() const { return mShowObjectNames; }
    bool showBMPs() const { return mShowBMPs; }

    QString openFileDirectory() const;
    void setOpenFileDirectory(const QString &path);

    bool showAdjacentMaps() const { return mShowAdjacentMaps; }
    void setShowAdjacentMaps(bool show);

    int heightMapDisplayStyle() const
    { return mHeightMapDisplayStyle; }

    bool useVirtualTilesets() const
    { return mUseVirtualTilesets; }

signals:
    void snapToGridChanged(bool snapToGrid);
    void showCoordinatesChanged(bool showGrid);
    void showWorldGridChanged(bool showGrid);
    void showCellGridChanged(bool showGrid);
    void gridColorChanged(const QColor &gridColor);

    void useOpenGLChanged(bool useOpenGL);
    void worldThumbnailsChanged(bool thumbs);

    void showObjectNamesChanged(bool show);
    void showBMPsChanged(bool show);

#define MINIMAP_WIDTH_MIN 128
#define MINIMAP_WIDTH_MAX 512
    void showMiniMapChanged(bool show);
    void miniMapWidthChanged(int width);

    void highlightCurrentLevelChanged(bool highlight);
    void mapsDirectoryChanged();
    void tilesDirectoryChanged();
    void showAdjacentMapsChanged(bool show);
    void heightMapDisplayStyleChanged(int style);
    void highlightRoomUnderPointerChanged(bool highlight);

public slots:
    void setSnapToGrid(bool snapToGrid);
    void setShowCoordinates(bool showCoords);
    void setShowWorldGrid(bool showGrid);
    void setShowCellGrid(bool showGrid);
    void setGridColor(const QColor &gridColor);
    void setShowMiniMap(bool show);
    void setMiniMapWidth(int width);
    void setShowObjectNames(bool show);
    void setShowBMPs(bool show);
    void setHighlightCurrentLevel(bool highlight);
    void setHighlightRoomUnderPointer(bool highlight);
    void setHeightMapDisplayStyle(int style);

private:
    Preferences();
    ~Preferences();

    QSettings *mSettings;

    bool mSnapToGrid;
    bool mShowCoordinates;
    bool mShowWorldGrid;
    bool mShowCellGrid;
    QColor mGridColor;
    bool mUseOpenGL;
    bool mWorldThumbnails;
    bool mShowObjectNames;
    bool mShowBMPs;
    bool mShowMiniMap;
    int mMiniMapWidth;
    bool mHighlightCurrentLevel;
    QString mConfigDirectory;
    QString mMapsDirectory;
    QString mTilesDirectory;
    QString mOpenFileDirectory;
    bool mShowAdjacentMaps;
    int mHeightMapDisplayStyle;
    bool mHighlightRoomUnderPointer;
    bool mUseVirtualTilesets;

    static Preferences *mInstance;
};

#endif // PREFERENCES_H
