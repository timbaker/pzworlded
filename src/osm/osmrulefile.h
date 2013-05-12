#ifndef OSMRULEFILE_H
#define OSMRULEFILE_H

#include <QColor>
#include <QCoreApplication>
#include <QMap>
#include <QString>

namespace OSM {

class RuleFile
{
    Q_DECLARE_TR_FUNCTIONS(RuleFile)
public:

    class Line
    {
    public:
        QString rule1, rule2;
        int zoom1, zoom2;
        QColor color1, color2;
        qreal width1, width2;
    };

    class Area
    {
    public:
        QString rule_id;
        int zoom1, zoom2;
        QColor color;
    };

    class WayToRule
    {
    public:
        QString key;
        QMap<QString,QString> valueToRule;
    };

    RuleFile();

    bool read(const QString &fileName);

    QString errorString() const
    { return mError; }

private:
    QColor parseColor(const QStringList &sl);
    QColor resolveColor(const QString &s);

public:
    QStringList mRuleIDs;
    QMap<QString,QColor> mColorByName;
    QList<Line> mLines;
    QList<Area> mAreas;
    QMap<QString,WayToRule> mWayToRule;

    QString mError;
};

} // namespace OSM

#endif // OSMRULEFILE_H
