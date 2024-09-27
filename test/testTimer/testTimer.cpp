#include <iostream>

#include "iomanager.h"
#include "forTest.h"

static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

static int timeOutMs = 1000;
static KSC::Timer::ptr s_timer;

void timerCallback() {
    SYLAR_LOG_INFO(g_logger) << "timerCallback:" << timeOutMs;
    timeOutMs += 1000;
    if(timeOutMs < 5000) {
        s_timer->reset(timeOutMs, true);
    } else {
        s_timer->cancel();
    }
}

void testTimer() {
    KSC::IOManager iom;
    s_timer = iom.addTimer(1000, timerCallback, true);

    iom.addTimer(500, []{
        SYLAR_LOG_INFO(g_logger) << "timer:500";
    });

    iom.addTimer(5000, []{
        SYLAR_LOG_INFO(g_logger) << "timer:5000";
    });
}

int main() {
    KSC::forTest();
    testTimer();
    return 0;
}