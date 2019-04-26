#ifndef ASSET_UTIL_H
#define ASSET_UTIL_H

#include <QDebug>

#include <QtCore/qglobal.h>

#if defined(TILED_LIBRARY)
#  define TILEDSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TILEDSHARED_EXPORT Q_DECL_IMPORT
#endif

class non_copyable
{
protected:
    non_copyable() = default;
    ~non_copyable() = default;

    Q_DISABLE_COPY(non_copyable)

    non_copyable(non_copyable &&) = delete;
};

class TILEDSHARED_EXPORT AssetParams {};

class TILEDSHARED_EXPORT AssetPath : public QString
{
public:
    AssetPath()
    {}

    AssetPath(const QString& string)
        : QString(string)
    {}

    bool isValid() const
    {
        return !isEmpty();
    }

    QString getString() const
    {
        return *this;
    }
};

#endif // ASSET_UTIL_H
