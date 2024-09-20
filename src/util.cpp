#include <sys/syscall.h>
#include <unistd.h>

#include "util.h"

namespace KSC {

pid_t GetThreadId(){
    return syscall(SYS_gettid);
}




};