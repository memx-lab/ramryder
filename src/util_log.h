#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdarg.h>

int rm_log_init(const char *config_path);
void rm_log_fini(void);
void rm_log_info(const char *fmt, ...);
void rm_log_warn(const char *fmt, ...);
void rm_log_error(const char *fmt, ...);
void rm_log_debug(const char *fmt, ...);

#define LOG_INFO(...) rm_log_info(__VA_ARGS__)
#define LOG_WARN(...) rm_log_warn(__VA_ARGS__)
#define LOG_ERROR(...) rm_log_error(__VA_ARGS__)
#define LOG_DEBUG(...) rm_log_debug(__VA_ARGS__)

#endif // UTIL_LOG_H
