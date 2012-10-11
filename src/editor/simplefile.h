#ifndef SIMPLEFILE_H
#define SIMPLEFILE_H

#include <QString>
#include <QTextStream>

class SimpleFileKeyValue
{
public:
    QString name;
    QString value;
};

class SimpleFileBlock
{
public:
    QString name;
    QList<SimpleFileKeyValue> values;
    QList<SimpleFileBlock> blocks;

    QString value(const char *key)
    { return value(QLatin1String(key)); }

    QString value(const QString &key);

    SimpleFileBlock block(const char *name)
    { return block(QLatin1String(name)); }

    SimpleFileBlock block(const QString &name);

    void print();
};


class SimpleFile : public SimpleFileBlock
{
public:
    SimpleFile();

    bool read(const QString &filePath);

private:
    SimpleFileBlock readBlock(QTextStream &ts);
};

#endif // SIMPLEFILE_H
