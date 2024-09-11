#ifndef DOROUTINE_H
#define DOROUTINE_H

#include <memory>
#include <functional>

#include <ucontext.h>

namespace KSC {

class MallocStackAllocator {
public:
    static void *Alloc(size_t size) { return malloc(size); }
    static void Dealloc(void *vp) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

class Doroutine : public std::enable_shared_from_this<Doroutine> {
// 协程状态
public:
    using ptr = std::shared_ptr<Doroutine>;
    enum State {
        READY,
        RUNNING,
        TERM
    };

// 构造函数
    Doroutine();
    Doroutine(std::function<void()> func, size_t stackSize = 0, bool runInScheduler = false);

// 析构函数
    ~Doroutine();

// resume
    void resume();

// yield
    void yield();

// reset
    void reset(std::function<void()> func);

// 获取协程ID
    uint64_t getId() const { return m_id; }

// 获取协程状态
    State getState() const { return m_state; }

public:
// 设置当前线程正在运行的协程
    static void SetThis(ptr curDoroutine);
// 获取当前线程正在运行的协程
    static ptr GetThis();
// 获取当前线程正在运行的协程的协程id
    static uint64_t GetThisId();
// 协程通用工作函数
    static void WorkFunc();
// 线程主协程初始化
    static void threadMainDoroutineInit();


private:
    uint64_t m_id = -1;
    State m_state = READY;
    uint32_t m_stackSize = 0;
    void *m_stack = nullptr;
    bool m_runInScheduler = false;
    std::function<void()> m_func;
    ucontext_t m_ctx;
};

};


#endif // #ifndef DOROUTINE_H