#ifndef ISOGRIDSQUARE_H
#define ISOGRIDSQUARE_H

#include "tiledeffile.h"

#include <QStringList>

namespace Navigate {

class IsoChunk;

class IsoGridSquare
{
public:
    IsoGridSquare(int x, int y, int z, IsoChunk *chunk);

    bool isSolid();
    bool isBlockedWest();
    bool isBlockedNorth();
    bool isWater();

    int getX() { return x; }
    int getY() { return y; }
    int getZ() { return z; }

    int x;
    int y;
    int z;

    IsoChunk *mChunk;
    QStringList mProperties;
    bool mSolid;
    bool mBlockedWest;
    bool mBlockedNorth;
    bool mWater;
    bool mRoom;

    static TileDefFile mTileDefFile;
};

} // namespace Navigate

#endif // ISOGRIDSQUARE_H
