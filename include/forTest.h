#include "log.h"

namespace KSC {

void forTest() {
    sylar::Logger::ptr rootLogger = SYLAR_LOG_ROOT();
    sylar::LogAppender::ptr writeAppender1(new sylar::FileLogAppender("./log.txt"));
    rootLogger->addAppender(writeAppender1);
    rootLogger->setLevel(sylar::LogLevel::DEBUG);
}

};



