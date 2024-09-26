#include <iostream>

#include "scheduler.h"
#include "hook.h"
#include "util.h"

namespace KSC {

static thread_local Scheduler *st_scheduler = nullptr; // 当前线程的调度器
static thread_local Doroutine::ptr st_schedulerDoroutine = nullptr; // 当前线程的调度协程

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string &name) 
    : m_useCaller(useCaller) 
    , m_name(name) {

    if (useCaller) {
        --threads;
        Doroutine::threadMainDoroutineInit();
        st_scheduler = this;
        m_rootDoroutine = std::make_shared<Doroutine>(std::bind(&Scheduler::run, this), 0, false); // 调度协程是和主协程进行切换的
        
        st_schedulerDoroutine = m_rootDoroutine;
        m_rootThreadId = KSC::GetThreadId();
        m_threadIds.push_back(m_rootThreadId);
    } else {
        m_rootThreadId = -1;
    }
    m_threadCount = threads;
}

Scheduler::~Scheduler() {
    if (GetThis() == this) {
        st_schedulerDoroutine = nullptr;
        st_scheduler = nullptr;
    }
}

void Scheduler::start() {
    std::lock_guard<std::mutex> lck(m_mtx);
    if (m_stopping) {
        std::cout << "Scheduler is stopping" << std::endl;
        return;
    }
    m_threadPool.resize(m_threadCount);
    for (size_t i = 0; i < m_threadPool.size(); i++) {
        m_threadPool[i] = new std::thread(std::bind(&Scheduler::run, this));
    }
}

void Scheduler::stop() {
    if (stopping()) {
        return;
    }
    m_stopping = true;

    for (size_t i = 0; i < m_threadCount; i++) {
        tickle();
    }

    if (m_rootDoroutine) {
        tickle();
    }

    if (m_rootDoroutine) {
        m_rootDoroutine->resume();
    }

    std::vector<std::thread*> thrs;
    {
        std::lock_guard<std::mutex> lck(m_mtx);
        thrs.swap(m_threadPool);
    }
    for (auto &i : thrs) {
        i->join();
    }
}

Scheduler *Scheduler::GetThis() {
    return st_scheduler;
}

// 这里直接传共享指针会导致引用计数无法减为0，从而内存泄漏
// 具体计数有问题的点在于协程resume的时候swapcontext导致的
// 具体原因还没有分析出来，因此先换成传原始指针
Doroutine *Scheduler::GetMainDoroutine() {
    return st_schedulerDoroutine.get();
}

void Scheduler::tickle() {
    std::cout << "tickle" << std::endl;
}

void Scheduler::idle() {
    std::cout << "idle" << std::endl;
    while (!stopping()) {
        Doroutine::GetThis()->yield();
    }
}

bool Scheduler::stopping() {
    std::lock_guard<std::mutex> lck(m_mtx);
    return m_stopping && m_tasks.empty() && m_activeThreadCount == 0;
}

void Scheduler::run() {
    setHookEnable(true);
    setThis();
    if (KSC::GetThreadId() != m_rootThreadId) {
        Doroutine::threadMainDoroutineInit();
        st_schedulerDoroutine = Doroutine::GetMainThis();
    }

    Doroutine::ptr idleDoroutine = std::make_shared<Doroutine>(std::bind(&Scheduler::idle, this));
    Doroutine::ptr funcDoroutine;

    SchedulerTask task;
    while (true) {
        task.reset();
        bool tickleMe = false;
        {
            std::lock_guard<std::mutex> lck(m_mtx);
            auto it = m_tasks.begin();
            while (it != m_tasks.end()) {
                if (it->thread != -1 && it->thread != KSC::GetThreadId()) {
                    // 指定了调度线程，但不是在当前线程上调度，标记一下需要通知其他线程进行调度，然后跳过这个任务，继续下一个
                    ++it;
                    tickleMe = true;
                    continue;
                }

                if(it->doroutine && it->doroutine->getState() == Doroutine::RUNNING) {
                    ++it;
                    continue;
                }

                task = *it;
                m_tasks.erase(it++);
                ++m_activeThreadCount;
                break;
            }
            tickleMe |= (it != m_tasks.end());
        }

        if (tickleMe) {
            tickle();
        }

        if (task.doroutine) {
            task.doroutine->resume();
            --m_activeThreadCount;
            task.reset();
        } else if (task.func) {
            if (funcDoroutine) {
                funcDoroutine->reset(task.func);
            } else {
                funcDoroutine = std::make_shared<Doroutine>(task.func);
            }
            task.reset();
            funcDoroutine->resume();
            --m_activeThreadCount;
            funcDoroutine.reset();
        } else {
            // 进到这个分支情况一定是任务队列空了，调度idle协程即可
            if (idleDoroutine->getState() == Doroutine::TERM) {
                // 如果调度器没有调度任务，那么idle协程会不停地resume/yield，不会结束，如果idle协程结束了，那一定是调度器停止了
                break;
            }
            ++m_idleThreadCount;
            idleDoroutine->resume();
            --m_idleThreadCount;
        }
    }
}

void Scheduler::setThis() {
    st_scheduler = this;
}
};