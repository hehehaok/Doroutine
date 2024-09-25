#include <iostream>
#include <chrono>

#include "scheduler.h"
#include "util.h"

void testDoroutine1() {
    std::cout << KSC::Doroutine::GetThis()->getId() << " create!" << std::endl;
    std::cout << "test_Doroutine1 begin" << std::endl;
    KSC::Scheduler::GetThis()->schedule(KSC::Doroutine::GetThis());
    
    std::cout << "before testDoroutine1 yield" << std::endl;
    KSC::Doroutine::GetThis()->yield();
    std::cout << "after testDoroutine1 yield" << std::endl;

    std::cout << "testDoroutine1 end" << std::endl;
}

/**
 * @brief 演示协程睡眠对主程序的影响
 */
void testDoroutine2() {
    std::cout << KSC::Doroutine::GetThis()->getId() << " create!" << std::endl;
    std::cout << "testDoroutine2 begin" << std::endl;

    /**
     * 一个线程同一时间只能有一个协程在运行，线程调度协程的本质就是按顺序执行任务队列里的协程
     * 由于必须等一个协程执行完后才能执行下一个协程，所以任何一个协程的阻塞都会影响整个线程的协程调度，这里
     * 睡眠的3秒钟之内调度器不会调度新的协程，对sleep函数进行hook之后可以改变这种情况
     */
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "testDoroutine2 end" << std::endl;
}

void testDoroutine3() {
    std::cout << KSC::Doroutine::GetThis()->getId() << " create!" << std::endl;
    std::cout << "testDoroutine3 begin" << std::endl;
    std::cout << "testDoroutine3 end" << std::endl;
}

void testDoroutine5() {
    static int count = 0;

    std::cout << KSC::Doroutine::GetThis()->getId() << " create!" << std::endl;
    std::cout << "testDoroutine5 begin, i = " << count << std::endl;
    std::cout << "testDoroutine5 end i = " << count << std::endl;

    count++;
}

/**
 * @brief 演示指定执行线程的情况
 */
void testDoroutine4() {
    std::cout << KSC::Doroutine::GetThis()->getId() << " create!" << std::endl;
    std::cout << "testDoroutine4 begin" << std::endl;
    
    for (int i = 0; i < 3; i++) {
        KSC::Scheduler::GetThis()->schedule(testDoroutine5, KSC::GetThreadId());
    }

    std::cout << "testDoroutine4 end" << std::endl;
}

int main() {
    std::cout << "main begin" << std::endl;

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
    std::cout << "main end" << std::endl;
    return 0;
}