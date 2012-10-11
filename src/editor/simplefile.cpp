#include "simplefile.h"

#include <QDebug>
#include <QFile>
#include <QStringList>
#include <QTextStream>

SimpleFile::SimpleFile()
{
}

bool SimpleFile::read(const QString &filePath)
{
    values.clear();
    blocks.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
        } else {
            buf += line.trimmed();
        }
    }

    file.close();

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

/////

QString SimpleFileBlock::value(const QString &key)
{
    foreach (SimpleFileKeyValue kv, values) {
        if (kv.name == key)
            return kv.value;
    }
    return QString();
}

SimpleFileBlock SimpleFileBlock::block(const QString &name)
{
    foreach (SimpleFileBlock block, blocks) {
        if (block.name == name)
            return block;
    }
    return SimpleFileBlock();
}

void SimpleFileBlock::print()
{
    qDebug() << "block" << name;
    foreach (SimpleFileKeyValue value, values)
        qDebug() << value.name << " = " << value.value;
    foreach (SimpleFileBlock block, blocks)
        block.print();
}
