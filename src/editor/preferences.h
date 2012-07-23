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
    bool showMiniMap() const;
    bool highlightCurrentLevel() const;

    QString mapsDirectory() const;
    void setMapsDirectory(const QString &path);

    QStringList searchPaths() const;
    void setSearchPaths(const QStringList &paths);

signals:
    void snapToGridChanged(bool snapToGrid);
    void showCoordinatesChanged(bool showGrid);
    void showWorldGridChanged(bool showGrid);
    void showCellGridChanged(bool showGrid);
    void showMiniMapChanged(bool show);
    void highlightCurrentLevelChanged(bool highlight);
    void mapsDirectoryChanged();

public slots:
    void setSnapToGrid(bool snapToGrid);
    void setShowCoordinates(bool showCoords);
    void setShowWorldGrid(bool showGrid);
    void setShowCellGrid(bool showGrid);
    void setShowMiniMap(bool show);
    void setHighlightCurrentLevel(bool highlight);

private:
    Preferences();
    ~Preferences();

    QSettings *mSettings;

    bool mSnapToGrid;
    bool mShowCoordinates;
    bool mShowWorldGrid;
    bool mShowCellGrid;
    bool mShowMiniMap;
    bool mHighlightCurrentLevel;
    QString mMapsDirectory;
    QStringList mSearchPaths;

    static Preferences *mInstance;
};

#endif // PREFERENCES_H
