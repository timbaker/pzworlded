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

#ifndef SIMPLEFILE_H
#define SIMPLEFILE_H

#include <QString>
#include <QTextStream>

class SimpleFileKeyValue
{
public:
    SimpleFileKeyValue()
    {

    }

    SimpleFileKeyValue(const QString &name, const QString &value) :
        name(name),
        value(value)
    {

    }

    QString name;
    QString value;
};

class SimpleFileBlock
{
public:
    QString name;
    QList<SimpleFileKeyValue> values;
    QList<SimpleFileBlock> blocks;

    int findBlock(const QString &key) const;
    int findValue(const QString &key) const;

    SimpleFileKeyValue keyValue(const char *name)
    { return keyValue(QLatin1String(name)); }
    SimpleFileKeyValue keyValue(const QString &name);

    QString value(const char *key)
    { return value(QLatin1String(key)); }

    QString value(const QString &key);

    void addValue(const char *key, const QString &value)
    { addValue(QLatin1String(key), value); }

    void addValue(const QString &key, const QString &value);

    void renameValue(const char *key, const QString &newName)
    { renameValue(QLatin1String(key), newName); }

    void renameValue(const QString &key, const QString &newName);

    void replaceValue(const char *key, const QString &value, bool atEnd = true)
    { replaceValue(QLatin1String(key), value, atEnd); }

    void replaceValue(const QString &key, const QString &value, bool atEnd = true);

    SimpleFileBlock block(const char *name)
    { return block(QLatin1String(name)); }

    SimpleFileBlock block(const QString &name);

    QString toString(int depth = -1);
    void write(QTextStream &ts, int indent);

    void print();

    class INDENT
    {
    public:
        INDENT(int &depth) :
            mDepth(depth)
        {
            ++mDepth;
        }
        ~INDENT()
        {
            --mDepth;
        }

        QString text() const
        { return QString(QLatin1Char(' ')).repeated(mDepth * 4); }

    private:
        int &mDepth;
    };
};

class SimpleFile : public SimpleFileBlock
{
public:
    SimpleFile();

    bool read(const QString &filePath);

    bool write(const QString &filePath);

    QString errorString() const
    { return mError; }

    void setVersion(int version)
    { mVersion = version; }

    int version() const
    { return mVersion; }

private:
    SimpleFileBlock readBlock(QTextStream &ts);
    void writeBlock(QTextStream &ts, const SimpleFileBlock &block);

    QString mError;
    int mVersion;

    int mIndent;
};

#endif // SIMPLEFILE_H
