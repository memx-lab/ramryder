#include "util_log.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <zlog.h>

static zlog_category_t *g_log_cat = NULL;
static bool g_log_inited = false;

static const char *rm_log_level_name(int level)
{
    switch (level) {
    case ZLOG_LEVEL_DEBUG:
        return "DEBUG";
    case ZLOG_LEVEL_INFO:
        return "INFO";
    case ZLOG_LEVEL_WARN:
        return "WARN";
    case ZLOG_LEVEL_ERROR:
        return "ERROR";
    default:
        return "LOG";
    }
}

static void rm_log_vfallback(int level, const char *file, const char *func, long line,
                const char *fmt, va_list ap)
{
    fprintf(stderr, "[%s] [%s:%ld %s] ", rm_log_level_name(level), file, line, func);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

int rm_log_init(const char *config_path)
{
    int rc;

    if (g_log_inited) {
        return 0;
    }

    rc = zlog_init(config_path);
    if (rc != 0) {
        fprintf(stderr, "Failed to init zlog with config %s\n", config_path);
        return -1;
    }

    g_log_cat = zlog_get_category("resource_manager");
    if (!g_log_cat) {
        zlog_fini();
        fprintf(stderr, "Failed to get zlog category resource_manager\n");
        return -1;
    }

    g_log_inited = true;
    return 0;
}

void rm_log_fini(void)
{
    if (!g_log_inited) {
        return;
    }

    zlog_fini();
    g_log_cat = NULL;
    g_log_inited = false;
}

void rm_log_write(int level, const char *file, const char *func, long line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (g_log_cat) {
        vzlog(g_log_cat, file, strlen(file), func, strlen(func), line, level, fmt, ap);
    } else {
        rm_log_vfallback(level, file, func, line, fmt, ap);
    }
    va_end(ap);
}
