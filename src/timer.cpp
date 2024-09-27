#include <iostream>

#include "log.h"
#include "timer.h"
#include "util.h"

namespace KSC {

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

bool Timer::cancel() {
    TimerManager::writeMtx lck(m_manager->m_rwMtx);
    if (m_func) {
        m_func = nullptr;
        auto it = m_manager->m_timers.find(shared_from_this());
        m_manager->m_timers.erase(it);
        return true;
    }
    return false;
}

bool Timer::refresh() {
    TimerManager::writeMtx lck(m_manager->m_rwMtx);
    if (!m_func) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    m_next = KSC::GetElapsedMS() + m_periodInMs;
    m_manager->addTimer(shared_from_this(), lck);
    return true;
}

bool Timer::reset(uint64_t ms, bool fromNow) {
    if (ms == m_periodInMs && !fromNow) {
        return true;
    }
    TimerManager::writeMtx lck(m_manager->m_rwMtx);
    if (!m_func) {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if(it == m_manager->m_timers.end()) {
        return false;
    }
    m_manager->m_timers.erase(it);
    uint64_t start = 0;
    if (fromNow) {
        start = KSC::GetElapsedMS();
    } else {
        start = m_next - m_periodInMs;
    }
    m_periodInMs = ms;
    m_next = start + m_periodInMs;
    m_manager->addTimer(shared_from_this(), lck);
    return true;
}

Timer::Timer(uint64_t ms, std::function<void()> func, bool repeat, TimerManager *manager) 
    : m_periodInMs(ms)
    , m_func(func)
    , m_repeat(repeat)
    , m_manager(manager) {
    m_next = KSC::GetElapsedMS() + m_periodInMs;
}

Timer::Timer(uint64_t next) 
    : m_next(next) {
}

bool Timer::Comparator::operator()(const Timer::ptr &leftTimer, const Timer::ptr &rightTimer) const
{
    if (!leftTimer && !rightTimer) {
        return false;
    }
    if (!leftTimer) {
        return true;
    }
    if (!rightTimer) {
        return false;
    }
    if (leftTimer->m_next < rightTimer->m_next) {
        return true;
    }
    if (leftTimer->m_next > rightTimer->m_next) {
        return false;
    }
    return leftTimer.get() < rightTimer.get();
}

TimerManager::TimerManager() {
    m_previouseTime = KSC::GetElapsedMS();
}

TimerManager::~TimerManager() {
}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> func, bool repeat) {
    Timer::ptr timer(new Timer(ms, func, repeat, this));
    writeMtx lck(m_rwMtx);
    addTimer(timer, lck);
    return timer;
}

static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> func) {
    std::shared_ptr<void> tmp = weak_cond.lock();
    if (tmp) {
        func();
    }
}

Timer::ptr TimerManager::addConditionalTimer(uint64_t ms, std::function<void()> func, std::weak_ptr<void> weakCond, bool repeat) {
    return addTimer(ms, std::bind(OnTimer, weakCond, func), repeat);
}

uint64_t TimerManager::getNextTimer() {
    readMtx lck(m_rwMtx);
    if (m_timers.empty()) {
        return ~0ull;
    }

    const Timer::ptr& next = *m_timers.begin();
    uint64_t nowInMs = KSC::GetElapsedMS();
    if (nowInMs < next->m_next) {
        return next->m_next - nowInMs;
    }
    return 0;
}

void TimerManager::listExpiredFunc(std::vector<std::function<void()>> &funcs) {
    uint64_t nowInMs = KSC::GetElapsedMS();
    std::vector<Timer::ptr> expired;
    {
        readMtx lck(m_rwMtx);
        if (m_timers.empty()) {
            return;
        }
    }
    writeMtx lck(m_rwMtx);
    if (m_timers.empty()) {
        return;
    }
    bool rollover = detectClockRollover(nowInMs);
    if (!rollover && ((*m_timers.begin())->m_next > nowInMs)) {
        return;
    }

    Timer::ptr now_timer(new Timer(nowInMs));
    auto it = rollover ? m_timers.end() : m_timers.lower_bound(now_timer);
    while(it != m_timers.end() && (*it)->m_next == nowInMs) {
        ++it;
    }
    expired.insert(expired.begin(), m_timers.begin(), it);
    m_timers.erase(m_timers.begin(), it);
    funcs.reserve(expired.size());

    for(auto& timer : expired) {
        funcs.push_back(timer->m_func);
        if(timer->m_repeat) {
            timer->m_next = nowInMs + timer->m_periodInMs;
            addTimer(timer, lck);
        } else {
            timer->m_func = nullptr;
        }
    }

}

bool TimerManager::hasTimer() {
    readMtx lck(m_rwMtx);
    return !m_timers.empty();
}

void TimerManager::onTimerInsertedAtFront() {
    SYLAR_LOG_DEBUG(g_logger) << "onTimerInsertedAtFront()";
}

void TimerManager::addTimer(Timer::ptr timer, writeMtx &lck)
{
    auto it = m_timers.insert(timer).first;
    bool isFirst = (it == m_timers.begin()) && !m_isTickled;
    if (isFirst) {
        m_isTickled = true;
    }
    lck.unlock();
    if (isFirst) {
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
    bool rollover = false;
    if(now_ms < m_previouseTime && now_ms < (m_previouseTime - 60 * 60 * 1000)) {
        rollover = true;
    }
    m_previouseTime = now_ms;
    return rollover;
}

};