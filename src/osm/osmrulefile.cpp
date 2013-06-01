#include "osmrulefile.h"

#include "simplefile.h"

#include <QDebug>
#include <QFileInfo>

#if defined(Q_OS_WIN) && (_MSC_VER == 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

using namespace OSM;

RuleFile::RuleFile()
{
}

bool RuleFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    SimpleFileKeyValue kv;
    if (simple.keyValue("rule_id", kv))
        mRuleIDs = kv.values();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("colors")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                mColorByName[kv.name] = parseColor(kv.values());
                qDebug() << kv.name << mColorByName[kv.name];
            }
        } else if (block.name == QLatin1String("line")) {
            Line l;
            if (block.keyValue("rule_id", kv)) {
                l.rule1 = kv.values()[0];
                l.rule2 = (kv.values().size() > 1) ? kv.values()[1] : QString();
                if (!mRuleIDs.contains(l.rule1)) {
                    mError = tr("Line %1: Unknown rule \"%2\"").arg(kv.lineNumber).arg(l.rule1);
                    return false;
                }
                if (!l.rule2.isEmpty() && !mRuleIDs.contains(l.rule2)) {
                    mError = tr("Line %1: Unknown rule \"%2\"").arg(kv.lineNumber).arg(l.rule2);
                    return false;
                }
            }
            if (block.keyValue("color", kv)) {
                QStringList values = kv.values();
                if (values.size() == 3)
                    l.color1 = parseColor(values);
                else
                    l.color1 = resolveColor(kv.values()[0]);
                if (kv.values().size() == 2)
                    l.color2 = resolveColor(kv.values()[1]);
            }
            if (block.keyValue("width", kv)) {
                l.width1 = kv.values()[0].toDouble();
                if (kv.values().size() > 1)
                    l.width2 = kv.values()[1].toDouble();
            }
            mLines += l;
        } else if (block.name == QLatin1String("area")) {
            Area a;
            if (block.keyValue("rule_id", kv)) {
                a.rule_id = block.value(("rule_id"));
                if (!mRuleIDs.contains(a.rule_id)) {
                    mError = tr("Line %1: Unknown rule \"%2\"").arg(kv.lineNumber).arg(a.rule_id);
                    return false;
                }
            }
            a.color = resolveColor(block.value("color"));
            mAreas += a;
        } else if (block.name == QLatin1String("way_kv_to_rule")) {
            WayToRule wtr;
            if (block.keyValue("osm_key", kv))
                wtr.key = kv.value;
            if (wtr.key.isEmpty()) {
                mError = tr("Line %1: missing osm_key").arg(block.lineNumber);
                return false;
            }
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name != QLatin1String("osm_key")) {
                    if (!mRuleIDs.contains(kv.value)) {
                        mError = tr("Line %1: Unknown rule \"%2\"").arg(kv.lineNumber).arg(kv.value);
                        return false;
                    }
                    wtr.valueToRule[kv.name] = kv.value;
                }
            }
            mWayToRule[wtr.key] = wtr;
        }
    }

    return true;
}

QColor RuleFile::parseColor(const QStringList &sl)
{
    if (sl.size() == 3) {
        return QColor(sl[0].toInt(0,0), sl[1].toInt(0,0), sl[2].toInt(0,0), 200);
    }
    return QColor();
}

QColor RuleFile::resolveColor(const QString &s)
{
    QStringList sl = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (sl.size() == 3) {
        return QColor(sl[0].toInt(0,0), sl[1].toInt(0,0), sl[2].toInt(0,0), 200);
    }
    if (sl.size() == 1 && mColorByName.contains(sl[0]))
        return mColorByName[sl[0]];
    return QColor();
}
