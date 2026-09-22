#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// 远程日志出口：由 MqttLink 注册，参数为级别与整行日志文本（不含级别前缀）
typedef void (*LogSinkFn)(const char* level, const char* line);

void Logger_setSink(LogSinkFn sink);
void Logger_log(const char* level, const char* fmt, ...);

#define LOG_D(fmt, ...) Logger_log("DEBUG", fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) Logger_log("INFO", fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) Logger_log("WARN", fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) Logger_log("ERROR", fmt, ##__VA_ARGS__)

#endif // LOGGER_H
