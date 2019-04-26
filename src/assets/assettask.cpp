#include "assettask.h"

void BaseAsyncAssetTask::execute()
{
    mFuture = QtConcurrent::run(this, &BaseAsyncAssetTask::run);
    mFutureWatcher.setFuture(mFuture);
}

void BaseAsyncAssetTask::cancel()
{
    bCancelled = true;
    mFuture.cancel();
}

void BaseAsyncAssetTask::connect(QObject *receiver, std::function<void()> f)
{
    QObject::connect(&mFutureWatcher, &QFutureWatcher<void>::finished, receiver, f);
}
