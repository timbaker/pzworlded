/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#ifndef TILEMETAINFOMGR_H
#define TILEMETAINFOMGR_H

#include <QMap>
#include <QObject>
#include <QStringList>

namespace Tiled {

class Tile;
class Tileset;

class TileMetaInfo
{
public:
    QString mMetaGameEnum;
};

class TilesetMetaInfo
{
public:
    QString mTilesetName;
    QMap<QString,TileMetaInfo> mInfo; // index is "column,row"

    static QString key(Tile *tile);
};

class TileMetaInfoMgr : public QObject
{
    Q_OBJECT

public:
    static TileMetaInfoMgr *instance();
    static void deleteInstance();

    void changeTilesDirectory(const QString &path, const QString &path2x);

    QString tilesDirectory() const;
    QString tiles2xDirectory() const;

    QList<Tileset*> tilesets() const
    { return mTilesetByName.values(); }

    Tileset *tileset(int n) const
    { return tilesets().at(n); }

    Tileset *tileset(const QString &tilesetName)
    {
        if (mTilesetByName.contains(tilesetName))
            return mTilesetByName[tilesetName];
        return 0;
    }

    int indexOf(Tileset *ts)
    { return tilesets().indexOf(ts); }

    int indexOf(const QString &tilesetName)
    {
        if (mTilesetByName.contains(tilesetName))
            return tilesets().indexOf(mTilesetByName[tilesetName]);
        return -1;
    }

    QStringList tilesetPaths() const;

    QStringList enumNames() const
    { return mEnumNames; }

    const QMap<QString,int> enums() const
    { return mEnums; }

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();
    bool upgradeTxt();
    bool mergeTxt();

    bool hasReadTxt()
    { return mHasReadTxt; }

    QString errorString() const
    { return mError; }

    Tileset *loadTileset(const QString &source);
    bool loadTilesetImage(Tileset *ts, const QString &source);
    void addTileset(Tileset *ts);
    void removeTileset(Tileset *ts);

    void loadTilesets(const QList<Tileset*> &tilesets = QList<Tileset*>()); // this shouldn't be public

    void setTileEnum(Tile *tile, const QString &enumName);
    QString tileEnum(Tile *tile);
    int tileEnumValue(Tile *tile);
    bool isEnumWest(int enumValue) const;
    bool isEnumNorth(int enumValue) const;
    bool isEnumWest(const QString &enumName) const;
    bool isEnumNorth(const QString &enumName) const;

signals:
    void tilesetAdded(Tiled::Tileset *ts);
    void tilesetAboutToBeRemoved(Tiled::Tileset *ts);
    void tilesetRemoved(Tiled::Tileset *ts);

private slots:
    void tilesetChanged(Tileset *ts);

private:
    bool parse2Ints(const QString &s, int *pa, int *pb);

private:
    static TileMetaInfoMgr *mInstance;
    TileMetaInfoMgr(QObject *parent = 0);
    ~TileMetaInfoMgr();

    QMap<QString,Tileset*> mTilesetByName;
    QList<Tiled::Tileset*> mRemovedTilesets;

    QStringList mEnumNames;
    QMap<QString,int> mEnums;
    QMap<QString,TilesetMetaInfo*> mTilesetInfo;

    int mRevision;
    int mSourceRevision;
    QString mError;
    bool mHasReadTxt;
};

} // namespace Tiled

#endif // TILEMETAINFOMGR_H
