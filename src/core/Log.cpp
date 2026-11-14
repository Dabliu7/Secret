#include "mtasa/Log.h"

#include <cstdio>
#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace mtasa {

void LogV(LogLevel level, const char* fmt, va_list args) {
#ifdef __ANDROID__
    static const int kPrio[] = {ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR};
    __android_log_vprint(kPrio[static_cast<int>(level)], "MTASA", fmt, args);
#else
    static const char* kTag[] = {"D", "I", "W", "E"};
    std::fprintf(stderr, "[MTASA/%s] ", kTag[static_cast<int>(level)]);
    std::vfprintf(stderr, fmt, args);
    std::fputc('\n', stderr);
#endif
}

void Log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    LogV(level, fmt, args);
    va_end(args);
}

}  // namespace mtasa
