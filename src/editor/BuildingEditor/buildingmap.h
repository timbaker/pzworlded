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

#ifndef BUILDINGMAP_H
#define BUILDINGMAP_H

#include <QObject>

#include <QMap>
#include <QRegion>
#include <QSet>
#include <QStringList>

class CompositeLayerGroup;
class MapComposite;

namespace Tiled {
class Map;
class MapRenderer;
class TileLayer;
class Tileset;
}

namespace BuildingEditor {

class Building;
class BuildingFloor;
class BuildingObject;
class Room;

class ShadowBuilding;

class BuildingModifier
{
public:
    BuildingModifier(ShadowBuilding *shadowBuilding);
    virtual ~BuildingModifier();

protected:
    ShadowBuilding *mShadowBuilding;
};

class ShadowBuilding
{
public:
    ShadowBuilding(const Building *building);
    ~ShadowBuilding();

    BuildingFloor *floor(int level) const;

    BuildingObject *shadowObject(BuildingObject *object)
    { return mOriginalToShadowObject[object]; }

    void buildingRotated();
    void buildingResized();

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void floorTilesChanged(BuildingFloor *floor);
    void floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                           const QRect &bounds);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);

    void roomAdded(Room *room);
    void roomRemoved(Room *room);

    /////
    bool setCursorObject(BuildingFloor *floor, BuildingObject *object);
    void dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset);
    void resetDrag(BuildingObject *object);

    void changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room *> > &grid);
    void resetFloorGrid(BuildingFloor *floor);
    /////

    BuildingFloor *cloneFloor(BuildingFloor *floor);
    BuildingObject *cloneObject(BuildingFloor *shadowFloor, BuildingObject *object);
    void recreateObject(BuildingFloor *originalFloor, BuildingObject *object);

    void addModifier(BuildingModifier *modifier);
    void removeModifier(BuildingModifier *modifier);

private:    const Building *mBuilding;
    Building *mShadowBuilding;
    QList<BuildingModifier*> mModifiers;
    BuildingModifier *mCursorObjectModifier;
    QMap<BuildingObject*,BuildingObject*> mOriginalToShadowObject;
};

class BuildingMap : public QObject
{
    Q_OBJECT
public:
    BuildingMap(Building *building);

    ~BuildingMap();

    MapComposite *mapComposite() const
    { return mMapComposite; }

    Tiled::Map *map() const
    { return mMap; }

    Tiled::MapRenderer *mapRenderer() const
    { return mMapRenderer; }

    QString buildingTileAt(int x, int y, int level, const QString &layerName);

    static QStringList layerNames(int level);
    static QStringList requiredLayerNames();

    /////
    void setCursorObject(BuildingFloor *floor, BuildingObject *object);
    void dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset);
    void resetDrag(BuildingFloor *floor, BuildingObject *object);

    void changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room*> > &grid);
    void resetFloorGrid(BuildingFloor *floor);

    void suppressTiles(BuildingFloor *floor, const QRegion &rgn);
    /////

    Tiled::Map *mergedMap() const;

    static void loadNeededTilesets(Building *building);

    void addRoomDefObjects(Tiled::Map *map);
    void addRoomDefObjects(Tiled::Map *map, BuildingFloor *floor);

    static int defaultOrientation();

    bool isTilesetUsed(Tiled::Tileset *tileset);

public:
    void buildingRotated();
    void buildingResized();

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void floorTilesChanged(BuildingFloor *floor);
    void floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                           const QRect &bounds);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);

    void roomAdded(Room *room);
    void roomRemoved(Room *room);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void recreateAllLater();

signals:
    void aboutToRecreateLayers();
    void layersRecreated();
    void mapResized();
    void layersUpdated(int level, const QRegion &rgn);

private slots:
    void handlePending();

private:
    void BuildingToMap();
    void BuildingSquaresToTileLayers(BuildingFloor *floor, const QRect &area,
                                     CompositeLayerGroup *layerGroup);

    void userTilesToLayer(BuildingFloor *floor, const QString &layerName,
                          const QRect &bounds);

private:
    Building *mBuilding;
    MapComposite *mMapComposite;
    Tiled::Map *mMap; // tile layers contain only user-drawn tiles
    MapComposite *mBlendMapComposite;
    Tiled::Map *mBlendMap; // tile layers contain only building-generated tiles
    Tiled::MapRenderer *mMapRenderer;
    QMap<QString,int> mLayerToSection;

    BuildingFloor *mCursorObjectFloor;
    ShadowBuilding *mShadowBuilding;
    QMap<BuildingFloor*,QRegion> mSuppressTiles;

    bool pending;
    bool pendingRecreateAll;
    bool pendingBuildingResized;
    QSet<BuildingFloor*> pendingLayoutToSquares; // LayoutToSquares
    QMap<BuildingFloor*,QRegion> pendingSquaresToTileLayers; // BuildingSquaresToTileLayers
    QSet<BuildingFloor*> pendingEraseUserTiles; // TileLayer::erase on all user-tile layers
    QMap<BuildingFloor*,QMap<QString,QRegion> > pendingUserTilesToLayer; // floorTilesToLayer
};

} // namespace BuildingEditor

#endif // BUILDINGMAP_H
