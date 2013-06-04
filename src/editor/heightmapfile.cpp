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

#include "heightmapfile.h"

#include <QDataStream>

HeightMapChunk::HeightMapChunk(int x, int y, int width, int height) :
    mX(x),
    mY(y),
    mWidth(width),
    mHeight(height),
    d(width * height),
    mModified(false)
{
}

void HeightMapChunk::setHeightAt(int x, int y, int height)
{
    if (contains(x, y)) {
        x -= mX, y -= mY;
        d[x + y * mWidth] = height;
        mModified = true;
    }
}

int HeightMapChunk::heightAt(int x, int y)
{
    if (!contains(x, y))
        return 0;
    x -= mX, y -= mY;
    return d[x + y * mWidth];
}

bool HeightMapChunk::read(QDataStream &in)
{
    int skip = 0;
    for (int x = 0; x < mWidth; ++x) {
        for (int y = 0; y < mHeight; ++y) {
            if (skip > 0) {
                --skip;
            } else {
                int count; in >> count;
                if (count == -1) {
                    in >> skip;
                    if (skip > 0) {
                        --skip;
                    }
                }  else {
                    in >> d[x + y * mWidth];
                }
            }
        }
    }
    return true;
}

bool HeightMapChunk::write(QDataStream &out)
{
    int zeroheightcount = 0;
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (d[x + y * mWidth] == 0)
                zeroheightcount++;
            else {
                if (zeroheightcount > 0) {
                    out << qint32(-1);
                    out << qint32(zeroheightcount);
                }
                zeroheightcount = 0;
                out << quint8(d[x + y * mWidth]);
            }
        }
    }
    if (zeroheightcount > 0) {
        out << qint32(-1);
        out << qint32(zeroheightcount);
    }
    mModified = false;
    return true;
}

/////

HeightMapFile::HeightMapFile()
{
}

bool HeightMapFile::create(const QString &fileName, int width, int height)
{
    Q_ASSERT(!mFile.isOpen());
    mFile.setFileName(fileName);
    if (!mFile.open(QIODevice::ReadWrite)) {
        mError = mFile.errorString();
        return false;
    }

    QDataStream d(&mFile);
    d.setByteOrder(QDataStream::LittleEndian);
    d << qint32(width); // #tiles
    d << qint32(height); // #tiles

    int chunksX = width / chunkDim();
    int chunksY = height / chunkDim();
    qint64 positionMapStart = mFile.pos();
    mFile.seek(positionMapStart + chunksX * chunksY * sizeof(qint64));

    QList<qint64> positionMap;
    for (int y = 0; y < chunksY; y++) {
        for (int x = 0; x < chunksX; x++) {
            positionMap += mFile.pos();
            HeightMapChunk chunk(x * chunkDim(), y * chunkDim(), chunkDim(), chunkDim());
            if (!chunk.write(d)) {
                mError = tr("Failed to write chunk");
                mFile.close();
                return false;
            }
        }
    }

    mFile.seek(positionMapStart);
    for (int i = 0; i < chunksX * chunksY; i++)
        d << positionMap[i];

    mFile.close();

    mWidth = width; // #tiles
    mHeight = height; // #tiles

    return true;
}

bool HeightMapFile::open(const QString &fileName)
{
    Q_ASSERT(!mFile.isOpen());
    mFile.setFileName(fileName);
    if (!mFile.open(QIODevice::ReadWrite)) {
        mError = mFile.errorString();
        return false;
    }

    QDataStream in(&mFile);
    in.setByteOrder(QDataStream::LittleEndian);

    qint32 width; in >> width;
    qint32 height; in >> height;
    if (width < 0 || height < 0 || width > 300 * 100 || height > 300 * 100)
        return false;

    mWidth = width;
    mHeight = height;

    return true;
}

HeightMapChunk *HeightMapFile::readChunk(int x, int y)
{
    HeightMapChunk *c = new HeightMapChunk(x * chunkDim(), y * chunkDim(),
                                           chunkDim(), chunkDim());

    QDataStream in(&mFile);
    in.setByteOrder(QDataStream::LittleEndian);

    int chunksX = mWidth / chunkDim();
    mFile.seek(8 + (x + y * chunksX) * sizeof(qint64));
    qint64 chunkStart; in >> chunkStart;
    mFile.seek(chunkStart);
    if (c->read(in))
        return c;
    Q_ASSERT(false);
    return 0;
}

bool HeightMapFile::writeChunk(HeightMapChunk *chunk)
{
    // create a new temp chunk file
    // copy all data from before this chunk to the new file
    // write this chunk
    // copy all data from after this chunk to the new file
    // update the position-map at the start of the file
    return true;

    if (!chunk->modified()) return true;
    QDataStream d(&mFile);
    d.setByteOrder(QDataStream::LittleEndian);
    return chunk->write(d);
}

