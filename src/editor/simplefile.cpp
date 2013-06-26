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

    int lineNumber = 0;
    bool ok;
    SimpleFileBlock block = readBlock(ts, lineNumber, ok);
    if (!ok)
        return false;
    blocks = block.blocks;
    values = block.values;

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

static QString rtrim(QString str)
{
  while (str.size() > 0 && str.at(str.size() - 1).isSpace())
    str.chop(1);
  return str;
}

SimpleFileBlock SimpleFile::readBlock(QTextStream &ts, int &lineNumber, bool &ok)
{
    SimpleFileBlock block;
    block.lineNumber = lineNumber - 1; // approximate
    QString buf;
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        ++lineNumber;
        if (line.contains(QLatin1Char('='))) {
            int n = line.indexOf(QLatin1Char('='));
            SimpleFileKeyValue kv;
            kv.name = line.left(n).trimmed();
            kv.value = line.mid(n + 1).trimmed();
            kv.lineNumber = lineNumber;
            if (kv.value.startsWith(QLatin1Char('['))) {
                while (!ts.atEnd() && !rtrim(kv.value).endsWith(QLatin1Char(']'))) {
                    kv.value += ts.readLine();
                    ++lineNumber;
                }
                kv.value = kv.value.mid(1);
                kv.value.chop(1);
            }
            block.values += kv;
        }
        else if (line.contains(QLatin1Char('{'))) {
            if (line.trimmed().length() != 1) {
                mError = tr("Brace must be on a line by itself (line %1)")
                        .arg(lineNumber);
                ok = false;
                return block;
            }
            SimpleFileBlock childBlock = readBlock(ts, lineNumber, ok);
            if (!ok) return block;
            childBlock.name = buf;
            block.blocks += childBlock;
            buf.clear();
        }
        else if (line.contains(QLatin1Char('}'))) {
            if (line.trimmed().length() != 1) {
                mError = tr("Brace must be on a line by itself (line %1)")
                        .arg(lineNumber);
                ok = false;
                return block;
            }
            break;
        }
        else
            buf += line.trimmed();
    }
    ok = true;
    return block;
}

void SimpleFile::writeBlock(QTextStream &ts, const SimpleFileBlock &block)
{
    INDENT indent(mIndent);
    foreach (SimpleFileKeyValue kv, block.values) {
        if (kv.multiValue && kv.values().size() > 1) {
            ts << indent.text() << kv.name << " = [\n";
            for (int i = 0; i < kv.values().size(); i += kv.multiValueStride) {
                INDENT indent2(mIndent);
                QStringList values = kv.values().mid(i, kv.multiValueStride);
                ts << indent2.text()
                   << values.join(QLatin1String(" "))
                   << "\n";
            }
            ts << indent.text() << "]\n";
            continue;
        }
        ts << indent.text() << kv.name << " = " << kv.value << "\n";
    }
    foreach (SimpleFileBlock child, block.blocks) {
        ts << indent.text() << child.name << "\n" << indent.text() << "{\n";
        writeBlock(ts, child);
        ts << indent.text() << "}\n";
    }
}

/////

int SimpleFileBlock::findBlock(const QString &key) const
{
    for (int i = 0; i < blocks.size(); i++) {
        if (blocks[i].name == key)
            return i;
    }
    return -1;
}

bool SimpleFileBlock::hasValue(const QString &key) const
{
    return findValue(key) >= 0;
}

int SimpleFileBlock::findValue(const QString &key) const
{
    for (int i = 0; i < values.size(); i++) {
        if (values[i].name == key)
            return i;
    }
    return -1;
}

bool SimpleFileBlock::keyValue(const QString &name, SimpleFileKeyValue &kv)
{
    int i = findValue(name);
    if (i >= 0) {
        kv = values[i];
        return true;
    }

    kv.name = name;
    kv.value = QString();
    kv.lineNumber = -1;
    return false;
}

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

void SimpleFileBlock::addValue(const QString &key, const QStringList &values, int stride)
{
    this->values += SimpleFileKeyValue(key, values, stride);
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
    int i = findBlock(name);
    if (i >= 0)
        return blocks[i];
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
        if (kv.multiValue && kv.values().size() > 1) {
            ts << indent.text() << kv.name << " = [\n";
            for (int i = 0; i < kv.values().size(); i += kv.multiValueStride) {
                INDENT indent2(depth);
                QStringList values = kv.values().mid(i, kv.multiValueStride);
                ts << indent2.text()
                   << values.join(QLatin1String(" "))
                   << "\n";
            }
            ts << indent.text() << "]\n";
            continue;
        }
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
