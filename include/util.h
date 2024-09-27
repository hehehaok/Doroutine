#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <string>

namespace KSC {

pid_t GetThreadId();

std::string GetThreadName();

void SetThreadName(const std::string &name);

uint64_t GetDoroutineId();

uint64_t GetElapsedMS();

};


#endif