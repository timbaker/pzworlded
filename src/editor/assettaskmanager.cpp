#include "assettaskmanager.h"

#include "assettask.h"

SINGLETON_IMPL(AssetTaskManager)

AssetTaskManager::AssetTaskManager()
{
    // To make your application perform idle processing (i.e. executing a special function whenever there are no pending events),
    // use a QTimer with 0 timeout. More sophisticated idle processing schemes can be achieved using processEvents().
    m_timer.setInterval(0);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &AssetTaskManager::updateAsyncTransactions);
}

void AssetTaskManager::submit(AssetTask *task)
{
    Q_ASSERT(m_pending.contains(task) == false);
    if (m_pending.contains(task))
        return;
    m_pending << task;

    if (!m_timer.isActive())
        m_timer.start();
}

void AssetTaskManager::updateAsyncTransactions()
{
    int n = m_in_progress.size();
    QList<AssetTask*> in_progress(m_in_progress);
    QList<AssetTask*> done;
    for (int i = 0; i < n; i++)
    {
        AssetTask* item = in_progress.at(i);
        if (!item->isDone())
        {
            continue;
        }
        done << item;
        if (item->isCancelled())
        {
            int dbg = 1;
        }
        else
        {
            item->handleResult();
        }
        item->release();
    }
    for (AssetTask* task : done)
        m_in_progress.removeOne(task);
#if 0
    while (true)
    {
        if (lock.compareAndSet(false, true))
        {
            boolean priority = true;
            if (priority)
            {
                for (int i = 0 ; i < m_added.size(); i++)
                {
                    AsyncItem item = m_added.get(i);
                    int insertAt = m_pending.size();
                    for (int j = 0; j < m_pending.size(); j++)
                    {
                        AsyncItem item1 = m_pending.get(j);
                        if (item.m_task.m_priority > item1.m_task.m_priority)
                        {
                            insertAt = j;
                            break;
                        }
                    }
                    m_pending.add(insertAt, item);
                }
            }
            else
            {
                m_pending.addAll(m_added);
            }
            m_added.clear();
            lock.set(false);
            break;
        }
    }
#endif
    int canAdd = 16 - m_in_progress.size();
    while (canAdd > 0 && !m_pending.isEmpty())
    {
        AssetTask* item = m_pending.takeAt(0);
        if (item->isCancelled())
        {
            item->release();
            continue;
        }
        m_in_progress << item;
        item->execute();
        canAdd--;
    }

    if (m_pending.isEmpty() && m_in_progress.isEmpty())
        m_timer.stop();
}
