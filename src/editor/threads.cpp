/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "threads.h"

BaseWorker::BaseWorker(InterruptibleThread *thread) :
    mThread(thread),
    mWorkPending(false),
    mWorkPrevented(false)
{

}

BaseWorker::~BaseWorker()
{
}

bool BaseWorker::aborted()
{
    QMutexLocker locker(&mThread->mMutex);
    return mThread->mInterrupted;
}

void BaseWorker::scheduleWork()
{
    if (aborted())
        return;
    if (mWorkPrevented)
        return;
    if (mWorkPending)
        return;
    mWorkPending = true;
    QMetaObject::invokeMethod(this, "workWrapper", Qt::QueuedConnection);
}

void BaseWorker::allowWork()
{
    mWorkPrevented = false;
}

void BaseWorker::preventWork()
{
    mWorkPrevented = true;
}

void BaseWorker::workWrapper()
{
    IN_WORKER_THREAD

    mWorkPending = false;

    QMutexLocker locker(&mThread->mMutex);
    if (mThread->mInterrupted)
        return;
    mThread->mWorkerBusy = true;
    locker.unlock();

    work();

    locker.relock();
    mThread->mWorkerBusy = false;
    if (mThread->mWaiting)
        mThread->mWaitCondition.wakeOne();
}
