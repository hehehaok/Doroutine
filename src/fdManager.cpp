#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fdManager.h"
#include "hook.h"



namespace KSC {

FdCtx::FdCtx(int fd) : m_fd(fd) {
    init();
}

FdCtx::~FdCtx() {

}

void FdCtx::setTimeout(int type, uint64_t v) {
    if (type == SO_RCVTIMEO) {
        m_recvTimeout = v;
    } else {
        m_sendTimeout = v;
    }
}

uint64_t FdCtx::getTimeout(int type) {
    if (type == SO_RCVTIMEO) {
        return m_recvTimeout;
    } else {
        return m_sendTimeout;
    }
}

bool FdCtx::init() {
    if (m_isInit) {
        return true;
    }

    struct stat fdStat;
    if(-1 == fstat(m_fd, &fdStat)) {
        m_isInit = false;
        m_isSocket = false;
    } else {
        m_isInit = true;
        m_isSocket = S_ISSOCK(fdStat.st_mode);
    }

    if(m_isSocket) {
        int flags = fcntl_f(m_fd, F_GETFL, 0);
        if(!(flags & O_NONBLOCK)) {
            fcntl_f(m_fd, F_SETFL, flags | O_NONBLOCK);
        }
        m_sysNoBlock = true;
    } else {
        m_sysNoBlock = false;
    }

    return m_isInit;
}

FdCtx::ptr FdManager::get(int fd, bool autoCreate) {
    if (fd == -1) {
        return nullptr;
    }

    {
        readMtx lck(m_rwMtx);
        if ((int)m_fds.size() <= fd && !autoCreate) {
            return nullptr;
        }

        if ((int)m_fds.size() > fd && (m_fds[fd] || !autoCreate)) {
            return m_fds[fd];
        }
    }

    writeMtx lck(m_rwMtx);
    FdCtx::ptr ctx = std::make_shared<FdCtx>(fd);
    if (fd >= m_fds.size()) {
        m_fds.resize(fd * 1.5);
    }
    m_fds[fd] = ctx;
    return ctx;
}

void FdManager::delFd(int fd) {
    writeMtx lck(m_rwMtx);
    if (fd >= m_fds.size()) {
        return;
    }
    m_fds[fd].reset();
}

FdManager *FdManager::GetInstance() {
    static FdManager instance;
    return &instance;
}

FdManager::FdManager() {
    m_fds.resize(64);
}


};