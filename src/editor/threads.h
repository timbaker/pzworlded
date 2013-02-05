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

#ifndef THREADS_H
#define THREADS_H

#include <QCoreApplication>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>

#define IN_APP_THREAD Q_ASSERT(QThread::currentThread() == qApp->thread());
#define IN_WORKER_THREAD Q_ASSERT(QThread::currentThread() != qApp->thread());

class InterruptibleThread;

class BaseWorker : public QObject
{
    Q_OBJECT

public:
    BaseWorker(InterruptibleThread *thread);
    virtual ~BaseWorker();

    bool aborted();

    InterruptibleThread *workerThread() const { return mThread; }

protected:
    void scheduleWork();
    void allowWork();
    void preventWork();

private slots:
    void workWrapper();

private:
    virtual void work() = 0;

    InterruptibleThread *mThread;
    bool mWorkPending;
    bool mWorkPrevented;
};

class InterruptibleThread : public QThread
{
public:
    InterruptibleThread() :
        QThread(),
        mInterrupted(false),
        mWorkerBusy(false),
        mWaiting(false)
    {

    }

    void interrupt(bool wait = false)
    {
        IN_APP_THREAD;
        QMutexLocker locker(&mMutex);
        mInterrupted = true;
        if (wait && mWorkerBusy) {
            mWaiting = true;
            mWaitCondition.wait(&mMutex);
            mWaiting = false;
        }
    }

    void resume() { mInterrupted = false; }

    bool *var() { return &mInterrupted; }

private:
    bool mInterrupted;
    bool mWorkerBusy;
    bool mWaiting;
    QMutex mMutex;
    QWaitCondition mWaitCondition;
    friend class BaseWorker;
};

class Sleep : public QThread
{
public:
    static void sleep(unsigned long secs) {
        QThread::sleep(secs);
    }
    static void msleep(unsigned long msecs) {
        QThread::msleep(msecs);
    }
    static void usleep(unsigned long usecs) {
        QThread::usleep(usecs);
    }
};

#endif // THREADS_H
