#include <iostream>

#include "iomanager.h"

static int timeOutMs = 1000;
static KSC::Timer::ptr s_timer;

void timerCallback() {
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
        std::cout << "timer:500" << std::endl;
    });

    iom.addTimer(5000, []{
        std::cout << "timer:5000" << std::endl;
    });
}

int main() {
    testTimer();
    return 0;
}