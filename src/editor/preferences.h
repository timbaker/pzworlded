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

    bool snapToGrid() const;
    bool showCoordinates() const;
    bool showWorldGrid() const;
    bool showCellGrid() const;
    QColor gridColor() const { return mGridColor; }
    bool showMiniMap() const;
    int miniMapWidth() const;
    bool highlightCurrentLevel() const;

    QString mapsDirectory() const;
    void setMapsDirectory(const QString &path);

    QString lotsDirectory() const;
    void setLotsDirectory(const QString &path);

    QStringList searchPaths() const;
    void setSearchPaths(const QStringList &paths);

    bool useOpenGL() const { return mUseOpenGL; }
    void setUseOpenGL(bool useOpenGL);

    bool showObjectNames() const { return mShowObjectNames; }

signals:
    void snapToGridChanged(bool snapToGrid);
    void showCoordinatesChanged(bool showGrid);
    void showWorldGridChanged(bool showGrid);
    void showCellGridChanged(bool showGrid);
    void gridColorChanged(const QColor &gridColor);

    void useOpenGLChanged(bool useOpenGL);

    void showObjectNamesChanged(bool show);

#define MINIMAP_WIDTH_MIN 128
#define MINIMAP_WIDTH_MAX 512
    void showMiniMapChanged(bool show);
    void miniMapWidthChanged(int width);

    void highlightCurrentLevelChanged(bool highlight);
    void mapsDirectoryChanged();
    void lotsDirectoryChanged();

public slots:
    void setSnapToGrid(bool snapToGrid);
    void setShowCoordinates(bool showCoords);
    void setShowWorldGrid(bool showGrid);
    void setShowCellGrid(bool showGrid);
    void setGridColor(const QColor &gridColor);
    void setShowMiniMap(bool show);
    void setMiniMapWidth(int width);
    void setShowObjectNames(bool show);
    void setHighlightCurrentLevel(bool highlight);

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
    bool mShowObjectNames;
    bool mShowMiniMap;
    int mMiniMapWidth;
    bool mHighlightCurrentLevel;
    QString mMapsDirectory;
    QString mLotsDirectory;
    QStringList mSearchPaths;

    static Preferences *mInstance;
};

#endif // PREFERENCES_H
