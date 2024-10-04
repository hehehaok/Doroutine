#include "log.h"

namespace KSC {

void setLogLevelDebug() {
    sylar::Logger::ptr rootLogger = SYLAR_LOG_ROOT();
    sylar::LogAppender::ptr writeAppender1(new sylar::FileLogAppender("./log.txt"));
    rootLogger->addAppender(writeAppender1);
    rootLogger->setLevel(sylar::LogLevel::DEBUG);
}

void setLogDisable() {
    sylar::Logger::ptr rootLogger = SYLAR_LOG_ROOT();
    rootLogger->setLevel(sylar::LogLevel::FATAL);
}

};



