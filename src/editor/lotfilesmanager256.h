/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#ifndef LOTFILESMANAGER256_H
#define LOTFILESMANAGER256_H

#include "lotfilesmanager.h"

#define CELL_SIZE_256 256
#define CHUNKS_PER_CELL_256 32
#define CHUNK_SIZE_256 8

class CombinedCellMaps
{
public:
    CombinedCellMaps();
    ~CombinedCellMaps();

    bool load(WorldDocument *worldDoc, int cell256X, int cell256Y);
    MapInfo* getCombinedMap();

    int mCell256X;
    int mCell256Y;
    int mMinCell300X;
    int mMinCell300Y;
    int mCellsWidth;
    int mCellsHeight;
    QList<WorldCell*> mCells;
    DelayedMapLoader mLoader;
    MapComposite* mMapComposite = nullptr;
    QString mError;
};

class LotFilesManager256 : public QObject
{
    Q_OBJECT
public:

    static LotFilesManager256 *instance();
    static void deleteInstance();

    enum GenerateMode {
        GenerateAll,
        GenerateSelected
    };

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);
    bool generateCell(WorldCell* cell);
    bool generateCell(int cell256X, int cell256Y);
    bool generateHeader(CombinedCellMaps &combinedMaps, MapComposite *mapComposite);
    bool generateHeaderAux(int cell256X, int cell256Y);
    bool generateChunk(QDataStream &out, int chunkX, int chunkY);
    void generateBuildingObjects(int mapWidth, int mapHeight);
    void generateBuildingObjects(int mapWidth, int mapHeight,
                                 LotFile::Room *room, LotFile::RoomRect *rr);
    void generateJumboTrees(CombinedCellMaps &combinedMaps);

    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);

    int getRoomID(int x, int y, int z);

    QString errorString() const { return mError; }

signals:

private:
    uint cellToGid(const Tiled::Cell *cell);
    bool processObjectGroups(CombinedCellMaps &combinedMaps, WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(CombinedCellMaps &combinedMaps,WorldCell *cell, Tiled::ObjectGroup *objectGroup, int levelOffset, const QPoint &offset);
    void resolveProperties(PropertyHolder *ph, PropertyList &result);
    qint8 calculateZombieDensity(int x, int y);

private:
    Q_DISABLE_COPY(LotFilesManager256)

    explicit LotFilesManager256(QObject *parent = nullptr);
    ~LotFilesManager256();

    static LotFilesManager256 *mInstance;

    WorldDocument *mWorldDoc;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<QString, uint> mTilesetNameToFirstGid;
    Tiled::Tileset *mJumboTreeTileset;
    QMap<int,LotFile::Tile*> TileMap;
    QVector<QVector<QVector<LotFile::Square> > > mGridData;
    int mMinLevel;
    int mMaxLevel;
    QList<LotFile::RoomRect*> mRoomRects;
    QMap<int,QList<LotFile::RoomRect*> > mRoomRectByLevel;
    LotFile::RectLookup<LotFile::RoomRect> mRoomRectLookup;
    LotFile::RectLookup<LotFile::Room> mRoomLookup;
    QList<LotFile::Room*> roomList;
    QList<LotFile::Building*> buildingList;
    QImage ZombieSpawnMap;
    QList<const JumboZone*> mJumboZoneList;
    QSet<QPair<int, int>> mDoneCells256;
    LotFile::Stats mStats;

    QString mError;
};


#endif // LOTFILESMANAGER256_H
