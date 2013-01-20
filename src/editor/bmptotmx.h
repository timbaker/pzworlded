/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef BMPTOTMX_H
#define BMPTOTMX_H

#include <QImage>
#include <QMap>
#include <QObject>
#include <QStringList>

class WorldBMP;
class WorldCell;
class WorldDocument;

class BMPToTMXImages
{
public:
    QString mPath;
    QImage mBmp;
    QImage mBmpVeg;
    QRect mBounds; // cells covered
};

class BMPToTMX : public QObject
{
    Q_OBJECT
public:
    static BMPToTMX *instance();
    static void deleteInstance();

    enum GenerateMode {
        GenerateAll,
        GenerateSelected
    };

    bool generateWorld(WorldDocument *worldDoc, GenerateMode mode);
    bool generateCell(WorldCell *cell);

    QString errorString() const { return mError; }

    BMPToTMXImages *getImages(const QString &path, const QPoint &origin);
    QSize validateImages(const QString &path);

    void assignMapsToCells(WorldDocument *worldDoc, GenerateMode mode);

    QString defaultRulesFile() const;
    QString defaultBlendsFile() const;
    QString defaultMapBaseXMLFile() const;

    class Tileset
    {
    public:
        int firstGID;
        QString name;
    };

    class ConversionEntry
    {
    public:
        int bitmapIndex;
        QRgb color;
        QRgb condition;
        QStringList tileChoices;
        QString targetLayer;

        ConversionEntry(int bitmapIndex, QRgb col, QStringList tiles, QString layer, QRgb condition)
        { this->bitmapIndex = bitmapIndex, color = col, tileChoices = tiles, targetLayer = layer, this->condition = condition; }
        ConversionEntry(int bitmapIndex, QRgb col, QStringList tiles, QString layer)
        { this->bitmapIndex = bitmapIndex, color = col, tileChoices = tiles, targetLayer = layer, this->condition = qRgb(0, 0, 0); }
        ConversionEntry(int bitmapIndex, QRgb col, QString tile, QString layer);
        ConversionEntry(int bitmapIndex, QRgb col, QString tile, QString layer, QRgb condition);
    };

    class Blend
    {
    public:
        enum Direction {
            Unknown,
            N,
            S,
            E,
            W,
            NW,
            NE,
            SW,
            SE
        };

        QString targetLayer;
        QString mainTile;
        QString blendTile;
        Direction dir;
        QStringList ExclusionList;

        Blend()
        {}
        Blend(const QString &layer, const QString &main, const QString &blend, Direction dir, const QStringList &exclusions)
            : targetLayer(layer), mainTile(main), blendTile(blend), dir(dir), ExclusionList(exclusions)
        {}

        bool isNull()
        { return mainTile.isEmpty(); }
    };


private:
    bool shouldGenerateCell(WorldCell *cell, int &bmpIndex);

    QImage loadImage(const QString &path, const QString &suffix = QString());
    bool LoadBaseXML();
    bool LoadRules();
    bool setupBlends();

    void AddConversion(int bitmapIndex, QRgb col, QStringList tiles, QString layer, QRgb condition);
    void AddConversion(int bitmapIndex, QRgb col, QStringList tiles, QString layer);

    bool BlendMap();
    QString getNeighbouringTile(int x, int y);
    Blend getBlendRule(int x, int y, const QString &floorTile, const QString &layer);

    QString toCSV(int floor, QVector<QVector<QVector<QString> > > &Entries);
    int getGIDFromTileName(const QString &name);

    void assignMapToCell(WorldCell *cell);

    QString tmxNameForCell(WorldCell *cell, WorldBMP *bmp);

    void reportUnknownColors();

private:
    Q_DISABLE_COPY(BMPToTMX)

    explicit BMPToTMX(QObject *parent = 0);
    ~BMPToTMX();

    static BMPToTMX *mInstance;

    WorldDocument *mWorldDoc;
    QList<BMPToTMXImages*> mImages;
    QMap<QRgb,QList<ConversionEntry> > Conversions;
    QVector<QVector<QVector<QString> > > Entries;
    QByteArray baseXML;
    QMap<QString,Tileset*> Tilesets;
    QList<Tileset*> TilesetList;
    QList<Blend> blendList;
    QList<QString> blendLayers;
    QString mError;

    struct UnknownColor {
        QRgb rgb;
        QPoint xy;
    };
    QMap<QString,QMap<QRgb,UnknownColor> > mUnknownColors;
    QMap<QString,QMap<QRgb,UnknownColor> > mUnknownVegColors;
};

#endif // BMPTOTMX_H
