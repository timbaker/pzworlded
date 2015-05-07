#ifndef CHUNKEDGES_H
#define CHUNKEDGES_H

#include <QList>

namespace Navigate {

class IsoChunk;
class IsoGridSquare;

class ChunkEdges
{
public:
    class Edge
    {
    public:
        ChunkEdges *chunk;
        bool horizontal;
        int x1;
        int y1;
        int x2;
        int y2;
        QList<Edge*> adjacent;

        Edge() :
            chunk(NULL),
            horizontal(true)
        {

        }

        bool hasSquare(const QList<IsoGridSquare*> &choices);
    };

    ChunkEdges();
    ~ChunkEdges();

    void fromChunk(IsoChunk *chunk);
    void calcEdges();
    void calcAdjacency();

    IsoChunk *mChunk;
    int wx;
    int wy;
    QList<Edge*> mEdges;
};

} // namespace Navigate

#endif // CHUNKEDGES_H
