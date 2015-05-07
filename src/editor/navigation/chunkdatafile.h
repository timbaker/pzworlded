#ifndef CHUNKDATAFILE_H
#define CHUNKDATAFILE_H

#include <QList>

class GenerateLotsSettings;
class MapComposite;

namespace LotFile {
class RoomRect;
}

namespace Navigate {

class ChunkDataFile
{
public:
    ChunkDataFile();
    void fromMap(int cellX, int cellY, MapComposite *mapComposite, const QList<LotFile::RoomRect *> &roomRects, const GenerateLotsSettings &settings);
};

} // namespace Navigate

#endif // CHUNKDATAFILE_H
