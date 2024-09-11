#include <atomic>
#include <iostream>
#include <thread>

#include "doroutine.h"

namespace KSC {

static std::atomic<uint64_t> s_doroutineId {0};
static std::atomic<uint64_t> s_doroutineCount {0};

static thread_local Doroutine::ptr s_threadCurdoroutine = nullptr;
static thread_local Doroutine::ptr s_threadMainDoroutine = nullptr;

Doroutine::Doroutine() {
    m_state = RUNNING;

    if (getcontext(&m_ctx)) {
        std::cout << "getcontext wrong!" << std::endl;
    }

    ++s_doroutineCount;
    m_id = s_doroutineId++;
    std::cout << "thread " << std::this_thread::get_id() << "'s main doroutine starts!" << std::endl;
}

Doroutine::Doroutine(std::function<void()> func, size_t stackSize, bool runInScheduler)
    : m_id(s_doroutineId++)
    , m_func(func)
    , m_runInScheduler(runInScheduler) {
    
    m_stackSize = stackSize ? stackSize : 1024 * 128;
    m_stack = StackAllocator::Alloc(m_stackSize);
    std::cout << "m_stack is " << m_stack << std::endl;

    if (getcontext(&m_ctx)) {
        std::cout << "getcontext wrong!" << std::endl;
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
    std::cout << "doroutine " << m_id << "'s ~Doroutine done!" << std::endl;
}

void Doroutine::resume() {
    SetThis(shared_from_this()); // 切换到当前协程
    m_state = RUNNING;
    if (m_runInScheduler) {
        // 和调度器的主协程进行切换
    } else {
        // 和当前线程的主协程进行切换
        swapcontext(&(s_threadMainDoroutine->m_ctx), &m_ctx);
    }
}

void Doroutine::yield() {
    SetThis(s_threadMainDoroutine);
    if (m_state != TERM) { // 协程在工作函数终止后也会yield一次用于回到主协程
        m_state = READY;
    }
    if (m_runInScheduler) {
        // 和调度器的主协程进行切换
    } else {
        // 和当前线程的主协程进行切换
        swapcontext(&m_ctx, &(s_threadMainDoroutine->m_ctx));
    }
}

void Doroutine::reset(std::function<void()> func) {
    if (!m_stack) {
        std::cout << "m_stack is nullptr, cant reset" << std::endl;
    }

    if (m_state != TERM) {
        std::cout << "m_state is not TERM, cant reset" << std::endl;
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
    s_threadCurdoroutine = curDoroutine;
}

Doroutine::ptr Doroutine::GetThis() {
    return s_threadCurdoroutine;
}

uint64_t Doroutine::GetThisId() {
    return s_threadCurdoroutine->getId();
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
    s_threadMainDoroutine = mainDoroutine;
}

};