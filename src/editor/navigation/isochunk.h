#ifndef ISOCHUNK_H
#define ISOCHUNK_H

#include "lotfilesmanager.h"
#include <QVector>

namespace Tiled {
class Cell;
}
class MapComposite;

namespace Navigate {

class IsoGridSquare;

class IsoChunk
{
public:
    static const int WIDTH = 10;

    IsoChunk(int wx, int wy, MapComposite *mapComposite, const QList<LotFile::RoomRect*> &roomRects);
    ~IsoChunk();

    IsoGridSquare *getGridSquare(int x, int y, int z);
    IsoGridSquare *getGridSquareWorldPos(int x, int y, int z);
    bool contains(int x, int y, int z);
    bool containsWorldPos(int x, int y, int z);
    int worldXMin() { return wx * WIDTH; }
    int worldYMin() { return wy * WIDTH; }
    int worldXMax() { return (wx + 1) * WIDTH; }
    int worldYMax() { return (wy + 1) * WIDTH; }

    void orderedCellsAt(int x, int y, QVector<const Tiled::Cell *> &cells);

    int wx;
    int wy;
    IsoGridSquare *squares[WIDTH][WIDTH];

    MapComposite *mMapComposite;
};

} // namespace Navgiate

#endif // ISOCHUNK_H
