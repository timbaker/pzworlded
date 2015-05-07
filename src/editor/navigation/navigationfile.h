#ifndef NAVIGATIONFILE_H
#define NAVIGATIONFILE_H

#include <QList>

class GenerateLotsSettings;
class MapComposite;

namespace LotFile {
class RoomRect;
}

namespace Navigate {

class NavigationFile
{
public:
    NavigationFile();

    void fromMap(int cellX, int cellY, MapComposite *mapComposite, const QList<LotFile::RoomRect*> &roomRects, const GenerateLotsSettings &settings);
};

} // namespace Navigate

#endif // NAVIGATIONFILE_H
