#include "chunkedges.h"

#include "isochunk.h"
#include "isogridsquare.h"

#include <QStack>

using namespace Navigate;

ChunkEdges::ChunkEdges()
{
    if (IsoGridSquare::mTileDefFile.tilesets().isEmpty())
        IsoGridSquare::mTileDefFile.read(QLatin1String("D:/pz/zomboid/workdir/media/newtiledefinitions.tiles"));
}

ChunkEdges::~ChunkEdges()
{
    qDeleteAll(mEdges);
}

void ChunkEdges::fromChunk(IsoChunk *chunk)
{
    mChunk = chunk;
    wx = chunk->wx;
    wy = chunk->wy;

    calcEdges();
    calcAdjacency();
}

void ChunkEdges::calcEdges()
{
    mEdges.clear();

    int x, y, z = 0;
    Edge *edge = NULL;

    x = 0; // west edge
    for (y = 0; y < IsoChunk::WIDTH; y++) {
        IsoGridSquare *sq = mChunk->getGridSquare(x, y, z);
        if (sq == NULL)
            continue;
        if (!sq->isSolid() && !sq->isBlockedWest()) {
            if (edge == NULL || sq->isBlockedNorth()) {
                edge = new Edge();
                edge->chunk = this;
                mEdges += edge;
                edge->horizontal = false;
                edge->x1 = x;
                edge->x2 = x;
                edge->y1 = y;
            }
            edge->y2 = y;
        } else {
            edge = NULL;
        }
    }

    edge = NULL;
    x = IsoChunk::WIDTH - 1; // east edge
    for (y = 0; y < IsoChunk::WIDTH; y++) {
        IsoGridSquare *sq = mChunk->getGridSquare(x, y, z);
        if (sq == NULL)
            continue;
        if (!sq->isSolid()) {
            if (edge == NULL || sq->isBlockedNorth()) {
                edge = new Edge();
                edge->chunk = this;
                mEdges += edge;
                edge->horizontal = false;
                edge->x1 = x;
                edge->x2 = x;
                edge->y1 = y;
            }
            edge->y2 = y;
        } else {
            edge = NULL;
        }
    }

    edge = NULL;
    y = 0; // north edge
    for (x = 0; x < IsoChunk::WIDTH; x++) {
        IsoGridSquare *sq = mChunk->getGridSquare(x, y, z);
        if (sq == NULL)
            continue;
        if (!sq->isSolid() && !sq->isBlockedNorth()) {
            if (edge == NULL || sq->isBlockedWest()) {
                edge = new Edge();
                edge->chunk = this;
                mEdges += edge;
                edge->x1 = x;
                edge->y1 = y;
                edge->y2 = y;
            }
            edge->x2 = x;
        } else {
            edge = NULL;
        }
    }

    edge = NULL;
    y = IsoChunk::WIDTH - 1; // south edge
    for (x = 0; x < IsoChunk::WIDTH; x++) {
        IsoGridSquare *sq = mChunk->getGridSquare(x, y, z);
        if (sq == NULL)
            continue;
        if (!sq->isSolid()) {
            if (edge == NULL || sq->isBlockedWest()) {
                edge = new Edge();
                edge->chunk = this;
                mEdges += edge;
                edge->x1 = x;
                edge->y1 = y;
                edge->y2 = y;
            }
            edge->x2 = x;
        } else {
            edge = NULL;
        }
    }
}

class FloodFill
{
public:
    IsoChunk *mChunk;
    IsoGridSquare *start;
    static const int FILL_WIDTH = IsoChunk::WIDTH;
    bool visited[FILL_WIDTH][FILL_WIDTH];
    QStack<IsoGridSquare*> stack;
    QList<IsoGridSquare*> choices;

    FloodFill(IsoChunk *chunk) :
        mChunk(chunk)
    {

    }

    void calculate(IsoGridSquare *sq)
    {
        start = sq;
        for (int y = 0; y < FILL_WIDTH; y++)
            for (int x = 0; x < FILL_WIDTH; x++)
                visited[x][y] = false;
        stack.clear();
        choices.clear();

        if (sq == NULL)
            return;

        bool spanLeft = false;
        bool spanRight = false;

        if (!push(start->getX(), start->getY()))
            return;

        while ((sq = pop()) != NULL) {
            int x = sq->getX();
            int y1 = sq->getY();
            while (shouldVisit(x, y1, x, y1 - 1))
                y1--;
            spanLeft = spanRight = false;
            do {
                visited[gridX(x)][gridY(y1)] = true;
                IsoGridSquare *sq2 = mChunk->getGridSquareWorldPos(x, y1, start->getZ());
                if (sq2 != NULL)
                    choices += sq2;

                if (!spanLeft && shouldVisit(x, y1, x - 1, y1)) {
                    if (!push(x - 1, y1)) return;
                    spanLeft = true;
                } else if (spanLeft && !shouldVisit(x, y1, x - 1, y1)) {
                    spanLeft = false;
                } else if (spanLeft && !shouldVisit(x - 1, y1, x - 1, y1 - 1)) {
                    // North wall splits the vertical span.
                    if (!push(x - 1, y1)) return;
                }

                if (!spanRight && shouldVisit(x, y1, x + 1, y1)) {
                    if (!push(x + 1, y1)) return;
                    spanRight = true;
                } else if (spanRight && !shouldVisit(x, y1, x + 1, y1)) {
                    spanRight = false;
                } else if (spanRight && !shouldVisit(x + 1, y1, x + 1, y1 - 1)) {
                    // North wall splits the vertical span.
                    if (!push(x + 1, y1)) return;
                }
                y1++;
            } while (shouldVisit(x, y1 - 1, x, y1));
        }
    }

    bool shouldVisit(int x1, int y1, int x2, int y2)
    {
        if (!mChunk->containsWorldPos(x2, y2, start->getZ()))
            return false;
        if (visited[gridX(x2)][gridY(y2)])
            return false;
        IsoGridSquare *sq1 = mChunk->getGridSquareWorldPos(x1, y1, start->getZ());
        IsoGridSquare *sq2 = mChunk->getGridSquareWorldPos(x2, y2, start->getZ());
        if (sq2 == NULL) return false;
        if (sq2->isSolid()) return false;
        if (x1 == x2 && y1 < y2 && sq2->isBlockedNorth()) return false;
        if (x1 == x2 && y1 > y2 && sq1->isBlockedNorth()) return false;
        if (y1 == y2 && x1 < x2 && sq2->isBlockedWest()) return false;
        if (y1 == y2 && x1 > x2 && sq1->isBlockedWest()) return false;
        return true;
    }

    bool push(int x, int y)
    {
        IsoGridSquare *sq = mChunk->getGridSquareWorldPos(x, y, 0);
        Q_ASSERT(sq != NULL);
        stack.push(sq);
        return true;
    }

    IsoGridSquare *pop()
    {
        return stack.isEmpty() ? NULL : stack.pop();
    }

    int gridX(int x)
    {
        return x % 10;
    }

    int gridY(int y)
    {
        return y % 10;
    }
};

void ChunkEdges::calcAdjacency()
{
    FloodFill ff(mChunk);
    int z = 0;
    for (int i = 0; i < mEdges.size(); i++) {
        Edge *e = mEdges.at(i);
        IsoGridSquare *sq = mChunk->getGridSquare(e->x1, e->y1, z);
        ff.calculate(sq);
        for (int j = 0; j < mEdges.size(); j++) {
            if (j == i)
                continue;
            Edge *e2 = mEdges.at(j);
            if (e2->hasSquare(ff.choices))
                e->adjacent += e2;
        }
    }
}

/////

bool ChunkEdges::Edge::hasSquare(const QList<IsoGridSquare *> &choices)
{
    for (int i = 0; i < choices.size(); i++) {
        int sx = choices.at(i)->x % IsoChunk::WIDTH;
        int sy = choices.at(i)->y % IsoChunk::WIDTH;
        if (horizontal) {
            if (sy == y1 && sx >= x1 && sx <= x2)
                return true;
        } else {
            if (sx == x1 && sy >= y1 && sy <= y2)
                return true;
        }
    }
    return false;
}
