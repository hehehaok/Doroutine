#ifndef TIMER_H
#define TIMER_H

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <set>
#include <memory>
#include <functional>

namespace KSC {

class TimerManager;


class Timer : public std::enable_shared_from_this<Timer> {
friend class TimerManager;
public:
    using ptr = std::shared_ptr<Timer>;

    bool cancel(); // 取消定时器
    bool refresh(); // 刷新设置定时器的执行时间
    bool reset(uint64_t ms, bool fromNow); // 重置定时器执行时间

private:
    Timer(uint64_t ms, std::function<void()> func, bool repeat, TimerManager* manager);
    Timer(uint64_t next);

private:
    bool m_repeat = false; // 是否重复循环定时器
    uint64_t m_periodInMs = 0; // 定时器执行周期
    uint64_t m_next = 0; // 定时器执行的绝对时间
    std::function<void()> m_func; // 定时器对应的回调函数
    TimerManager* m_manager = nullptr; // 定时器所属的管理器

private:
    struct Comparator {
        bool operator()(const Timer::ptr& leftTimer, const Timer::ptr& rightTimer) const;
    };
};

class TimerManager {
friend class Timer;
public:
    using readMtx = std::shared_lock<std::shared_mutex>;
    using writeMtx = std::unique_lock<std::shared_mutex>;

    TimerManager();
    virtual ~TimerManager();

    Timer::ptr addTimer(uint64_t ms, std::function<void()> func, bool repeat = false); // 添加定时器
    uint64_t getNextTimer(); // 到最近一个定时器执行的时间间隔（毫秒）
    void listExpiredFunc(std::vector<std::function<void()>>& funcs); // 获取需要执行的定时器回调的回调列表
    bool hasTimer(); // 是否有定时器

protected:
    virtual void onTimerInsertedAtFront(); // 当有新的定时器插入到定时器容器的首部时，执行该函数
    void addTimer(Timer::ptr timer, writeMtx &lck); // 添加定时器（同时检测是否执行onTimerInsertedAtFront）

private:
    bool detectClockRollover(uint64_t now_ms); // 检查服务器时间是否被调后了

private:
    std::shared_mutex m_rwMtx;
    std::set<Timer::ptr, Timer::Comparator> m_timers;
    bool m_isTickled = false; // 是否已经触发过onTimerInsertedAtFront()
    uint64_t m_previouseTime = 0; // 上一次的执行时间
};

};


#endif