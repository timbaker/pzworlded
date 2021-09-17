#ifndef TILESETSTXTFILE_H
#define TILESETSTXTFILE_H

#include <QObject>
#include <QSize>
#include <QString>

class TilesetsTxtFile : public QObject
{
    Q_OBJECT
public:
    static const int VERSION0 = 0;
    static const int VERSION_LATEST = VERSION0;

    TilesetsTxtFile();

    class MetaEnum
    {
    public:
        MetaEnum(const QString& name, int value)
            : mName(name)
            , mValue(value)
        {}
        QString mName;
        int mValue;
    };

    class Tile
    {
    public:
        int mX;
        int mY;
        QString mMetaEnum;
    };

    class Tileset
    {
    public:
        void setTile(const Tile& source);
        int findTile(int column, int row);

        QString mName;
        QString mFile;
        int mColumns;
        int mRows;
        QList<Tile> mTiles;
    };

    ~TilesetsTxtFile();

    bool read(const QString& path);
    bool write(const QString& path, int revision, int sourceRevision, const QList<Tileset*>& tilesets, const QList<MetaEnum>& metaEnums);

    const QString& errorString() const { return mError; }

    int mVersion;
    int mRevision;
    int mSourceRevision;
    QList<Tileset*> mTilesets;
    QList<MetaEnum> mEnums;

private:
    bool parse2Ints(const QString &s, int *pa, int *pb);

private:
    QString mError;
};

#endif // TILESETSTXTFILE_H
