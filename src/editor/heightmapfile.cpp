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
#include <QDebug>
#include <QTemporaryFile>

#define VERSION1 1
#define VERSION_LATEST VERSION1

HeightMapChunk::HeightMapChunk(int x, int y, int width, int height) :
    mX(x),
    mY(y),
    mWidth(width),
    mHeight(height),
    d(width * height),
    mModified(false),
    mRef(0)
{
}

void HeightMapChunk::setHeightAt(int x, int y, int height)
{
    if (contains(x, y)) {
        x -= mX, y -= mY;
        d[x + y * mWidth] = qBound(0, height, 255);
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
    quint8 sig[4];
    in >> sig[0]; in >> sig[1]; in >> sig[2]; in >> sig[3];
    if (memcmp(sig, "chnk", 4))
        return false;

    qint32 skip = 0;
    qint32 nonzero = 0;
    for (int x = 0; x < mWidth; ++x) {
        for (int y = 0; y < mHeight; ++y) {
            if (skip > 0) {
                --skip;
            } else if (nonzero > 0) {
                in >> d[x + y * mWidth];
                --nonzero;
            } else {
                qint32 count; in >> count;
                if (count == -1) {
                    in >> skip;
                    if (skip > 0) {
                        --skip;
                    }
                }  else {
                    nonzero = count;
                    if (nonzero > 0) {
                        in >> d[x + y * mWidth];
                        --nonzero;
                    }
                }
            }
        }
    }
    return true;
}

bool HeightMapChunk::write(QDataStream &out)
{
    out << quint8('c') << quint8('h') << quint8('n') << quint8('k');
    int zeroheightcount = 0;
    QVector<quint8> nonzero;
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (d[x + y * mWidth] == 0) {
                if (nonzero.size()) {
                    Q_ASSERT(zeroheightcount == 0);
                    out << qint32(nonzero.size());
                    foreach (quint8 h, nonzero)
                        out << h;
                    nonzero.clear();
                }
                zeroheightcount++;
            } else {
                if (zeroheightcount > 0) {
                    Q_ASSERT(nonzero.size() == 0);
                    out << qint32(-1);
                    out << qint32(zeroheightcount);
                    zeroheightcount = 0;
                }
                nonzero << d[x + y * mWidth];
            }
        }
    }
    if (nonzero.size()) {
        Q_ASSERT(zeroheightcount == 0);
        out << qint32(nonzero.size());
        foreach (quint8 h, nonzero)
            out << h;
    }
    if (zeroheightcount > 0) {
        Q_ASSERT(nonzero.size() == 0);
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

HeightMapFile::~HeightMapFile()
{
    qDeleteAll(mLoadedChunks);
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
    d << quint8('w') << quint8('h') << quint8('m') << quint8('p');
    d << qint32(VERSION_LATEST);
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
#ifndef QT_NO_DEBUG
    if (!validate(fileName))
        return false;
#endif

    Q_ASSERT(!mFile.isOpen());
    mFile.setFileName(fileName);
    if (!mFile.open(QIODevice::ReadWrite)) {
        mError = mFile.errorString();
        return false;
    }

    QDataStream in(&mFile);
    in.setByteOrder(QDataStream::LittleEndian);
    quint8 sig[4];
    in >> sig[0]; in >> sig[1]; in >> sig[2]; in >> sig[3];
    if (memcmp(sig, "whmp", 4)) {
        mError = tr("Unrecognized file format");
        return false;
    }
    qint32 version; in >> version;
    if (version != VERSION1) {
        mError = tr("Unknown version number '%1'").arg(version);
        return false;
    }

    qint32 width; in >> width;
    qint32 height; in >> height;
    if (width < 0 || height < 0 || width > 300 * 100 || height > 300 * 100) {
        mError = tr("World is greater than 100 cells wide/tall.  Assuming corrupted file.");
        return false;
    }

    Q_ASSERT(mFile.pos() == sizeof(header));

    mWidth = width;
    mHeight = height;

    return true;
}

HeightMapChunk *HeightMapFile::requestChunk(int x, int y)
{
    foreach (HeightMapChunk *c, mLoadedChunks) {
        if (c->contains(x, y)) {
            qDebug() << "re-used chunk " << x << "," << y;
            c->ref();
            return c;
        }
    }
    if (HeightMapChunk *c = readChunk(x, y)) {
        c->ref();
        mLoadedChunks += c;
        qDebug() << "loaded chunk " << x << "," << y;
        return c;
    }
    return 0;
}

void HeightMapFile::releaseChunk(HeightMapChunk *chunk)
{
    if (chunk->deref()) {
        // Modified chunks are kept in memory so they can be saved later.
        if (chunk->modified())
            return;
        mLoadedChunks.removeOne(chunk);
        delete chunk;
    }
}

bool HeightMapFile::save()
{
    foreach (HeightMapChunk *chunk, mLoadedChunks) {
        if (chunk->modified()) {
            if (!writeChunk(chunk)) {
                return false;
            }
        }
        if (chunk->refCount() == 0) {
            mLoadedChunks.removeOne(chunk);
            delete chunk;
        }
    }
    return true;
}

bool HeightMapFile::validate(const QString &fileName)
{
    QFile file;
    file.setFileName(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = file.errorString();
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    quint8 sig[4];
    in >> sig[0]; in >> sig[1]; in >> sig[2]; in >> sig[3];
    if (memcmp(sig, "whmp", 4)) {
        mError = tr("Unrecognized file format");
        return false;
    }
    qint32 version; in >> version;
    if (version != VERSION1) {
        mError = tr("Unknown version number '%1'").arg(version);
        return false;
    }

    qint32 width; in >> width;
    qint32 height; in >> height;
    if (width < 0 || height < 0 || width > 300 * 100 || height > 300 * 100) {
        mError = tr("World is greater than 100 cells wide/tall.  Assuming corrupted file.");
        return false;
    }

    Q_ASSERT(file.pos() == sizeof(header));

    int chunksX = width / chunkDim();
    int chunksY = height / chunkDim();
    QVector<qint64> positionMap;
    for (int i = 0; i < chunksX * chunksY; i++) {
        qint64 pos; in >> pos;
        if (pos < sizeof(header) || pos >= file.size()) {
            mError = tr("Invalid chunk offset");
            return false;
        }
        positionMap += pos;
    }

    foreach (qint64 pos, positionMap) {
        file.seek(pos);
        quint8 sig[4];
        in >> sig[0]; in >> sig[1]; in >> sig[2]; in >> sig[3];
        if (memcmp(sig, "chnk", 4)) {
            mError = tr("Expected 'chnk' at start of chunk");
            return false;
        }
    }

    return true;
}

HeightMapChunk *HeightMapFile::readChunk(int x, int y)
{
    x /= chunkDim();
    y /= chunkDim();
    HeightMapChunk *c = new HeightMapChunk(x * chunkDim(), y * chunkDim(), chunkDim(), chunkDim());

    mFile.seek(startOfChunk(x, y));
    QDataStream in(&mFile);
    in.setByteOrder(QDataStream::LittleEndian);
    if (c->read(in))
        return c;
    Q_ASSERT(false);
    return 0;
}

bool HeightMapFile::writeChunk(HeightMapChunk *chunk)
{
    // create a new temporary heightmap file
    // copy all data from before this chunk to the new file
    // write this chunk
    // copy all data from after this chunk to the new file
    // update the position-map at the start of the file
    // replace this file with the temporary file
    QTemporaryFile temp;
    if (!temp.open()) {
        mError = temp.errorString();
        return false;
    }

    QDataStream out(&temp);
    out.setByteOrder(QDataStream::LittleEndian);

    int chunksX = width() / chunkDim();
    int chunksY = height() / chunkDim();
    int cx = chunk->x() / chunkDim(), cy = chunk->y() / chunkDim();
    int chunkIndex = cx + cy * chunksX;
    qint64 startOfThisChunk = startOfChunk(chunkIndex);
    mFile.seek(0);
    QByteArray ba = mFile.read(startOfThisChunk);
    temp.write(ba);
    chunk->write(out);
    qint64 endOfChunk = temp.pos();
    qint64 endOfFile = temp.pos();

    if (chunkIndex < chunksX * chunksY - 1) { // this isn't the last chunk
        qint64 startOfNextChunk = startOfChunk(chunkIndex + 1);
        mFile.seek(startOfNextChunk);
        ba = mFile.readAll();
        temp.write(ba);
        endOfFile = temp.pos();

        mFile.seek(sizeof(header) + (chunkIndex + 1) * sizeof(qint64));
        QDataStream in(&mFile);
        in.setByteOrder(QDataStream::LittleEndian);
        QVector<qint64> positionMap(chunksX * chunksY - (chunkIndex + 1));
        Q_ASSERT(positionMap.size() + (chunkIndex + 1) == chunksX * chunksY);
        for (int i = chunkIndex + 1; i < chunksX * chunksY; i++) {
            in >> positionMap[i - (chunkIndex + 1)];
            positionMap[i - (chunkIndex + 1)] += endOfChunk - startOfNextChunk;
        }

        temp.seek(sizeof(header) + (chunkIndex + 1) * sizeof(qint64));
        foreach (qint64 pos, positionMap)
            out << pos;
    }

#ifndef QT_NO_DEBUG
    if (!validate(mFile.fileName())) {
        return false;
    }
#endif

    temp.seek(endOfFile);

    mFile.close();
    if (!mFile.remove()) {
        mError = tr("Error removing existing heightmap file.\n") + mFile.errorString();
        return false;
    }
    temp.setAutoRemove(false);
    if (!temp.rename(mFile.fileName())) {
        mError = tr("Error renaming temporary heightmap file to its new location.\n") + temp.errorString();
        return false;
    }
    temp.close();
    if (!mFile.open(QIODevice::ReadWrite)) {
        mError = tr("Error reopening the heightmap file after saving.\n") + mFile.errorString();
        return false;
    }

    return true;
#if 0
    if (!chunk->modified()) return true;
    QDataStream d(&mFile);
    d.setByteOrder(QDataStream::LittleEndian);
    return chunk->write(d);
#endif
}

qint64 HeightMapFile::startOfChunk(int x, int y)
{
    int chunksX = mWidth / chunkDim();
    return startOfChunk(x + y * chunksX);
}

qint64 HeightMapFile::startOfChunk(int index)
{
    mFile.seek(sizeof(header) + index * sizeof(qint64));

    QDataStream in(&mFile);
    in.setByteOrder(QDataStream::LittleEndian);
    qint64 chunkStart; in >> chunkStart;
    return chunkStart;
}

