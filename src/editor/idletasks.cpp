#include "idletasks.h"

SINGLETON_IMPL(IdleTasks)

IdleTasks::IdleTasks()
{
    // To make your application perform idle processing (i.e. executing a special function whenever there are no pending events),
    // use a QTimer with 0 timeout. More sophisticated idle processing schemes can be achieved using processEvents().
    m_timer.setInterval(0);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &IdleTasks::onTimer);
    m_timer.start();
}

void IdleTasks::blockTasks(bool block)
{
    if (block)
    {
        ++m_block_depth;
        if (m_block_depth > 1)
            return;
    }
    else
    {
        --m_block_depth;
        Q_ASSERT(m_block_depth >= 0);
        if (m_block_depth > 0)
            return;
    }

//    blockSignals(block);
    if (block && m_timer.isActive())
        m_timer.stop();
    else if (!block && !m_timer.isActive())
        m_timer.start();
}

void IdleTasks::onTimer()
{
    if (m_in_idle_time)
        return;
    m_in_idle_time = true;
    emit idleTime();
    m_in_idle_time = false;
}
