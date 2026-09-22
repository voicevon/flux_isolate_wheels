#include "Logger.h"

// 串口本地输出 + 可选远程出口（MQTT），所有 LOG_x 宏统一走此函数
static LogSinkFn s_logSink = NULL;

void Logger_setSink(LogSinkFn sink) {
  s_logSink = sink;
}

void Logger_log(const char* level, const char* fmt, ...) {
  static char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Serial.printf("[%s] %s\n", level, buf);

  if (s_logSink) {
    s_logSink(level, buf);
  }
}
