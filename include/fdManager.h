#ifndef FDMANAGER_H
#define FDMANAGER_H

#include <memory>
#include <vector>
#include <thread>
#include <shared_mutex>
#include <mutex>

namespace KSC {

class FdCtx : public std::enable_shared_from_this<FdCtx> {
public:
    using ptr = std::shared_ptr<FdCtx>;
    FdCtx(int fd);
    ~FdCtx();

    bool isInit() const { return m_isInit; }
    bool isSocket() const { return m_isSocket; }
    bool isClose() const { return m_isClosed; }

    void setUserNonblock(bool v) { m_userNoBlock = v; }
    bool getUserNonblock() const { return m_userNoBlock; }
    void setSysNonblock(bool v) { m_sysNoBlock = v; }
    bool getSysNonblock() const { return m_sysNoBlock; }
    void setTimeout(int type, uint64_t v);
    uint64_t getTimeout(int type);

private:
    bool init();

private:
    bool m_isInit = false;        // 是否初始化
    bool m_isSocket = false;      // 是否socket
    bool m_sysNoBlock = false;  // 是否hook非阻塞
    bool m_userNoBlock = false; // 是否用户主动设置非阻塞
    bool m_isClosed = false;      // 是否关闭
    int m_fd;                    // 文件标识符
    uint64_t m_recvTimeout = -1;      // 读事件超时时间ms
    uint64_t m_sendTimeout = -1;      // 写事件超时事件ms
};

class FdManager {
public:
    using readMtx = std::shared_lock<std::shared_mutex>;
    using writeMtx = std::unique_lock<std::shared_mutex>;

    FdCtx::ptr get(int fd, bool autoCreate = false);
    void delFd(int fd);
    static FdManager* GetInstance();

private:
    FdManager();
    FdManager(const FdManager &other) = delete;
    FdManager &operator=(const FdManager &other) = delete;
    FdManager(const FdManager &&other) = delete;
    FdManager &operator=(const FdManager &&other) = delete;

private:
    std::shared_mutex m_rwMtx;
    std::vector<FdCtx::ptr> m_fds;
};


};

#endif