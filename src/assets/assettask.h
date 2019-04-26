#ifndef ASSETTASK_H
#define ASSETTASK_H

#include "asset_util.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

class Asset;

class TILEDSHARED_EXPORT AssetTask : public non_copyable
{
public:
    AssetTask(Asset* asset)
    {
        m_asset = asset;
    }

    virtual ~AssetTask() {}

    virtual void execute() = 0;
    virtual void cancel() = 0;

    virtual bool isDone() = 0;
    virtual bool isCancelled() = 0;

    virtual void handleResult() = 0;
    virtual void release() = 0;

    Asset* m_asset;
};

class TILEDSHARED_EXPORT BaseAsyncAssetTask : public AssetTask
{
public:
    BaseAsyncAssetTask(Asset* asset)
        : AssetTask(asset)
    {

    }

    void execute() override;
    void cancel() override;

    bool isDone() override { return mFutureWatcher.isFinished(); }
    bool isCancelled() override { return bCancelled && mFutureWatcher.isCanceled(); }

    void connect(QObject* receiver, std::function<void()> f);

    virtual void run() = 0;

    bool bCancelled = false;
    QFuture<void> mFuture;
    QFutureWatcher<void> mFutureWatcher;
};

#endif // ASSETTASK_H
