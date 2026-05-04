#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdarg.h>
#include <zlog.h>

int rm_log_init(const char *config_path);
void rm_log_fini(void);
void rm_log_write(int level, const char *file, const char *func, long line, const char *fmt, ...);

#define LOG_INFO(...) rm_log_write(ZLOG_LEVEL_INFO, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) rm_log_write(ZLOG_LEVEL_WARN, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) rm_log_write(ZLOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) rm_log_write(ZLOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)

#endif // UTIL_LOG_H
