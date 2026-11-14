#pragma once
#include <cstdarg>

namespace mtasa {
enum class LogLevel { Debug, Info, Warn, Error };
void LogV(LogLevel level, const char* fmt, va_list args);
void Log(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
}  // namespace mtasa

#define LOGD(...) ::mtasa::Log(::mtasa::LogLevel::Debug, __VA_ARGS__)
#define LOGI(...) ::mtasa::Log(::mtasa::LogLevel::Info, __VA_ARGS__)
#define LOGW(...) ::mtasa::Log(::mtasa::LogLevel::Warn, __VA_ARGS__)
#define LOGE(...) ::mtasa::Log(::mtasa::LogLevel::Error, __VA_ARGS__)
