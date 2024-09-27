#ifndef SCHDULER_H
#define SCHDULER_H

#include <functional>
#include <memory>
#include <thread>
#include <list>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

#include "doroutine.h"
#include "log.h"

namespace KSC {
class Scheduler {
public:
    Scheduler(size_t threads = 1, bool useCaller = true, const std::string &name = "scheduler");
    virtual ~Scheduler();
    void start();
    void stop();
    const std::string &getName() const { return m_name; }

    template <class DoroutineOrCb>
    void schedule(DoroutineOrCb fc, int thread = -1) {
        bool needTickle = false;
        {
            std::lock_guard<std::mutex> lck(m_mtx);
            needTickle = scheduleNoLock(fc, thread);
        }

        if (needTickle) {
            tickle(); // 唤醒idle协程
        }
    }

    static Scheduler *GetThis();
    // static Doroutine::ptr GetMainDoroutine();
    static Doroutine *GetMainDoroutine();

protected:
    virtual void tickle();
    virtual void idle();
    virtual bool stopping();

    void run();
    void setThis();
    bool hasIdleThreads() { return m_idleThreadCount > 0; }

private:
    struct SchedulerTask {
        Doroutine::ptr doroutine = nullptr;
        std::function<void()> func = nullptr;
        int thread = -1; // 指定执行此任务的线程id，-1表示任意线程均可执行

        SchedulerTask(Doroutine::ptr _doroutine, int _thread) 
            : doroutine(_doroutine), thread(_thread) {}

        SchedulerTask(std::function<void()> _func, int _thread)
            : func(_func), thread(_thread) {}

        SchedulerTask()
            : doroutine(nullptr), func(nullptr), thread(-1) {}        

        void reset() {
            doroutine = nullptr;
            func = nullptr;
            thread = -1;
        }
    };

private:
    template <class DoroutineOrCb>
    bool scheduleNoLock(DoroutineOrCb fc, int thread) {
        bool need_tickle = m_tasks.empty();
        SchedulerTask task(fc, thread);
        if (task.doroutine || task.func) {
            m_tasks.push_back(task);
        }
        return need_tickle;
    }

private:
    std::string m_name; // 协程调度器名称
    std::mutex m_mtx; // 互斥锁
    std::vector<std::thread*> m_threadPool; // 线程池
    std::list<SchedulerTask> m_tasks; // 任务队列
    std::vector<int> m_threadIds;
    size_t m_threadCount; // 工作线程数量，不包括useCaller的主线程
    std::atomic<size_t> m_activeThreadCount {0};
    std::atomic<size_t> m_idleThreadCount {0};

    bool m_useCaller; // 是否use caller
    Doroutine::ptr m_rootDoroutine; // user_caller为true时，调度器所在线程的调度协程
    int m_rootThreadId = 0; // useCaller为true时，调度器所在线程的id

    bool m_stopping = false; // 是否正在停止
};

};

#endif // SCHDULER_H