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

class InterruptibleThread;

class CombinedCellMaps
{
public:
    CombinedCellMaps();
    ~CombinedCellMaps();

    bool startLoading(WorldDocument *worldDoc, int cell256X, int cell256Y);
    int checkLoading(WorldDocument *worldDoc);
    MapInfo* getCombinedMap();
    void moveToThread(MapComposite *mapComposite, QThread *thread);

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

class LotFilesManager256;

class LotFilesWorker256 : public QObject
{
    Q_OBJECT
public:
    enum class Status
    {
        Idle,
        LoadingMaps,
        Working,
        Error,
        Finished
    };

    LotFilesWorker256(LotFilesManager256 *manager, InterruptibleThread *thread);
    bool generateHeader(CombinedCellMaps &combinedMaps, MapComposite *mapComposite);
    bool generateHeaderAux(int cell256X, int cell256Y);
    bool generateChunk(QDataStream &out, int chunkX, int chunkY);
    void generateBuildingObjects(int mapWidth, int mapHeight);
    void generateBuildingObjects(int mapWidth, int mapHeight, LotFile::Room *room, LotFile::RoomRect *rr);
    void generateJumboTrees(CombinedCellMaps &combinedMaps);
    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);
    int getRoomID(int x, int y, int z);
    uint cellToGid(const Tiled::Cell *cell);
    bool processObjectGroups(CombinedCellMaps &combinedMaps, WorldCell *cell, MapComposite *mapComposite);
    bool processObjectGroup(CombinedCellMaps &combinedMaps,WorldCell *cell, Tiled::ObjectGroup *objectGroup, int levelOffset, const QPoint &offset);
    void resolveProperties(PropertyHolder *ph, PropertyList &result);
    qint8 calculateZombieDensity(int x, int y);

//    const QString tr(const char *str) const;

public slots:
    bool generateCell(CombinedCellMaps *combinedMaps);

private:
    Q_DISABLE_COPY(LotFilesWorker256)

    LotFilesManager256 *mManager;
    Status mStatus = Status::Idle;
    WorldDocument *mWorldDoc;
    CombinedCellMaps *mCombinedCellMaps;
    WorldCell *mCell;
    Tiled::Tileset *mJumboTreeTileset;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<QString, uint> mTilesetNameToFirstGid;
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
    LotFile::Stats mStats;
    QString mError;

    friend class LotFilesManager256;
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

    struct GenerateCellFailure
    {
        WorldCell* cell;
        QString error;

        GenerateCellFailure(WorldCell* cell, const QString& error)
            : cell(cell)
            , error(error)
        {
        }
    };

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);
    bool generateCell(WorldCell* cell);
    bool generateCell(WorldCell *cell, int cell256X, int cell256Y);

    QString errorString() const { return mError; }

signals:

private:
    Q_DISABLE_COPY(LotFilesManager256)

    explicit LotFilesManager256(QObject *parent = nullptr);
    ~LotFilesManager256();
    void updateWorkers();
    LotFilesWorker256 *getFirstWorkerWithStatus(LotFilesWorker256::Status status);
    LotFilesWorker256 *getIdleWorker();
    LotFilesWorker256 *getBusyWorker();

    static LotFilesManager256 *mInstance;

    WorldDocument *mWorldDoc;
    QImage ZombieSpawnMap;
    QList<const JumboZone*> mJumboZoneList;
    QSet<QPair<int, int>> mDoneCells256;
    QVector<InterruptibleThread*> mWorkerThreads;
    QVector<LotFilesWorker256*> mWorkers;
    LotFile::Stats mStats;
    QVector<GenerateCellFailure> mFailures;
    QString mError;

    friend class LotFilesWorker256;
};

#include <QMetaType>
Q_DECLARE_METATYPE(CombinedCellMaps*)

#endif // LOTFILESMANAGER256_H
