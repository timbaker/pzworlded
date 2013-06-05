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

#ifndef HEIGHTMAPFILE_H
#define HEIGHTMAPFILE_H

#include <QCoreApplication>
#include <QFile>
#include <QRect>
#include <QString>
#include <QVector>

class HeightMap;

class QDataStream;

class HeightMapChunk
{
public:
    HeightMapChunk(int x, int y, int width, int height);

    int x() const { return mX; }
    int y() const { return mY; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QRect bounds() const { return QRect(mX, mY, mWidth, mHeight); }

    bool contains(int x, int y) { return bounds().contains(x, y); }

    void setHeightAt(int x, int y, int height);
    int heightAt(int x, int y);

    bool read(QDataStream &in);
    bool write(QDataStream &out);

    bool modified() const
    { return mModified; }

    void ref() { ++mRef; }
    bool deref() { Q_ASSERT(mRef > 0); return --mRef == 0; }
    int refCount() const { return mRef; }

private:
    int mX;
    int mY;
    int mWidth;
    int mHeight;
    QVector<quint8> d;
    bool mModified;
    int mRef;
};

class HeightMapFile
{
    Q_DECLARE_TR_FUNCTIONS(HeightMapFile)

public:
    HeightMapFile();
    ~HeightMapFile();

    bool create(const QString &fileName, int width, int height);
    bool open(const QString &fileName);

    HeightMapChunk *requestChunk(int x, int y);
    void releaseChunk(HeightMapChunk *chunk);

    bool save();

    const QString &errorString() const
    { return mError; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int chunkDim() const { return 50; }

    bool validate(const QString &fileName);

private:
    struct header
    {
        quint8 sig[4]; // 'whmp'
        qint32 version;
        qint32 width;
        qint32 height;
    };

    HeightMapChunk *readChunk(int x, int y);
    bool writeChunk(HeightMapChunk *chunk);

    qint64 startOfChunk(int x, int y);
    qint64 startOfChunk(int index);

private:
    QString mError;
    QFile mFile;
    int mWidth;
    int mHeight;
    QList<HeightMapChunk*> mLoadedChunks;
};

#endif // HEIGHTMAPFILE_H
