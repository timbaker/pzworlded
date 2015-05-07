#include "navigationfile.h"

#include "world.h"
#include "lotfilesmanager.h"

#include "chunkedges.h"
#include "isochunk.h"

#include <QDataStream>
#include <QFile>

using namespace Navigate;

NavigationFile::NavigationFile()
{
}

void NavigationFile::fromMap(int cellX, int cellY, MapComposite *mapComposite, const QList<LotFile::RoomRect *> &roomRects, const GenerateLotsSettings &settings)
{
    ChunkEdges *chunkEdges[30][30];

    for (int yy = 0; yy < 30; yy++) {
        for (int xx = 0; xx < 30; xx++) {
            IsoChunk *chunk = new IsoChunk(xx, yy, mapComposite, roomRects);
            chunkEdges[xx][yy] = new ChunkEdges();
            chunkEdges[xx][yy]->fromChunk(chunk);
//            qDebug() << "chunk " << xx << "," << yy << " #edges=" << chunkEdges.mEdges.size();
        }
    }

    QString lotsDirectory = settings.exportDir;
    QFile file(lotsDirectory + QLatin1Char('/') + QString::fromLatin1("navigate_%1_%2.bin")
               .arg(settings.worldOrigin.x() + cellX).arg(settings.worldOrigin.y() + cellY));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    for (int yy = 0; yy < 30; yy++) {
        for (int xx = 0; xx < 30; xx++) {
            ChunkEdges *ce = chunkEdges[xx][yy];
            out << quint8(ce->mEdges.size());
            foreach (ChunkEdges::Edge *edge, ce->mEdges) {
                out << quint8(edge->x1);
                out << quint8(edge->y1);
                out << qint8(edge->horizontal ? (edge->x2 - edge->x1 + 1) : -(edge->y2 - edge->y1 + 1)); // > 0 horizontal, < 0 vertical
                out << quint8(edge->adjacent.size());
                for (int i = 0; i < edge->adjacent.size(); i++) {
                    out << quint8(ce->mEdges.indexOf(edge->adjacent.at(i)));
                }
            }
        }
    }

    file.close();

    for (int yy = 0; yy < 30; yy++) {
        for (int xx = 0; xx < 30; xx++) {
            delete chunkEdges[xx][yy]->mChunk;
            delete chunkEdges[xx][yy];
        }
    }
}
