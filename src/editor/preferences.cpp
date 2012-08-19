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

#include "preferences.h"

#include <QSettings>

Preferences *Preferences::mInstance = 0;

Preferences *Preferences::instance()
{
    if (!mInstance)
        mInstance = new Preferences;
    return mInstance;
}

void Preferences::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

bool Preferences::snapToGrid() const
{
    return mSnapToGrid;
}

bool Preferences::showCoordinates() const
{
    return mShowCoordinates;
}

bool Preferences::showWorldGrid() const
{
    return mShowWorldGrid;
}

bool Preferences::showCellGrid() const
{
    return mShowCellGrid;
}

bool Preferences::showMiniMap() const
{
    return mShowMiniMap;
}

int Preferences::miniMapWidth() const
{
    return mMiniMapWidth;
}

bool Preferences::highlightCurrentLevel() const
{
    return mHighlightCurrentLevel;
}

Preferences::Preferences()
    : QObject()
    , mSettings(new QSettings)
{
    // Retrieve interface settings
    mSettings->beginGroup(QLatin1String("Interface"));
    mSnapToGrid = mSettings->value(QLatin1String("SnapToGrid"), true).toBool();
    mShowCoordinates = mSettings->value(QLatin1String("ShowCoordinates"), true).toBool();
    mShowWorldGrid = mSettings->value(QLatin1String("ShowWorldGrid"), true).toBool();
    mShowCellGrid = mSettings->value(QLatin1String("ShowCellGrid"), false).toBool();
    mGridColor = QColor(mSettings->value(QLatin1String("GridColor"),
                                         QColor(Qt::black).name()).toString());
    mShowObjectNames = mSettings->value(QLatin1String("ShowObjectNames"), true).toBool();
    mShowMiniMap = mSettings->value(QLatin1String("ShowMiniMap"), true).toBool();
    mMiniMapWidth = mSettings->value(QLatin1String("MiniMapWidth"), 256).toInt();
    mHighlightCurrentLevel = mSettings->value(QLatin1String("HighlightCurrentLevel"),
                                              false).toBool();
    mUseOpenGL = mSettings->value(QLatin1String("OpenGL"), false).toBool();
    mSettings->endGroup();

    mSettings->beginGroup(QLatin1String("MapsDirectory"));
    mMapsDirectory = mSettings->value(QLatin1String("Current"), QString()).toString();
    mSettings->endGroup();

    mSearchPaths = mSettings->value(QLatin1String("SearchPaths"), QStringList()).toStringList();
}

Preferences::~Preferences()
{
    delete mSettings;
}

QString Preferences::mapsDirectory() const
{
    return mMapsDirectory;
}

void Preferences::setSnapToGrid(bool snapToGrid)
{
    if (snapToGrid == mSnapToGrid)
        return;

    mSnapToGrid = snapToGrid;
    mSettings->setValue(QLatin1String("Interface/SnapToGrid"), mSnapToGrid);
    emit snapToGridChanged(mSnapToGrid);
}

void Preferences::setShowCoordinates(bool showCoords)
{
    if (showCoords == mShowCoordinates)
        return;

    mShowCoordinates = showCoords;
    mSettings->setValue(QLatin1String("Interface/ShowCoordinates"), mShowCoordinates);
    emit showCoordinatesChanged(mShowCoordinates);
}

void Preferences::setShowWorldGrid(bool showGrid)
{
    if (showGrid == mShowWorldGrid)
        return;

    mShowWorldGrid = showGrid;
    mSettings->setValue(QLatin1String("Interface/ShowWorldGrid"), mShowWorldGrid);
    emit showWorldGridChanged(mShowWorldGrid);
}

void Preferences::setShowCellGrid(bool showGrid)
{
    if (showGrid == mShowCellGrid)
        return;

    mShowCellGrid = showGrid;
    mSettings->setValue(QLatin1String("Interface/ShowCellGrid"), mShowCellGrid);
    emit showCellGridChanged(mShowCellGrid);
}

void Preferences::setGridColor(const QColor &gridColor)
{
    if (mGridColor == gridColor)
        return;

    mGridColor = gridColor;
    mSettings->setValue(QLatin1String("Interface/GridColor"), mGridColor.name());
    emit gridColorChanged(mGridColor);
}

void Preferences::setUseOpenGL(bool useOpenGL)
{
    if (mUseOpenGL == useOpenGL)
        return;

    mUseOpenGL = useOpenGL;
    mSettings->setValue(QLatin1String("Interface/OpenGL"), mUseOpenGL);

    emit useOpenGLChanged(mUseOpenGL);
}

void Preferences::setShowObjectNames(bool show)
{
    if (mShowObjectNames == show)
        return;

    mShowObjectNames = show;
    mSettings->setValue(QLatin1String("Interface/ShowObjectNames"), mShowObjectNames);

    emit showObjectNamesChanged(mShowObjectNames);
}

void Preferences::setShowMiniMap(bool show)
{
    if (show == mShowMiniMap)
        return;

    mShowMiniMap = show;
    mSettings->setValue(QLatin1String("Interface/ShowMiniMap"), mShowMiniMap);
    emit showMiniMapChanged(mShowMiniMap);
}

void Preferences::setMiniMapWidth(int width)
{
    width = qMin(width, MINIMAP_WIDTH_MAX);
    width = qMax(width, MINIMAP_WIDTH_MIN);

    if (mMiniMapWidth == width)
        return;
    mMiniMapWidth = width;
    mSettings->setValue(QLatin1String("Interface/MiniMapWidth"), width);
    emit miniMapWidthChanged(mMiniMapWidth);
}

void Preferences::setHighlightCurrentLevel(bool highlight)
{
    if (highlight == mHighlightCurrentLevel)
        return;

    mHighlightCurrentLevel = highlight;
    mSettings->setValue(QLatin1String("Interface/HighlightCurrentLevel"), mHighlightCurrentLevel);
    emit highlightCurrentLevelChanged(mHighlightCurrentLevel);
}

void Preferences::setMapsDirectory(const QString &path)
{
    if (mMapsDirectory == path)
        return;
    mMapsDirectory = path;
    mSettings->setValue(QLatin1String("MapsDirectory/Current"), path);

    // Put this up, otherwise the progress dialog shows and hides for each lot.
    // Since each open document has its own ZLotManager, this shows and hides for each document as well.
//    ZProgressManager::instance()->begin(QLatin1String("Checking lots..."));

    emit mapsDirectoryChanged();
}

QStringList Preferences::searchPaths() const
{
    return mSearchPaths;
}

void Preferences::setSearchPaths(const QStringList &paths)
{
    if (mSearchPaths == paths)
        return;
    mSearchPaths = paths;
    mSettings->setValue(QLatin1String("SearchPaths"), paths);

//    emit mapsDirectoryChanged();
}
