#include <iostream>
#include "doroutine.h"
#include "log.h"
#include "forTest.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_doroutine1() {
    SYLAR_LOG_INFO(g_logger) << "yield before:3";
    SYLAR_LOG_INFO(g_logger) << "yield before:2";
    SYLAR_LOG_INFO(g_logger) << "yield before:1";
    KSC::Doroutine::GetThis()->yield();
    SYLAR_LOG_INFO(g_logger) << "yield before:1";
    SYLAR_LOG_INFO(g_logger) << "yield before:2";
    SYLAR_LOG_INFO(g_logger) << "yield before:3";
}

void test_doroutine2() {
    SYLAR_LOG_INFO(g_logger) << "yield before:c";
    SYLAR_LOG_INFO(g_logger) << "yield before:b";
    SYLAR_LOG_INFO(g_logger) << "yield before:a";
    KSC::Doroutine::GetThis()->yield();
    SYLAR_LOG_INFO(g_logger) << "yield before:a";
    SYLAR_LOG_INFO(g_logger) << "yield before:b";
    SYLAR_LOG_INFO(g_logger) << "yield before:c";
}

void allocateAndPrint(size_t stackSize) {
    void *m_stack = nullptr;

    // First allocation.
    m_stack = KSC::StackAllocator::Alloc(stackSize);
    SYLAR_LOG_INFO(g_logger) << "First allocation address: " << m_stack;

    // Free the first allocation before attempting to allocate again.
    KSC::StackAllocator::Dealloc(m_stack);

    // Second allocation.
    m_stack = KSC::StackAllocator::Alloc(stackSize);
    SYLAR_LOG_INFO(g_logger) << "Second allocation address: " << m_stack;

    // Clean up.
    KSC::StackAllocator::Dealloc(m_stack);
}

int main() {
    // KSC::forTest();
    g_logger->setLevel(sylar::LogLevel::DEBUG);
    sylar::LogAppender::ptr writeAppender(new sylar::FileLogAppender("./log.txt"));
    g_logger->addAppender(writeAppender);
    KSC::Doroutine::threadMainDoroutineInit();
    KSC::Doroutine::ptr doroutine1 = std::make_shared<KSC::Doroutine>(test_doroutine1, 0, false);
    KSC::Doroutine::ptr doroutine2 = std::make_shared<KSC::Doroutine>(test_doroutine2, 0, false);
    SYLAR_LOG_INFO(g_logger) << "doroutine1 ptr count is " << doroutine1.use_count();
    SYLAR_LOG_INFO(g_logger) << "doroutine2 ptr count is " << doroutine2.use_count();

    doroutine1->resume();
    doroutine2->resume();
    SYLAR_LOG_INFO(g_logger) << "doroutine1 ptr count is " << doroutine1.use_count();
    SYLAR_LOG_INFO(g_logger) << "doroutine2 ptr count is " << doroutine2.use_count();

    doroutine1->resume();
    doroutine2->resume();
    SYLAR_LOG_INFO(g_logger) << "doroutine1 ptr count is " << doroutine1.use_count();
    SYLAR_LOG_INFO(g_logger) << "doroutine2 ptr count is " << doroutine2.use_count();

    SYLAR_LOG_INFO(g_logger) << "test end";
    return 0;
}