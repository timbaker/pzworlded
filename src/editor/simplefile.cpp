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

#include "simplefile.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>

SimpleFile::SimpleFile() :
    mVersion(0)
{
}

bool SimpleFile::read(const QString &filePath)
{
    values.clear();
    blocks.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        mError = file.errorString();
        return false;
    }

    QTextStream ts(&file);
    QString buf;
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.contains(QLatin1Char('{'))) {
            SimpleFileBlock block = readBlock(ts);
            block.name = buf;
            blocks += block;
            buf.clear();
        } else if (line.contains(QLatin1Char('='))) {
            QStringList split = line.trimmed().split(QLatin1Char('='));
            SimpleFileKeyValue kv;
            kv.name = split[0].trimmed();
            kv.value = split[1].trimmed();
            values += kv;
        } else {
            buf += line.trimmed();
        }
    }

    file.close();

    mVersion = value("version").toInt(); // will be zero for old files

    return true;
}

bool SimpleFile::write(const QString &filePath)
{
    QTemporaryFile tempFile;
    if (!tempFile.open(/*QIODevice::WriteOnly | QIODevice::Text*/)) {
        mError = tempFile.errorString();
        return false;
    }

    replaceValue("version", QString::number(mVersion), false);

    QTextStream ts(&tempFile);
    mIndent = -1;
    writeBlock(ts, *this);

    if (tempFile.error() != QFile::NoError) {
        mError = tempFile.errorString();
        return false;
    }

    // foo.txt -> foo.txt.bak
    QFileInfo destInfo(filePath);
    QString backupPath = filePath + QLatin1String(".bak");
    QFile backupFile(backupPath);
    if (destInfo.exists()) {
        if (backupFile.exists()) {
            if (!backupFile.remove()) {
                mError = QString(QLatin1String("Error deleting file!\n%1\n\n%2"))
                        .arg(backupPath)
                        .arg(backupFile.errorString());
                return false;
            }
        }
        QFile destFile(filePath);
        if (!destFile.rename(backupPath)) {
            mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                    .arg(filePath)
                    .arg(backupPath)
                    .arg(destFile.errorString());
            return false;
        }
    }

    // /tmp/tempXYZ -> foo.txt
    tempFile.close();
    if (!tempFile.rename(filePath)) {
        mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                .arg(tempFile.fileName())
                .arg(filePath)
                .arg(tempFile.errorString());
        // Try to un-rename the backup file
        if (backupFile.exists())
            backupFile.rename(filePath); // might fail
        return false;
    }

    // If anything above failed, the temp file should auto-remove, but not after
    // a successful save.
    tempFile.setAutoRemove(false);

    return true;
}

SimpleFileBlock SimpleFile::readBlock(QTextStream &ts)
{
    SimpleFileBlock block;
    QString buf;
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.contains(QLatin1Char('{'))) {
            SimpleFileBlock childBlock = readBlock(ts);
            childBlock.name = buf;
            block.blocks += childBlock;
            buf.clear();
        }
        else if (line.contains(QLatin1Char('}'))) {
            break;
        }
        else if (line.contains(QLatin1Char('='))) {
            QStringList split = line.trimmed().split(QLatin1Char('='));
            SimpleFileKeyValue kv;
            kv.name = split[0].trimmed();
            kv.value = split[1].trimmed();
            block.values += kv;
        }
        else
            buf += line.trimmed();
    }
    return block;
}

void SimpleFile::writeBlock(QTextStream &ts, const SimpleFileBlock &block)
{
    INDENT indent(mIndent);
    foreach (SimpleFileKeyValue kv, block.values) {
        ts << indent.text() << kv.name << " = " << kv.value << "\n";
    }
    foreach (SimpleFileBlock child, block.blocks) {
        ts << indent.text() << child.name << "\n" << indent.text() << "{\n";
        writeBlock(ts, child);
        ts << indent.text() << "}\n";
    }
}

/////

QString SimpleFileBlock::value(const QString &key)
{
    foreach (SimpleFileKeyValue kv, values) {
        if (kv.name == key)
            return kv.value;
    }
    return QString();
}

void SimpleFileBlock::addValue(const QString &key, const QString &value)
{
    values += SimpleFileKeyValue(key, value);
}

void SimpleFileBlock::renameValue(const QString &key, const QString &newName)
{
    for (int i = 0; i < values.count(); i++) {
        if (values[i].name == key) {
            values[i].name = newName;
            return;
        }
    }
}

void SimpleFileBlock::replaceValue(const QString &key, const QString &value,
                                   bool atEnd)
{
    for (int i = 0; i < values.count(); i++) {
        if (values[i].name == key) {
            values[i].value = value;
            return;
        }
    }
    int index = atEnd ? values.count() : 0;
    values.insert(index, SimpleFileKeyValue(key, value));
}

SimpleFileBlock SimpleFileBlock::block(const QString &name)
{
    foreach (SimpleFileBlock block, blocks) {
        if (block.name == name)
            return block;
    }
    return SimpleFileBlock();
}

QString SimpleFileBlock::toString(int depth)
{
    QString result;
    QTextStream ts(&result);
    write(ts, depth);
    return result;
}

void SimpleFileBlock::write(QTextStream &ts, int depth)
{
    INDENT indent(depth);
    foreach (SimpleFileKeyValue kv, values) {
        ts << indent.text() << kv.name << " = " << kv.value << "\n";
    }
    foreach (SimpleFileBlock child, blocks) {
        ts << indent.text() << child.name << "\n" << indent.text() << "{\n";
        child.write(ts, depth);
        ts << indent.text() << "}\n";
    }
}

void SimpleFileBlock::print()
{
    qDebug() << "block" << name;
    foreach (SimpleFileKeyValue value, values)
        qDebug() << value.name << " = " << value.value;
    foreach (SimpleFileBlock block, blocks)
        block.print();
}
