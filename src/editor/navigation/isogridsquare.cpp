#include "isogridsquare.h"

#include "isochunk.h"
#include "world.h"

#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>

using namespace Navigate;

QList<TileDefFile*> IsoGridSquare::mTileDefFiles;

IsoGridSquare::IsoGridSquare(int x, int y, int z, IsoChunk *chunk) :
    x(x),
    y(y),
    z(z),
    mChunk(chunk),
    mSolid(false),
    mBlockedWest(false),
    mBlockedNorth(false),
    mRoom(false),
    mWater(false)
{
    QVector<const Tiled::Cell *> cells;
    mChunk->orderedCellsAt(x, y, cells);

    QString HoppableW(QLatin1String("HoppableW"));
    QString HoppableN(QLatin1String("HoppableN"));
    QString collideW(QLatin1String("collideW"));
    QString collideN(QLatin1String("collideN"));
    QString doorFrW(QLatin1String("doorFrW")); // FIXME: unused?
    QString doorFrN(QLatin1String("doorFrN")); // FIXME: unused?
    QString DoorWallW(QLatin1String("DoorWallW"));
    QString DoorWallN(QLatin1String("DoorWallN"));
    QString solid(QLatin1String("solid"));
    QString solidtrans(QLatin1String("solidtrans"));
    QString tree(QLatin1String("tree"));
    QString WallW(QLatin1String("WallW"));
    QString WallN(QLatin1String("WallN"));
    QString wallNW(QLatin1String("WallNW"));
    QString WallWTrans(QLatin1String("WallWTrans"));
    QString WallNTrans(QLatin1String("WallNTrans"));
    QString WallNWTrans(QLatin1String("WallNWTrans"));
    QString water(QLatin1String("water"));
    QString windowW(QLatin1String("windowW"));
    QString windowN(QLatin1String("windowN"));

    foreach (const Tiled::Cell *cell, cells) {
        TileDefTileset *tdts = NULL;
        foreach (TileDefFile *tdefFile, mTileDefFiles) {
            tdts = tdefFile->tileset(cell->tile->tileset()->name());
            if (tdts != NULL)
                break;
        }
        if (tdts != NULL) {
            TileDefTile *tdt = tdts->tile(cell->tile->id() % tdts->mColumns, cell->tile->id() / tdts->mColumns);
            if (tdt == NULL)
                continue;
            foreach (QString key, tdt->mProperties.keys()) {
                if (key == WallW || key == wallNW || key == WallWTrans || key == WallNWTrans || key == doorFrW || key == DoorWallW || key == windowW)
                    mBlockedWest = true;
                if (key == WallN || key == wallNW || key == WallNTrans || key == WallNWTrans || key == doorFrN || key == DoorWallN || key == windowN)
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

bool IsoGridSquare::isSolid()
{
    return mSolid;
}

bool IsoGridSquare::isBlockedWest()
{
    return mBlockedWest;
}

bool IsoGridSquare::isBlockedNorth()
{
    return mBlockedNorth;
}

bool IsoGridSquare::isWater()
{
    return mWater;
}

bool IsoGridSquare::isRoom()
{
    return mRoom;
}

bool IsoGridSquare::loadTileDefFiles(const GenerateLotsSettings &settings, QString &error)
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
