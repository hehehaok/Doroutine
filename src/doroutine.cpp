#include <atomic>
#include <thread>

#include "log.h"
#include "doroutine.h"
#include "scheduler.h"

namespace KSC {

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

static std::atomic<uint64_t> s_doroutineId {0};
static std::atomic<uint64_t> s_doroutineCount {0};

static thread_local Doroutine::ptr st_threadCurdoroutine = nullptr;
static thread_local Doroutine::ptr st_threadMainDoroutine = nullptr;

Doroutine::Doroutine() {
    m_state = RUNNING;

    if (getcontext(&m_ctx)) {
        SYLAR_LOG_ERROR(g_logger) << "getcontext wrong!";
    }

    ++s_doroutineCount;
    m_id = s_doroutineId++;
    SYLAR_LOG_DEBUG(g_logger) << "thread " << std::this_thread::get_id() << "'s main doroutine starts!";
}

Doroutine::Doroutine(std::function<void()> func, size_t stackSize, bool runInScheduler)
    : m_id(s_doroutineId++)
    , m_func(func)
    , m_runInScheduler(runInScheduler) {
    
    m_stackSize = stackSize ? stackSize : 1024 * 128;
    m_stack = StackAllocator::Alloc(m_stackSize);

    if (getcontext(&m_ctx)) {
        SYLAR_LOG_ERROR(g_logger) << "getcontext wrong!";
    }

    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_size = m_stackSize;
    m_ctx.uc_stack.ss_sp = m_stack;

    makecontext(&m_ctx, &Doroutine::WorkFunc, 0);

    ++s_doroutineCount;
}

Doroutine::~Doroutine() {
    --s_doroutineCount;
    if (m_stack) {
        StackAllocator::Dealloc(m_stack);
    } else {
        SetThis(nullptr);
    }
    SYLAR_LOG_DEBUG(g_logger) << "doroutine " << m_id << "'s ~Doroutine done!";
}

void Doroutine::resume() {
    SetThis(shared_from_this()); // 切换到当前协程
    m_state = RUNNING;
    
    if (m_runInScheduler) {
        // 和调度器的主协程进行切换
        swapcontext(&(Scheduler::GetMainDoroutine()->m_ctx), &m_ctx);
    } else {
        // 和当前线程的主协程进行切换
        swapcontext(&(st_threadMainDoroutine->m_ctx), &m_ctx);
    }
}

void Doroutine::yield() {
    SetThis(st_threadMainDoroutine);
    if (m_state != TERM) { // 协程在工作函数终止后也会yield一次用于回到主协程
        m_state = READY;
    }
    if (m_runInScheduler) {
        // 和调度器的主协程进行切换
        swapcontext(&m_ctx, &(Scheduler::GetMainDoroutine()->m_ctx));
    } else {
        // 和当前线程的主协程进行切换
        swapcontext(&m_ctx, &(st_threadMainDoroutine->m_ctx));
    }
}

void Doroutine::reset(std::function<void()> func) {
    if (!m_stack) {
        SYLAR_LOG_DEBUG(g_logger) << "m_stack is nullptr, cant reset";
    }

    if (m_state != TERM) {
        SYLAR_LOG_DEBUG(g_logger) << "m_state is not TERM, cant reset";
    }

    m_func = func;

    getcontext(&m_ctx);

    m_ctx.uc_link          = nullptr;
    m_ctx.uc_stack.ss_sp   = m_stack;
    m_ctx.uc_stack.ss_size = m_stackSize;

    makecontext(&m_ctx, &WorkFunc, 0);
    m_state = READY;
}

void Doroutine::SetThis(ptr curDoroutine) {
    st_threadCurdoroutine = curDoroutine;
}

Doroutine::ptr Doroutine::GetThis() {
    return st_threadCurdoroutine;
}

Doroutine::ptr Doroutine::GetMainThis()
{
    return st_threadMainDoroutine;
}

uint64_t Doroutine::GetThisId() {
    if (st_threadCurdoroutine) {
        return st_threadCurdoroutine->getId();
    }
    return 0;
}

void Doroutine::WorkFunc() {
    ptr curDoroutine = GetThis();

    curDoroutine->m_func();
    curDoroutine->m_func = nullptr;
    curDoroutine->m_state = TERM;

    auto rawPtr = curDoroutine.get();
    curDoroutine.reset();
    rawPtr->yield();
}

void Doroutine::threadMainDoroutineInit() {
    ptr mainDoroutine = std::make_shared<Doroutine>();
    SetThis(mainDoroutine);
    st_threadMainDoroutine = mainDoroutine;
}

};