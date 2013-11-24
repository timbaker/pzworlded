/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TILEDEFFILE_H
#define TILEDEFFILE_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QVector>

class TileDefTileset;

class TileDefTile
{
public:
    TileDefTile(TileDefTileset *tileset, int id) :
        mTileset(tileset),
#ifdef WORLDED
        mID(id)
#else
        mID(id),
        mPropertyUI()
#endif
    {
    }

    TileDefTileset *tileset() const { return mTileset; }
    int id() const { return mID; }

#ifndef WORLDED
    UIProperties::UIProperty *property(const QString &name)
    { return mPropertyUI.property(name); }

    bool getBoolean(const QString &name)
    {
        return mPropertyUI.getBoolean(name);
    }

    int getInteger(const QString &name)
    {
        return mPropertyUI.getInteger(name);
    }

    QString getString(const QString &name)
    {
        return mPropertyUI.getString(name);
    }

    QString getEnum(const QString &name)
    {
        return mPropertyUI.getEnum(name);
    }
#endif

    TileDefTileset *mTileset;
    int mID;
#ifndef WORLDED
    UIProperties mPropertyUI;
#endif

    // This is to preserve all the properties that were in the .tiles file
    // for this tile.  If TileProperties.txt changes so that these properties
    // can't be edited they will still persist in the .tiles file.
    // TODO: add a way to report/clean out obsolete properties.
    QMap<QString,QString> mProperties;
};

class TileDefTileset
{
public:
#ifndef WORLDED
    TileDefTileset(Tileset *ts);
#endif
    TileDefTileset();
    ~TileDefTileset();

    void resize(int columns, int rows);

    TileDefTile *tile(int col, int row)
    {
        int index = col + row * mColumns;
        return (index >= 0 && index < mTiles.size()) ? mTiles[index] : 0;
    }

    QString mName;
    QString mImageSource;
    int mColumns;
    int mRows;
    QVector<TileDefTile*> mTiles;
    int mID;
};

/**
  * This class represents a single binary *.tiles file.
  */
class TileDefFile : public QObject
{
    Q_OBJECT
public:
    TileDefFile();
    ~TileDefFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);

    QString directory() const;

    void insertTileset(int index, TileDefTileset *ts);
    TileDefTileset *removeTileset(int index);

    TileDefTileset *tileset(const QString &name);

    const QList<TileDefTileset*> &tilesets() const
    { return mTilesets; }

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }

    QString errorString() const
    { return mError; }

private:
    QList<TileDefTileset*> mTilesets;
    QMap<QString,TileDefTileset*> mTilesetByName;
    QString mFileName;
    QString mError;
};

#endif // TILEDEFFILE_H
