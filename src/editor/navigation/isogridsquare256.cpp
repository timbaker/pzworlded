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

#include "isogridsquare256.h"

#include "isochunk256.h"
#include "tiledeffile.h"
#include "world.h"

#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>

using namespace Navigate;

QList<TileDefFile*> IsoGridSquare256::mTileDefFiles;

IsoGridSquare256::IsoGridSquare256(int x, int y, int z, IsoChunk256 *chunk) :
    x(x),
    y(y),
    z(z),
    mChunk(chunk),
    mSolid(false),
    mBlockedWest(false),
    mBlockedNorth(false),
    mWater(false),
    mRoom(false)
{
    QVector<const Tiled::Cell *> cells;
    mChunk->orderedCellsAt(x, y, cells);

    static const QString HoppableW(QLatin1String("HoppableW"));
    static const QString HoppableN(QLatin1String("HoppableN"));
    static const QString collideW(QLatin1String("collideW"));
    static const QString collideN(QLatin1String("collideN"));
    static const QString doorFrW(QLatin1String("doorFrW")); // FIXME: unused?
    static const QString doorFrN(QLatin1String("doorFrN")); // FIXME: unused?
    static const QString DoorWallW(QLatin1String("DoorWallW"));
    static const QString DoorWallN(QLatin1String("DoorWallN"));
    static const QString solid(QLatin1String("solid"));
    static const QString solidtrans(QLatin1String("solidtrans"));
    static const QString tree(QLatin1String("tree"));
    static const QString WallW(QLatin1String("WallW"));
    static const QString WallN(QLatin1String("WallN"));
    static const QString wallNW(QLatin1String("WallNW"));
    static const QString WallWTrans(QLatin1String("WallWTrans"));
    static const QString WallNTrans(QLatin1String("WallNTrans"));
    static const QString WallNWTrans(QLatin1String("WallNWTrans"));
    static const QString water(QLatin1String("water"));
    static const QString windowW(QLatin1String("windowW"));
    static const QString windowN(QLatin1String("windowN"));
    static const QString WindowW(QLatin1String("WindowW"));
    static const QString WindowN(QLatin1String("WindowN"));

    foreach (const Tiled::Cell *cell, cells) {
        TileDefTileset *tdts = nullptr;
        for (TileDefFile *tdefFile : mTileDefFiles) {
            tdts = tdefFile->tileset(cell->tile->tileset()->name());
            if (tdts != nullptr)
                break;
        }
        if (tdts != nullptr) {
            TileDefTile *tdt = tdts->tile(cell->tile->id() % tdts->mColumns, cell->tile->id() / tdts->mColumns);
            if (tdt == nullptr)
                continue;
            foreach (QString key, tdt->mProperties.keys()) {
                if (key == WallW || key == wallNW || key == WallWTrans || key == WallNWTrans || key == doorFrW || key == DoorWallW || key == windowW || key == WindowW)
                    mBlockedWest = true;
                if (key == WallN || key == wallNW || key == WallNTrans || key == WallNWTrans || key == doorFrN || key == DoorWallN || key == windowN || key == WindowN)
                    mBlockedNorth = true;
                if (key == solid || key == solidtrans)
                    mSolid = true;
                // FIXME: stairs are mSolid
            }
            if (tdt->mProperties.contains(water)) {
                mSolid = false;
                mWater = true;
            }
            if (tdt->mProperties.contains(tree))
                mSolid = false;
            if (tdt->mProperties.contains(HoppableW))
                mBlockedWest = false;
            if (tdt->mProperties.contains(HoppableN))
                mBlockedNorth = false;
        }
    }
}

bool IsoGridSquare256::isSolid()
{
    return mSolid;
}

bool IsoGridSquare256::isBlockedWest()
{
    return mBlockedWest;
}

bool IsoGridSquare256::isBlockedNorth()
{
    return mBlockedNorth;
}

bool IsoGridSquare256::isWater()
{
    return mWater;
}

bool IsoGridSquare256::isRoom()
{
    return mRoom;
}

bool IsoGridSquare256::loadTileDefFiles(const GenerateLotsSettings &settings, QString &error)
{
    qDeleteAll(mTileDefFiles);
    mTileDefFiles.clear();

    QDir dir(settings.tileDefFolder);
    QStringList filters(QLatin1String("*.tiles"));
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
    foreach (QString fileName, files) {
        if (fileName.endsWith(QLatin1String("_4.tiles")))
            continue;
        TileDefFile *tdefFile = new TileDefFile();
        if (!tdefFile->read(dir.filePath(fileName))) {
            error = tdefFile->errorString();
            return false;
        }
        qDebug() << "read " << fileName;
        mTileDefFiles += tdefFile;
    }
    return true;
}
