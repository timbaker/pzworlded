#include "isochunk.h"

#include "mapcomposite.h"

#include "isogridsquare.h"

using namespace Navigate;

IsoChunk::IsoChunk(int wx, int wy, MapComposite *mapComposite, const QList<LotFile::RoomRect *> &roomRects) :
    wx(wx),
    wy(wy),
    mMapComposite(mapComposite)
{
    int z = 0;
    for (int y = 0; y < WIDTH; y++) {
        for (int x = 0; x < WIDTH; x++) {
            squares[x][y] = new IsoGridSquare(wx * WIDTH + x, wy * WIDTH + y, z, this);
        }
    }

    QRect bounds(worldXMin(), worldYMin(), WIDTH, WIDTH);
    for (LotFile::RoomRect *rect : roomRects) {
        if (rect->bounds().intersects(bounds)) {
            for (int y = rect->y; y < rect->y + rect->h; y++) {
                for (int x = rect->x; x < rect->x + rect->w; x++) {
                    if (containsWorldPos(x, y, 0)) {
                        squares[x - worldXMin()][y - worldYMin()]->mRoom = true;
                    }
                }
            }
        }
    }
}

IsoChunk::~IsoChunk()
{
    for (int y = 0; y < WIDTH; y++) {
        for (int x = 0; x < WIDTH; x++) {
            delete squares[x][y];
        }
    }
}

IsoGridSquare *IsoChunk::getGridSquare(int x, int y, int z)
{
    if (contains(x, y, z)) {
        return squares[x][y];
    }
    return NULL;
}

IsoGridSquare *IsoChunk::getGridSquareWorldPos(int x, int y, int z)
{
    if (containsWorldPos(x, y, z)) {
        return squares[x - worldXMin()][y - worldYMin()];
    }
    return NULL;
}

bool IsoChunk::contains(int x, int y, int z)
{
    return x >= 0 && x < WIDTH && y >= 0 && y < WIDTH;
}

bool IsoChunk::containsWorldPos(int x, int y, int z)
{
    return x >= worldXMin() && x < worldXMax() && y >= worldYMin() && y < worldYMax();
}

void IsoChunk::orderedCellsAt(int x, int y, QVector<const Tiled::Cell *> &cells)
{
    mMapComposite->layerGroupForLevel(0)->orderedCellsAt2(QPoint(x, y), cells);
}
