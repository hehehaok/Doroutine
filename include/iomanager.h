#ifndef IOMANAGER_H
#define IOMANAGER_H

#include "scheduler.h"
#include "timer.h"

namespace KSC {

class IOManager : public Scheduler, public TimerManager {

public:
    using ptr = std::shared_ptr<IOManager>;
    using readMtx = std::shared_lock<std::shared_mutex>;
    using writeMtx = std::unique_lock<std::shared_mutex>;

    enum Event {
        NONE = 0X0,
        READ = 0x1,
        WRITE = 0x4,
    };

private:
    struct FdContext {
        struct EventContext {
            Scheduler *scheduler = nullptr;
            Doroutine::ptr doroutine = nullptr;
            std::function<void()> func = nullptr;
        };

        EventContext &getEventContext(Event event); // 根据触发的事件取得对应的上下文
        void resetEventContext(EventContext &ctx);  // 重置上下文
        void triggerEvent(Event event); // 根据触发的事件触发对应的回调

        EventContext read;
        EventContext write;
        int fd = 0;
        Event events = NONE;
        std::mutex mtx;
    };

public:
    IOManager(size_t threads = 1, bool userCaller = true, const std::string &name = "IOManager");
    ~IOManager();

    int addEvent(int fd, Event event, std::function<void()> func = nullptr); // 给特定标识符添加某一事件
    bool delEvent(int fd, Event event); // 删除特定标识符的指定事件
    bool cancelEvent(int fd, Event event); // 删除特定标识符的指定事件，但会在删除前触发一次回调
    bool cancelAll(int fd); // 删除特定标识符的所有事件

    static IOManager *GetThis();

protected:
    void tickle() override;
    bool stopping() override;
    void idle() override;
    void onTimerInsertedAtFront() override;

    bool stopping(uint64_t &timeout);
    void contextResize(size_t size);

private:
    int m_epfd = 0;
    int m_tickleFds[2];
    std::atomic<size_t> m_pendingEventCount = {0};
    std::shared_mutex m_rwmtx;
    std::vector<FdContext *> m_fdContexts;
};

};

#endif