/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#ifndef SCENEOVERLAY_H
#define SCENEOVERLAY_H

#include "tiledeffile.h"

#include <QGraphicsItem>

class BaseGraphicsScene;
class CellScene;
class MapBuildings;

class SceneOverlay : public QGraphicsItem
{
public:
    explicit SceneOverlay(BaseGraphicsScene *scene, QGraphicsItem *parent = 0);

protected:
    BaseGraphicsScene *mScene;
};

class LightSwitchOverlay : public SceneOverlay
{
public:
    LightSwitchOverlay(CellScene *scene, qreal x, qreal y, int z, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
    QPainterPath shape() const;

    CellScene *mCellScene;
    qreal mX;
    qreal mY;
    int mZ;
    QRegion mRoomRegion;
    QString mRoomName;
    static QImage mImage;
};

class CellSceneOverlays : public QObject
{
    Q_OBJECT
public:
    CellSceneOverlays(CellScene *scene);
    void updateCurrentLevelHighlight();

public slots:
    void update();

private:
    CellScene *mScene;
    QList<SceneOverlay*> mOverlays;
    MapBuildings *mMapBuildings;
    TileDefFile mTileDefFile;
    QList<TileDefTile*> mTileDefTiles;
};

/**
  * This class represents the lightbulbs.txt file.
  */
class LightbulbsFile : public QObject
{
public:
    LightbulbsFile();
    ~LightbulbsFile();

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();

    void addRoom(const QString &room)
    { mRooms += room; }

    void addMap(const QString &map)
    { mMaps += map; }

    void removeRoom(const QString &room)
    { mRooms.removeAll(room); }

    void removeMap(const QString &map)
    { mMaps.removeAll(map); }

    QStringList maps() const
    { return mMaps; }

    QStringList rooms() const
    { return mRooms; }

    QString errorString() const
    { return mError; }

private:
    QStringList mMaps;
    QStringList mRooms;
    QString mFileName;
    QString mError;
};

#include "singleton.h"
class LightbulbsMgr : public QObject, public Singleton<LightbulbsMgr>
{
    Q_OBJECT
public:
    LightbulbsMgr();

    void addRoom(const QString &room);
    void addMap(const QString &map);

    void toggleRoom(const QString &room);
    void toggleMap(const QString &map);

    bool ignoreRoom(const QString &room);
    bool ignoreMap(const QString &map);

    QStringList maps() const
    { return mFile.maps(); }

    QStringList rooms() const
    { return mFile.rooms(); }

signals:
    void changed();

private:
    LightbulbsFile mFile;
};

#endif // SCENEOVERLAY_H
