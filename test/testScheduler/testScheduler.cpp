#include <iostream>
#include <chrono>
#include <thread>

#include "scheduler.h"
#include "util.h"
#include "hook.h"
#include "forTest.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void testDoroutine1() {
    SYLAR_LOG_INFO(g_logger) << KSC::Doroutine::GetThis()->getId() << " create!";
    SYLAR_LOG_INFO(g_logger) << "test_Doroutine1 begin";
    KSC::Scheduler::GetThis()->schedule(KSC::Doroutine::GetThis());
    
    SYLAR_LOG_INFO(g_logger) << "before testDoroutine1 yield";
    KSC::Doroutine::GetThis()->yield();
    SYLAR_LOG_INFO(g_logger) << "after testDoroutine1 yield";

    SYLAR_LOG_INFO(g_logger) << "testDoroutine1 end";
}

/**
 * @brief 演示协程睡眠对主程序的影响
 */
void testDoroutine2() {
    SYLAR_LOG_INFO(g_logger) << KSC::Doroutine::GetThis()->getId() << " create!";
    SYLAR_LOG_INFO(g_logger) << "testDoroutine2 begin";

    /**
     * 一个线程同一时间只能有一个协程在运行，线程调度协程的本质就是按顺序执行任务队列里的协程
     * 由于必须等一个协程执行完后才能执行下一个协程，所以任何一个协程的阻塞都会影响整个线程的协程调度，这里
     * 睡眠的3秒钟之内调度器不会调度新的协程，对sleep函数进行hook之后可以改变这种情况
     */

    // 一定要先把hook取消掉，不然std::this_thread::sleep_for会调用到hook后的nanosleep接口，但是我们又没有创建iomanager实例，导致出错
    KSC::setHookEnable(false); 
    std::this_thread::sleep_for(std::chrono::seconds(3));
    SYLAR_LOG_INFO(g_logger) << "testDoroutine2 end";
}

void testDoroutine3() {
    SYLAR_LOG_INFO(g_logger) << KSC::Doroutine::GetThis()->getId() << " create!";
    SYLAR_LOG_INFO(g_logger) << "testDoroutine3 begin";
    SYLAR_LOG_INFO(g_logger) << "testDoroutine3 end";
}

void testDoroutine5() {
    static int count = 0;

    SYLAR_LOG_INFO(g_logger) << KSC::Doroutine::GetThis()->getId() << " create!";
    SYLAR_LOG_INFO(g_logger) << "testDoroutine5 begin, i = " << count;
    SYLAR_LOG_INFO(g_logger) << "testDoroutine5 end i = " << count;

    count++;
}

/**
 * @brief 演示指定执行线程的情况
 */
void testDoroutine4() {
    SYLAR_LOG_INFO(g_logger) << KSC::Doroutine::GetThis()->getId() << " create!";
    SYLAR_LOG_INFO(g_logger) << "testDoroutine4 begin";
    
    for (int i = 0; i < 3; i++) {
        KSC::Scheduler::GetThis()->schedule(testDoroutine5, KSC::GetThreadId());
    }

    SYLAR_LOG_INFO(g_logger) << "testDoroutine4 end";
}

int main() {
    // KSC::forTest();
    SYLAR_LOG_INFO(g_logger) << "main begin";

    /** 
     * 只使用main函数线程进行协程调度，相当于先攒下一波协程，然后切换到调度器的run方法将这些协程
     * 消耗掉，然后再返回main函数往下执行
     */
    KSC::Scheduler sc; 

    // 额外创建新的线程进行调度，那只要添加了调度任务，调度器马上就可以调度该任务
    // KSC::Scheduler sc(3, false);

    // 添加调度任务，使用函数作为调度对象
    sc.schedule(testDoroutine1);
    sc.schedule(testDoroutine2);

    // 添加调度任务，使用Doroutine类作为调度对象
    KSC::Doroutine::ptr doroutine3 = std::make_shared<KSC::Doroutine>(&testDoroutine3);
    sc.schedule(doroutine3);

    // 创建调度线程，开始任务调度，如果只使用main函数线程进行调度，那start相当于什么也没做
    sc.start();

    /**
     * 只要调度器未停止，就可以添加调度任务
     * 包括在子协程中也可以通过KSC::Scheduler::GetThis()->scheduler()的方式继续添加调度任务
     */
    sc.schedule(testDoroutine4);

    /**
     * 停止调度，如果未使用当前线程进行调度，那么只需要简单地等所有调度线程退出即可
     * 如果使用了当前线程进行调度，那么要先执行当前线程的协程调度函数，等其执行完后再返回caller协程继续往下执行
     */
    sc.stop();
    SYLAR_LOG_INFO(g_logger) << "main end";
    return 0;
}